// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/config.h"

#include <algorithm>

#include <regex>
#include <thread>

#include "scheduler.h"
#include "string.h"
#include "timer.h"
#include "util.h"

#ifdef WINDOWS
#include "iomanager.h"
#else
#ifndef OSX
extern char **environ;
#endif
#endif

#ifdef OSX
#include <crt_externs.h>
#endif

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:config");

bool
isValidConfigVarName(const std::string &name, bool allowDot)
{
    static const std::regex regname("[a-z][a-z0-9]*");
    static const std::regex regnameDot("[a-z][a-z0-9]*(\\.[a-z0-9]+)*");
    if (allowDot)
        return std::regex_match(name, regnameDot);
    else
        return std::regex_match(name, regname);
}

bool Config::s_locked = false;

void
Config::loadFromCommandLine(int &argc, char *argv[])
{
    char **end = argv + argc;
    char **arg = argv;
    // Skip argv[0] (presumably program name)
    ++arg;
    while (arg < end) {
        // Only look at arguments that begin with --
        if (strncmp(*arg, "--", 2u) != 0) {
            ++arg;
            continue;
        }
        // Don't process arguments after --
        if (strcmp(*arg, "--") == 0)
            break;
        char *equals = strchr(*arg, '=');
        char *val;
        // Support either --arg=value or --arg value
        if (equals) {
            *equals = '\0';
            val = equals + 1;
        } else {
            val = *(arg + 1);
        }

        ConfigVarBase::ptr var = lookup(*arg + 2);
        if (var) {
            // Don't use val == *end, we don't want to actually dereference end
            if (val == *(arg + 1) && arg + 1 == end)
                MORDOR_THROW_EXCEPTION(std::invalid_argument(*arg + 2));
            if (!var->fromString(val))
                MORDOR_THROW_EXCEPTION(std::invalid_argument(*arg + 2));
            // Adjust argv to remove this arg (and its param, if it was a
            // separate arg)
            int toSkip = 1;
            if (val != equals + 1)
                ++toSkip;
            memmove(arg, arg + toSkip, (end - arg - toSkip) * sizeof(char *));
            argc -= toSkip;
            end -= toSkip;
        } else {
            // --arg=value wasn't a ConfigVar, restore the equals
            if (equals)
                *equals = '=';
            ++arg;
        }
    }
}

void
Config::loadFromEnvironment()
{
#ifdef WINDOWS
    wchar_t *enviro = GetEnvironmentStringsW();
    if (!enviro)
        return;
    std::shared_ptr<wchar_t> environScope(enviro, &FreeEnvironmentStringsW);
    for (const wchar_t *env = enviro; *env; env += wcslen(env) + 1) {
        const wchar_t *equals = wcschr(env, '=');
        if (!equals)
            continue;
        if (equals == env)
            continue;
        std::string key(toUtf8(env, equals - env));
        std::string value(toUtf8(equals + 1));
#else
#ifdef OSX
	char **environ = *_NSGetEnviron();
#endif
    if (!environ)
        return;
    for (const char *env = *environ; *env; env += strlen(env) + 1) {
        const char *equals = strchr(env, '=');
        if (!equals)
            continue;
        if (equals == env)
            continue;
        std::string key(env, equals - env);
        std::string value(equals + 1);
#endif
        std::transform(key.begin(), key.end(), key.begin(), tolower);
        replace(key, '_', '.');
        if (!isValidConfigVarName(key))
            continue;
        ConfigVarBase::ptr var = lookup(key);
        if (var)
            var->fromString(value);
    }
}

ConfigVarBase::ptr
Config::lookup(const std::string &name)
{
    ConfigVarSet::iterator it = vars().find(name);
    if (it != vars().end())
        return it->second;
    return ConfigVarBase::ptr();
}

void
Config::visit(std::function<void (ConfigVarBase::ptr)> dg)
{
    for (ConfigVarSet::const_iterator it = vars().begin();
        it != vars().end();
        ++it) {
        dg(it->second);
    }
}

#ifdef WINDOWS
static void loadFromRegistry(HKEY hKey)
{
    std::string buffer;
    std::wstring valueName;
    DWORD type;
    DWORD index = 0;
    DWORD valueNameSize, size;
    LSTATUS status = RegQueryInfoKeyW(hKey, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        &valueNameSize, &size, NULL, NULL);
    if (status)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(status, "RegQueryInfoKeyW");
    valueName.resize(std::max<DWORD>(valueNameSize + 1, 1u));
    buffer.resize(std::max<DWORD>(size, 1u));
    while (true) {
        valueNameSize = (DWORD)valueName.size();
        size = (DWORD)buffer.size();
        status = RegEnumValueW(hKey, index++, &valueName[0], &valueNameSize,
            NULL, &type, (LPBYTE)&buffer[0], &size);
        if (status == ERROR_NO_MORE_ITEMS)
            break;
        if (status == ERROR_MORE_DATA)
            continue;
        if (status)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(status, "RegEnumValueW");
        switch (type) {
            case REG_DWORD:
                if (size != 4)
                    continue;
                break;
            case REG_QWORD:
                if (size != 8)
                    continue;
                break;
            case REG_EXPAND_SZ:
            case REG_SZ:
                break;
            default:
                continue;
        }
        std::string varName = toUtf8(valueName.c_str(), valueNameSize);
        ConfigVarBase::ptr var = Config::lookup(varName);
        if (var) {
            std::string data;
            switch (type) {
                case REG_DWORD:
                    data = std::to_string(*(DWORD *)&buffer[0]);
                    break;
                case REG_QWORD:
                    data = to_string(*(long long *)&buffer[0]);
                    break;
                case REG_EXPAND_SZ:
                case REG_SZ:
                    if (((wchar_t *)&buffer[0])[size / sizeof(wchar_t) - 1] ==
                        L'\0')
                        size -= sizeof(wchar_t);
                    data = toUtf8((wchar_t *)&buffer[0],
                        size / sizeof(wchar_t));
                    break;
            }
            var->fromString(data);
        }
    }
}

Config::RegistryMonitor::RegistryMonitor(IOManager &ioManager,
    HKEY hKey, const std::wstring &subKey)
    : m_ioManager(ioManager),
      m_hKey(NULL),
      m_hEvent(NULL)
{
    LSTATUS status = RegOpenKeyExW(hKey, subKey.c_str(), 0,
        KEY_QUERY_VALUE | KEY_NOTIFY, &m_hKey);
    if (status)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(status, "RegOpenKeyExW");
    try {
        m_hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!m_hEvent)
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
        status = RegNotifyChangeKeyValue(m_hKey, FALSE,
            REG_NOTIFY_CHANGE_LAST_SET, m_hEvent, TRUE);
        if (status)
            MORDOR_THROW_EXCEPTION_FROM_ERROR_API(status,
                "RegNotifyChangeKeyValue");
    } catch (...) {
        if (m_hKey)
            RegCloseKey(m_hKey);
        if (m_hEvent)
            CloseHandle(m_hEvent);
        throw;
    }
}

Config::RegistryMonitor::~RegistryMonitor()
{
    m_ioManager.unregisterEvent(m_hEvent);
    RegCloseKey(m_hKey);
    CloseHandle(m_hEvent);
}

void
Config::RegistryMonitor::onRegistryChange(
    std::weak_ptr<RegistryMonitor> self)
{
    try
    {
        RegistryMonitor::ptr strongSelf = self.lock();
        if (strongSelf) {
            LSTATUS status = RegNotifyChangeKeyValue(strongSelf->m_hKey, FALSE,
                REG_NOTIFY_CHANGE_LAST_SET, strongSelf->m_hEvent, TRUE);
            if (status)
                MORDOR_THROW_EXCEPTION_FROM_ERROR_API(status,
                    "RegNotifyChangeKeyValue");
            Mordor::loadFromRegistry(strongSelf->m_hKey);
        }
    }
    catch(...)
    {
        MORDOR_LOG_WARNING(g_log) << "failed to monitor registry: " <<
            boost::current_exception_diagnostic_information();
    }
}

void
Config::loadFromRegistry(HKEY hKey, const std::string &subKey)
{
    loadFromRegistry(hKey, toUtf16(subKey));
}

void
Config::loadFromRegistry(HKEY hKey, const std::wstring &subKey)
{
    HKEY localKey;
    LSTATUS status = RegOpenKeyExW(hKey, subKey.c_str(), 0, KEY_QUERY_VALUE,
        &localKey);
    if (status)
        MORDOR_THROW_EXCEPTION_FROM_ERROR_API(status, "RegOpenKeyExW");
    try {
        Mordor::loadFromRegistry(localKey);
    } catch (...) {
        RegCloseKey(localKey);
        throw;
    }
    RegCloseKey(localKey);
}

Config::RegistryMonitor::ptr
Config::monitorRegistry(IOManager &ioManager, HKEY hKey,
    const std::string &subKey)
{
    return monitorRegistry(ioManager, hKey, toUtf16(subKey));
}

Config::RegistryMonitor::ptr
Config::monitorRegistry(IOManager &ioManager, HKEY hKey,
    const std::wstring &subKey)
{
    RegistryMonitor::ptr result(new RegistryMonitor(ioManager, hKey, subKey));
    // Have to wait until after the object is constructed to get the weak_ptr
    // we need
    ioManager.registerEvent(result->m_hEvent,
        std::bind(&RegistryMonitor::onRegistryChange,
            std::weak_ptr<RegistryMonitor>(result)), true);
    Mordor::loadFromRegistry(result->m_hKey);
    return result;
}
#endif

static bool verifyThreadCount(int value)
{
    return value != 0;
}

static void updateThreadCount(int value, Scheduler &scheduler)
{
    if (value < 0)
        value = -value * std::thread::hardware_concurrency();
    scheduler.threadCount(value);
}

void associateSchedulerWithConfigVar(Scheduler &scheduler,
    ConfigVar<int>::ptr configVar)
{
    configVar->beforeChange.connect(&verifyThreadCount);
    configVar->onChange.connect(std::bind(&updateThreadCount, std::placeholders::_1, std::ref(scheduler)));
    updateThreadCount(configVar->val(), scheduler);
}

HijackConfigVar::HijackConfigVar(const std::string &name, const std::string &value)
    : m_var(Config::lookup(name))
{
    MORDOR_ASSERT(m_var);
    m_oldValue = m_var->toString();
    // failed to set value
    if (!m_var->fromString(value))
        m_var.reset();
}

HijackConfigVar::~HijackConfigVar()
{
    reset();
}

void
HijackConfigVar::reset()
{
    if (m_var) {
        m_var->fromString(m_oldValue);
        m_var.reset();
    }
}

}

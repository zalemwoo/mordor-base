// Copyright (c) 2009 - Mozy, Inc.

#include "log.h"

#include <iostream>

#ifndef WINDOWS
#define SYSLOG_NAMES
#include <syslog.h>
#endif

#include <regex>

#include "assert.h"
#include "fiber.h"
#include "mordor/config.h"
#include "mordor/string.h"
#include "timer.h"

namespace Mordor {

static void enableLoggers();
static void enableStdoutLogging();
static void enableFileLogging();
#ifdef WINDOWS
static void enableDebugLogging();
#else
static void enableSyslogLogging();
#endif

static ConfigVar<std::string>::ptr g_logError =
    Config::lookup("log.errormask", std::string(".*"), "Regex of loggers to enable error for.");
static ConfigVar<std::string>::ptr g_logWarn =
    Config::lookup("log.warnmask", std::string(".*"), "Regex of loggers to enable warning for.");
static ConfigVar<std::string>::ptr g_logInfo =
    Config::lookup("log.infomask", std::string(".*"), "Regex of loggers to enable info for.");
static ConfigVar<std::string>::ptr g_logVerbose =
    Config::lookup("log.verbosemask", std::string(""), "Regex of loggers to enable verbose for.");
static ConfigVar<std::string>::ptr g_logDebug =
    Config::lookup("log.debugmask", std::string(""), "Regex of loggers to enable debugging for.");
static ConfigVar<std::string>::ptr g_logTrace =
    Config::lookup("log.tracemask", std::string(""), "Regex of loggers to enable trace for.");

static ConfigVar<bool>::ptr g_logStdout =
    Config::lookup("log.stdout", false, "Log to stdout");
#ifdef WINDOWS
static ConfigVar<bool>::ptr g_logDebugWindow =
    Config::lookup("log.debug", false, "Log to Debug Window");
#else
static ConfigVar<std::string>::ptr g_logSyslogFacility =
    Config::lookup("log.syslogfacility", std::string(), "Log to syslog using facility");
#endif

static FiberLocalStorage<bool> f_logDisabled;

static unsigned long long g_start;

namespace {

static struct LogInitializer
{
    LogInitializer()
    {
        g_start = TimerManager::now();

        g_logError->monitor(&enableLoggers);
        g_logWarn->monitor(&enableLoggers);
        g_logInfo->monitor(&enableLoggers);
        g_logVerbose->monitor(&enableLoggers);
        g_logDebug->monitor(&enableLoggers);
        g_logTrace->monitor(&enableLoggers);

        g_logStdout->monitor(&enableStdoutLogging);
#ifdef WINDOWS
        g_logDebugWindow->monitor(&enableDebugLogging);
#else
        g_logSyslogFacility->monitor(&enableSyslogLogging);
#endif
    }
} g_init;

}

static inline std::time_t formatTimePoint (const std::chrono::system_clock::time_point& tp)
{
     // convert to system time:
     std::time_t t = std::chrono::system_clock::to_time_t(tp);
     return t;
}

static void enableLogger(Logger::ptr logger,
    const std::regex &errorRegex, const std::regex &warnRegex,
    const std::regex &infoRegex, const std::regex &verboseRegex,
    const std::regex &debugRegex, const std::regex &traceRegex)
{
    Log::Level level = Log::FATAL;
    if (std::regex_match(logger->name(), errorRegex))
        level = Log::ERROR;
    if (std::regex_match(logger->name(), warnRegex))
        level = Log::WARNING;
    if (std::regex_match(logger->name(), infoRegex))
        level = Log::INFO;
    if (std::regex_match(logger->name(), verboseRegex))
        level = Log::VERBOSE;
    if (std::regex_match(logger->name(), debugRegex))
        level = Log::DEBUG;
    if (std::regex_match(logger->name(), traceRegex))
        level = Log::TRACE;

    if (logger->level() != level)
        logger->level(level, false);
}

static std::regex buildLogRegex(const std::string &exp, const std::string &default_exp)
{
    try
    {
        return std::regex(exp);
    }
    catch(std::regex_error &)
    {
        return std::regex(default_exp);
    }
}

static void enableLoggers()
{
    std::regex errorRegex = buildLogRegex(g_logError->val(), ".*");
    std::regex warnRegex = buildLogRegex(g_logWarn->val(), ".*");
    std::regex infoRegex = buildLogRegex(g_logInfo->val(), ".*");
    std::regex verboseRegex = buildLogRegex(g_logVerbose->val(), "");
    std::regex debugRegex = buildLogRegex(g_logDebug->val(), "");
    std::regex traceRegex = buildLogRegex(g_logTrace->val(), "");
    Log::visit(std::bind(&enableLogger, std::placeholders::_1,
            std::cref(errorRegex), std::cref(warnRegex),
            std::cref(infoRegex), std::cref(verboseRegex),
            std::cref(debugRegex), std::cref(traceRegex)));
}

static void enableStdoutLogging()
{
    static LogSink::ptr stdoutSink;
    bool log = g_logStdout->val();
    if (stdoutSink.get() && !log) {
        Log::root()->removeSink(stdoutSink);
        stdoutSink.reset();
    } else if (!stdoutSink.get() && log) {
        stdoutSink.reset(new StdoutLogSink());
        Log::root()->addSink(stdoutSink);
    }
}

#ifdef WINDOWS
static void enableDebugLogging()
{
    static LogSink::ptr debugSink;
    bool log = g_logDebugWindow->val();
    if (debugSink.get() && !log) {
        Log::root()->removeSink(debugSink);
        debugSink.reset();
    } else if (!debugSink.get() && log) {
        debugSink.reset(new DebugLogSink());
        Log::root()->addSink(debugSink);
    }
}
#else
static void enableSyslogLogging()
{
    static LogSink::ptr syslogSink;
    int facility = SyslogLogSink::facilityFromString(
        g_logSyslogFacility->val().c_str());
    if (syslogSink.get() && facility == -1) {
        Log::root()->removeSink(syslogSink);
        syslogSink.reset();
    } else if (facility != -1) {
        if (syslogSink.get()) {
            if (static_cast<SyslogLogSink*>(syslogSink.get())->facility() ==
                facility)
                return;
            Log::root()->removeSink(syslogSink);
            syslogSink.reset();
        }
        syslogSink.reset(new SyslogLogSink(facility));
        Log::root()->addSink(syslogSink);
    }
}
#endif

void
StdoutLogSink::log(const std::string &logger,
        std::chrono::system_clock::time_point now, unsigned long long elapsed,
        tid_t thread, void *fiber,
        Log::Level level, const std::string &str,
        const char *file, int line)
{
    std::ostringstream os;
    os << Mordor::formatTimePoint(now) << " " << elapsed << " " << level << " " << thread << " "
        << fiber << " " << logger << " " << file << ":" << line << " "
        << str << std::endl;
    std::cout << os.str();
    std::cout.flush();
}

#ifdef WINDOWS
void
DebugLogSink::log(const std::string &logger,
        std::chrono::system_clock::time_point now, unsigned long long elapsed,
        tid_t thread, void *fiber,
        Log::Level level, const std::string &str,
        const char *file, int line)
{
    std::wostringstream os;
    os << Mordor::formatTimePoint(now) << " " << elapsed << " " << level << " " << thread << " "
        << fiber << " " << toUtf16(logger) << " " << toUtf16(file)
        << ":" << line << " " << toUtf16(str) << std::endl;
    OutputDebugStringW(os.str().c_str());
}
#else
SyslogLogSink::SyslogLogSink(int facility)
 : m_facility(facility)
{}

void
SyslogLogSink::log(const std::string &logger,
        std::chrono::system_clock::time_point now, unsigned long long elapsed,
        tid_t thread, void *fiber,
        Log::Level level, const std::string &string,
        const char *file, int line)
{
    int syslogLevel = LOG_NOTICE;
    switch (level) {
        case Log::FATAL:
            syslogLevel = LOG_CRIT;
            break;
        case Log::ERROR:
            syslogLevel = LOG_ERR;
            break;
        case Log::WARNING:
            syslogLevel = LOG_WARNING;
            break;
        case Log::INFO:
            syslogLevel = LOG_NOTICE;
            break;
        case Log::VERBOSE:
            syslogLevel = LOG_INFO;
            break;
        default:
            syslogLevel = LOG_DEBUG;
            break;
    }
    std::ostringstream os;
    os << Mordor::formatTimePoint(now) << " " << elapsed << " " << level << " " << thread << " "
        << fiber << " " << logger << " " << file << ":" << line << " "
        << string << std::endl;
    std::string str = os.str();
    syslog(syslogLevel | m_facility, "%.*s", (int)str.size(), str.c_str());
}

int
SyslogLogSink::facilityFromString(const char *string)
{
    CODE *facilities = facilitynames;
    while (facilities->c_name && *facilities->c_name) {
        if (strcmp(facilities->c_name, string) == 0)
            return facilities->c_val;
        ++facilities;
    }
    return -1;
}

const char *
SyslogLogSink::facilityToString(int facility)
{
    CODE *facilities = facilitynames;
    while (facilities->c_name && *facilities->c_name) {
        if (facilities->c_val == facility)
            return facilities->c_name;
        ++facilities;
    }
    return NULL;
}
#endif

static void deleteNothing(Logger *l)
{}

Logger::ptr Log::root()
{
    static Logger::ptr _root(new Logger());
    return _root;
}

Logger::ptr Log::lookup(const std::string &name)
{
    Logger::ptr log = root();
    if(name.empty() || name == ":"){
        return log;
    }
    std::set<Logger::ptr, LoggerLess>::iterator it;
    Logger dummy(name, log);
    Logger::ptr dummyPtr(&dummy, &deleteNothing);
    size_t start = 0;
    std::string child_name;
    std::string node_name;
    while (start < name.size()) {
        size_t pos = name.find(':', start);
        if(pos == std::string::npos){
            child_name = name.substr(start);
            start = name.size();
        }else{
            child_name = name.substr(start, pos - start);
            start = pos + 1;
        }
        if(child_name.empty()){
            continue;
        }
        if(!node_name.empty()){
            node_name += ":";
        }
        node_name += child_name;
        dummyPtr->m_name = node_name;
        it = log->m_children.lower_bound(dummyPtr);
        if(it == log->m_children.end() || (*it)->m_name != node_name){
            Logger::ptr child(new Logger(node_name, log));
            log->m_children.insert(child);
            log = child;
        }else{
            log = *it;
        }
    }
    return log;
}

void
Log::visit(std::function<void (std::shared_ptr<Logger>)> dg)
{
    std::list<Logger::ptr> toVisit;
    toVisit.push_back(root());
    while (!toVisit.empty())
    {
        Logger::ptr cur = toVisit.front();
        toVisit.pop_front();
        dg(cur);
        for (std::set<Logger::ptr, LoggerLess>::iterator it = cur->m_children.begin();
            it != cur->m_children.end();
            ++it) {
            toVisit.push_back(*it);
        }
    }
}

LogDisabler::LogDisabler()
{
    m_disabled = !f_logDisabled;
    f_logDisabled = true;
}

LogDisabler::~LogDisabler()
{
    if (m_disabled)
        f_logDisabled = false;
}

bool
LoggerLess::operator ()(const Logger::ptr &lhs,
    const Logger::ptr &rhs) const
{
    return lhs->m_name < rhs->m_name;
}

Logger::Logger()
: m_name(":"),
  m_level(Log::INFO),
  m_inheritSinks(false)
{}

Logger::Logger(const std::string &name, Logger::ptr parent)
: m_name(name),
  m_parent(parent),
  m_level(Log::INFO),
  m_inheritSinks(true)
{}

Logger::~Logger()
{
    m_level = Log::NONE;
    clearSinks();
}

bool
Logger::enabled(Log::Level level)
{
    return level == Log::FATAL || (m_level >= level && !f_logDisabled);
}

void
Logger::level(Log::Level level, bool propagate)
{
    m_level = level;
    if (propagate) {
        for (std::set<Logger::ptr, LoggerLess>::iterator it(m_children.begin());
            it != m_children.end();
            ++it) {
            (*it)->level(level);
        }
    }
}

void
Logger::removeSink(LogSink::ptr sink)
{
    std::list<LogSink::ptr>::iterator it =
        std::find(m_sinks.begin(), m_sinks.end(), sink);
    if (it != m_sinks.end())
        m_sinks.erase(it);
}

void
Logger::log(Log::Level level, const std::string &str,
    const char *file, int line)
{
    if (str.empty() || !enabled(level))
        return;
    error_t error = lastError();
    LogDisabler disable;
    unsigned long long elapsed = TimerManager::now() - g_start;
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    Logger::ptr _this = shared_from_this();
    tid_t thread = gettid();
    void *fiber = Fiber::getThis().get();
    bool somethingLogged = false;
    while (_this) {
        for (std::list<LogSink::ptr>::iterator it(_this->m_sinks.begin());
            it != _this->m_sinks.end();
            ++it) {
            somethingLogged = true;
            (*it)->log(m_name, now, elapsed, thread, fiber, level, str, file, line);
        }
        if (!_this->m_inheritSinks)
            break;
        _this = _this->m_parent.lock();
    }
    // Restore lastError
    if (somethingLogged)
        lastError(error);
}

LogEvent::~LogEvent()
{
    m_logger->log(m_level, m_os.str(), m_file, m_line);
}

static const char *levelStrs[] = {
    "NONE",
    "FATAL",
    "ERROR",
    "WARN",
    "INFO",
    "VERBOSE",
    "DEBUG",
    "TRACE",
};

std::ostream &operator <<(std::ostream &os, Log::Level level)
{
    MORDOR_ASSERT(level >= Log::FATAL && level <= Log::TRACE);
    return os << levelStrs[level];
}

#ifdef WINDOWS
static const wchar_t *levelStrsw[] = {
    L"NONE",
    L"FATAL",
    L"ERROR",
    L"WARN",
    L"INFO",
    L"VERBOSE",
    L"DEBUG",
    L"TRACE",
};

std::wostream &operator <<(std::wostream &os, Log::Level level)
{
    MORDOR_ASSERT(level >= Log::FATAL && level <= Log::TRACE);
    return os << levelStrsw[level];
}
#endif

}

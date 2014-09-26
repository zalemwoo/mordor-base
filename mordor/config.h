#ifndef __CONFIG_H__
#define __CONFIG_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "predef.h"

#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/signals2/signal.hpp>

#include "assert.h"
#include "util.h"

namespace Mordor {

#ifdef WINDOWS
class IOManager;
#endif

namespace JSON {
class Value;
}


/*
Configuration Variables (ConfigVars) are a mechanism for configuring the
behavior of Mordor at runtime (e.g. without requiring recompilation).
It is a generic and useful mechanim, and software that uses Mordor can
define and use its own set of ConfigVars.

ConfigVars are stored in a singleton key-value table.

Typical usages of Configuration Variables include:
-Variables to adjust the level of logging (e.g. "log.debugmask")
-Variables to control the frequency of periodic timers
-Variables to enable experimental features or debugging tools

The key name of a ConfigVar can only contain lower case letters and digits
and the "." separator.  When defined using an environmental variable
upper case characters are converted to lower case, and the "_" character
can be used in place of ".".

The value of a ConfigVar has a specific type, e.g. string, integer or
double.  To access the specific value it is necessary the use the correctly
templated ConfigVar object.  e.g. ConfigVar<std::string> to read a string.
For convenience the toString() and fromString() can be used to
access the value in a general way, for example when iterating through all the
configuration variables.

A ConfigVar can only be defined once (via the templated version of
Config::lookup()), and this typically happens at global scope in a source code
file, with the result assigning to a global variable.

for example:

static ConfigVar<std::string>::ptr g_servername =
    Config::lookup<std::string>("myapp.server",
                                std::string("http://test.com"),
                                "Main Server");

Outside the source code where the ConfigVar is defined the variables can
be read or written by performing a lookup using the non-templated version of
Config::lookup()

for example:

static ConfigVarBase::ptr servername = Config::lookup("myapp.server");
std::cout << servername.toString();

To access the real value of a ConfigVar you would typically use a cast operation like this:

ConfigVar<bool>::ptr boolVar = std::dynamic_pointer_cast<ConfigVar<bool> >(Config::lookup('myapp.showui'))
if (configVarPtr) {
    bool b = boolVar->val();
    ...
}

In this case the type specified must exactly match the type used when the
ConfigVar was defined.

In addition to programmatic access it is possible to override the default
value of a ConfigVar using built in support for reading environmental
variables (Config::loadFromEnvironment()), Windows registry settings
(Config::loadFromRegistry()) etc.  These mechanisms are optional and must
be explicitly called from the code that uses Mordor.  You could also easily
extend this concept with your own code to read ConfigVars from ini files,
Apple property lists, sql databases etc.

Like any other global variable, ConfigVars should be used with some degree of
constraint and common sense.  For example they make sense for things that
are primarily adjusted only during testing, for example to point a client
to a test server rather than a default server or to increase the frequency of
a periodic operation.  But they should not be used as a replacement for clean
APIs used during the regular software flow.
*/

/// check if the name is a valid ConfigVar name
/// @param name     ConfigVar name
/// @param allowDot Whether dot is allowed in the ConfigVar name
/// @note ConfigVar name rule when allowDot value is
///   - true:  [a-z][a-z0-9]*(\.[a-z0-9]+)*
///   - false: [a-z][a-z0-9]*
/// @return if @p name is a valid ConfigVar name
bool isValidConfigVarName(const std::string &name, bool allowDot = true);

class ConfigVarBase : public Mordor::noncopyable
{
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;

public:
    ConfigVarBase(const std::string &name, const std::string &description = "",
            bool lockable = false)
        : m_name(name),
          m_description(description),
          m_lockable(lockable)
    {}
    virtual ~ConfigVarBase() {}

    std::string name() const { return m_name; }
    std::string description() const { return m_description; }
    bool isLockable() const { return m_lockable; }

    /// onChange should not throw any exceptions
    boost::signals2::signal<void ()> onChange;
    /// @deprecated (use onChange directly)
    void monitor(boost::function<void ()> dg) { onChange.connect(dg); }

    virtual std::string toString() const = 0;
    /// @return If the new value was accepted
    virtual bool fromString(const std::string &str) = 0;

private:
    std::string m_name, m_description;
    bool m_lockable;
};

template <class T>
bool isConfigNotLocked(const T &);

template <class T>
class ConfigVar : public ConfigVarBase
{
public:
    struct BreakOnFailureCombiner
    {
        typedef bool result_type;
        template <typename InputIterator>
        bool operator()(InputIterator first, InputIterator last) const
        {
            try {
                for (; first != last; ++first)
                    if (!*first) return false;
            } catch (...) {
                return false;
            }
            return true;
        }
    };

    typedef std::shared_ptr<ConfigVar> ptr;
    typedef boost::signals2::signal<bool (const T&), BreakOnFailureCombiner> before_change_signal_type;
    typedef boost::signals2::signal<void (const T&)> on_change_signal_type;

public:
    ConfigVar(const std::string &name, const T &defaultValue,
        const std::string &description = "", bool lockable = false)
        : ConfigVarBase(name, description, lockable),
          m_val(defaultValue)
    {
        // if Config is locked, should reject changes to lockable ConfigVars
        if (isLockable())
            beforeChange.connect(&isConfigNotLocked<T>);
    }

    std::string toString() const
    {
        return boost::lexical_cast<std::string>(m_val);
    }

    bool fromString(const std::string &str)
    {
        try {
            return val(boost::lexical_cast<T>(str));
        } catch (boost::bad_lexical_cast &) {
            return false;
        }
    }

    /// beforeChange gives the opportunity to reject the new value;
    /// return false or throw an exception to prevent the change
    before_change_signal_type beforeChange;
    /// onChange should not throw any exceptions
    on_change_signal_type onChange;

    // TODO: atomicCompareExchange and/or mutex
    T val() const { return m_val; }
    bool val(const T &v)
    {
        T oldVal = m_val;
        if (oldVal != v) {
            if (!beforeChange(v))
                return false;
            m_val = v;
            onChange(v);
            ConfigVarBase::onChange();
        }
        return true;
    }

private:
    T m_val;
};

class Config
{
private:
    static std::string getName(const ConfigVarBase::ptr &var)
    {
        return var->name();
    }
    typedef boost::multi_index_container<
        ConfigVarBase::ptr,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<
            boost::multi_index::global_fun<const ConfigVarBase::ptr &,
                std::string, &getName> >
        >
    > ConfigVarSet;

public:
#ifdef WINDOWS
    /// Encapsulates monitoring the registry for ConfigVar changes
    struct RegistryMonitor
    {
        friend class Config;
    public:
        typedef std::shared_ptr<RegistryMonitor> ptr;
    private:
        RegistryMonitor(IOManager &iomanager, HKEY hKey,
            const std::wstring &subKey);

    public:
        RegistryMonitor(const RegistryMonitor &copy);
        ~RegistryMonitor();

    private:
        static void onRegistryChange(std::weak_ptr<RegistryMonitor> self);

    private:
        IOManager &m_ioManager;
        HKEY m_hKey;
        HANDLE m_hEvent;
    };
#endif

public:
    /// Declare a ConfigVar
    ///
    /// @note A ConfigVar can only be declared once.
    /// @throws std::invalid_argument With what() == the name of the ConfigVar
    ///         if the value is not valid.
    template <class T>
    static typename ConfigVar<T>::ptr lookup(const std::string &name,
        const T &defaultValue, const std::string &description = "",
        bool lockable = false)
    {
        if (!isValidConfigVarName(name))
            MORDOR_THROW_EXCEPTION(std::invalid_argument(name));

        MORDOR_ASSERT(vars().find(name) == vars().end());
        typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, defaultValue,
            description, lockable));
        vars().insert(v);
        return v;
    }

    //This signature of Lookup is used to perform a lookup for a
    //previously declared ConfigVar
    static ConfigVarBase::ptr lookup(const std::string &name);

    // Use to iterate all the ConfigVars
    static void visit(boost::function<void (ConfigVarBase::ptr)> dg);

    /// Load ConfigVars from command line arguments
    ///
    /// argv[0] is skipped (assumed to be the program name), and argc and argv
    /// are updated to remove any arguments that were used to set ConfigVars.
    /// Arguments can be of the form --configVarName=value or
    /// --configVarName value.  Any arguments after a -- are ignored.
    /// @throws std::invalid_argument With what() == the name of the ConfigVar
    ///         if the value was not successfully set
    static void loadFromCommandLine(int &argc, char *argv[]);

    // Update value of ConfigVars based on environmental variables.
    // This is done by iterating the environmental looking for any that match
    // the format KEY=VALUE for a previously declared ConfigVar.
    // The key is automatically converted to lowercase, and "_" can be
    // used in place of "."
    static void loadFromEnvironment();

    // Update value of ConfigVars based on json object.
    // If a config var not declared previously,
    // we will create a new var to save it.
    static void loadFromJSON(const JSON::Value &json);
#ifdef WINDOWS
    static void loadFromRegistry(HKEY key, const std::string &subKey);
    static void loadFromRegistry(HKEY key, const std::wstring &subKey);
    /// @see RegistryMonitor
    static RegistryMonitor::ptr monitorRegistry(IOManager &ioManager, HKEY key,
        const std::string &subKey);
    /// @see RegistryMonitor
    static RegistryMonitor::ptr monitorRegistry(IOManager &ioManager, HKEY key,
        const std::wstring &subKey);
#endif

    /// Set lock flag of the Config
    /// @param locked If set to true, it will lock those lockable Configvars from
    ///               updating their values.
    static void lock(bool locked) { s_locked = locked; }
    static bool isLocked() { return s_locked; }

private:
    static ConfigVarSet &vars()
    {
        static ConfigVarSet vars;
        return vars;
    }
    static bool s_locked;
};


template <class T>
bool isConfigNotLocked(const T &)
{
    return !Config::isLocked();
}

class Timer;
class TimerManager;
class Scheduler;

#if 0 // Zs
/// Creates a timer that is controlled by a string ConfigVar
///
/// The timer is automatically updated to use the current value of the
/// ConfigVar, and the string is parsed with stringToMicroseconds from string.h
std::shared_ptr<Timer> associateTimerWithConfigVar(
    TimerManager &timerManager,
    std::shared_ptr<ConfigVar<std::string> > configVar,
    boost::function<void ()> dg);
#endif // Ze

/// Associate a scheduler with a ConfigVar
///
/// Allows dynamically changing the number of threads associated with a
/// scheduler.  Negative values are taken to mean a multiplier of the number
/// of available processor cores.
void associateSchedulerWithConfigVar(Scheduler &scheduler,
    std::shared_ptr<ConfigVar<int> > configVar);


/// helper class to allow temporarily change the ConfigVar Value
///
/// When an instance is created, the specified ConfigVar is hijacked to the @c
/// tempValue, after the instance is @c reset() or destroyed, the value is
/// restored.
class HijackConfigVar
{
public:
    HijackConfigVar(const std::string &name, const std::string &value);
    const std::string& originValue() const { return m_oldValue; }
    void reset();
    ~HijackConfigVar();

private:
    std::shared_ptr<Mordor::ConfigVarBase> m_var;
    std::string m_oldValue;
};

}

#endif

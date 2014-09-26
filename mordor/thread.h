#ifndef __MORDOR_THREAD_H__
#define __MORDOR_THREAD_H__
// Copyright (c) 2010 - Mozy, Inc.

#include <iosfwd>

#include <boost/function.hpp>

#include "version.h"
#ifndef WINDOWS
#include "semaphore.h"
#endif

#include "util.h"

namespace Mordor {

#ifdef WINDOWS
typedef DWORD tid_t;
#elif defined(LINUX)
typedef pid_t tid_t;
#elif defined(OSX)
typedef mach_port_t tid_t;
#else
typedef pthread_t tid_t;
#endif

inline tid_t emptytid() { return (tid_t)-1; }
tid_t gettid();

class Scheduler;

class Thread : Mordor::noncopyable
{
public:
    /// thread bookmark
    ///
    /// bookmark current thread id and scheduler, allow to switch back to
    /// the same thread id later.
    /// @pre The process must be running with available scheduler, otherwise
    /// it is not possible to switch execution between threads with bookmark.
    /// @note Bookmark was designed to address the issue where we failed to
    /// rethrow an exception in catch block, because GCC C++ runtime saves the
    /// exception stack in a pthread TLS variable. and swapcontext(3) does not
    /// take care of TLS. but developer needs to be more aware of underlying
    /// thread using thread bookmark, so we developed another way to fix this
    /// problem. thus bookmark only serve as a way which allow user to stick
    /// to a native thread.
    class Bookmark
    {
    public:
        Bookmark();
        /// switch to bookmark's tid
        void switchTo();
        /// bookmark's tid
        tid_t tid() const { return m_tid; }

    private:
        Scheduler *m_scheduler;
        tid_t m_tid;
    };

public:
    Thread(boost::function<void ()> dg, const char *name = NULL);
    ~Thread();

    tid_t tid() const { return m_tid; }

    void join();

private:
    static
#ifdef WINDOWS
    unsigned WINAPI
#else
    void *
#endif
    run(void *arg);

private:
    tid_t m_tid;
#ifdef WINDOWS
    HANDLE m_hThread;
#else
    pthread_t m_thread;
#endif

#ifdef LINUX
    boost::function<void ()> m_dg;
    Semaphore m_semaphore;
    const char *m_name;
#endif
};

}

#endif

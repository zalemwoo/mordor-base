#ifndef __MORDOR_IOMANAGER_EPOLL_H__
#define __MORDOR_IOMANAGER_EPOLL_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <sys/types.h>
#include <sys/event.h>
#include <mutex>
#include <map>

#include "scheduler.h"
#include "timer.h"
#include "version.h"

#ifndef BSD
#error IOManagerKQueue is BSD only
#endif

namespace Mordor {

class IOManager : public Scheduler, public TimerManager
{
public:
    enum Event {
        READ,
        WRITE,
        CLOSE
    };

private:
    struct AsyncEvent
    {
        struct kevent event;

        Scheduler *m_scheduler, *m_schedulerClose;
        std::shared_ptr<Fiber> m_fiber, m_fiberClose;
        std::function<void ()> m_dg, m_dgClose;
    };

public:
    IOManager(size_t threads = 1, bool useCaller = true, bool autoStart = true);
    ~IOManager();

    bool stopping();

    void registerEvent(int fd, Event events, std::function<void ()> dg = NULL);
    void cancelEvent(int fd, Event events);
    void unregisterEvent(int fd, Event events);

protected:
    bool stopping(unsigned long long &nextTimeout);
    void idle();
    void tickle();

    void onTimerInsertedAtFront() { tickle(); }

private:
    int m_kqfd;
    int m_tickleFds[2];
    std::map<std::pair<int, Event>, AsyncEvent> m_pendingEvents;
    std::mutex m_mutex;
};

}

#endif


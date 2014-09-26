#ifndef __MORDOR_SCHEDULER_H__
#define __MORDOR_SCHEDULER_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <list>
#include <mutex>

#include <boost/function.hpp>

#include "util.h"
#include "thread.h"
#include "thread_local_storage.h"

namespace Mordor {

class Fiber;

/// Cooperative user-mode thread (Fiber) Scheduler

/// A Scheduler is used to cooperatively schedule fibers on threads,
/// implementing an M-on-N threading model. A Scheduler can either "hijack"
/// the thread it was created on (by passing useCaller = true in the
/// constructor), spawn multiple threads of its own, or a hybrid of the two.
///
/// Hijacking and Schedulers begin processing fibers when either
/// yieldTo() or dispatch() is called. The Scheduler will stop itself when
/// there are no more Fibers scheduled, and return from yieldTo() or
/// dispatch(). Hybrid and spawned Schedulers must be explicitly stopped via
/// stop(). stop() will return only after there are no more Fibers scheduled.
class Scheduler : public Mordor::noncopyable
{
public:
    /// Default constructor

    /// By default, a single-threaded hijacking Scheduler is constructed.
    /// If threads > 1 && useCaller == true, a hybrid Scheduler is constructed.
    /// If useCaller == false, this Scheduler will not be associated with
    /// the currently executing thread.
    /// @param threads How many threads this Scheduler should be comprised of
    /// @param useCaller If this Scheduler should "hijack" the currently
    /// executing thread
    /// @param batchSize Number of operations to pull off the scheduler queue
    /// on every iteration
    /// @pre if (useCaller == true) Scheduler::getThis() == NULL
    Scheduler(size_t threads = 1, bool useCaller = true, size_t batchSize = 1);
    /// Destroys the scheduler, implicitly calling stop()
    virtual ~Scheduler();

    /// @return The Scheduler controlling the currently executing thread
    static Scheduler* getThis();

    /// Explicitly start the Scheduler

    /// Derived classes should call start() in their constructor.
    /// It is safe to call start() even if the Scheduler is already started -
    /// it will be a no-op
    void start();

    /// Explicitly stop the scheduler

    /// This must be called for hybrid and spawned Schedulers.  It is safe to
    /// call stop() even if the Scheduler is already stopped (or stopping) -
    /// it will be a no-op
    /// For hybrid or hijacking schedulers, it must be called from within
    /// the scheduler.  For spawned Schedulers, it must be called from outside
    /// the Scheduler.
    /// If called on a hybrid/hijacking scheduler from a Fiber
    /// that did not create the Scheduler, it will return immediately (the
    /// Scheduler will yield to the creating Fiber when all work is complete).
    /// In all other cases stop() will not return until all work is complete.
    void stop();

    /// Schedule a Fiber to be executed on the Scheduler

    /// @param fd The Fiber or the functor to schedule, if a pointer is passed
    ///           in, the ownership will be transfered to this scheduler
    /// @param thread Optionally provide a specific thread for the Fiber to run
    /// on
    template <class FiberOrDg>
    void schedule(FiberOrDg fd, tid_t thread = emptytid())
    {
        bool tickleMe;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            tickleMe = scheduleNoLock(fd, thread);
        }
        if (shouldTickle(tickleMe))
            tickle();
    }

    /// Schedule multiple items to be executed at once

    /// @param begin The first item to schedule
    /// @param end One past the last item to schedule
    template <class InputIterator>
    void schedule(InputIterator begin, InputIterator end)
    {
        bool tickleMe = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            while (begin != end) {
                tickleMe = scheduleNoLock(&*begin) || tickleMe;
                ++begin;
            }
        }
        if (shouldTickle(tickleMe))
            tickle();
    }

    /// Change the currently executing Fiber to be running on this Scheduler

    /// This function can be used to change which Scheduler/thread the
    /// currently executing Fiber is executing on.  This switch is done by
    /// rescheduling this Fiber on this Scheduler, and yielding to the current
    /// Scheduler.
    /// @param thread Optionally provide a specific thread for this Fiber to
    /// run on
    /// @post Scheduler::getThis() == this
    void switchTo(tid_t thread = emptytid());

    /// Yield to the Scheduler to allow other Fibers to execute on this thread

    /// The Scheduler will not re-schedule this Fiber automatically.
    ///
    /// In a hijacking Scheduler, any scheduled work will begin running if
    /// someone yields to the Scheduler.
    /// @pre Scheduler::getThis() != NULL
    static void yieldTo();

    /// Yield to the Scheduler to allow other Fibers to execute on this thread

    /// The Scheduler will automatically re-schedule this Fiber.
    /// @pre Scheduler::getThis() != NULL
    static void yield();

    /// Force a hijacking Scheduler to process scheduled work

    /// Calls yieldTo(), and yields back to the currently executing Fiber
    /// when there is no more work to be done
    /// @pre this is a hijacking Scheduler
    void dispatch();

    size_t threadCount() const
    {
        return m_threadCount + (m_rootFiber ? 1 : 0);
    }
    /// Change the number of threads in this scheduler
    void threadCount(size_t threads);

    const std::vector<std::shared_ptr<Thread> >& threads() const
    {
        return m_threads;
    }

    tid_t rootThreadId() const { return m_rootThread; }
protected:
    /// Derived classes can query stopping() to see if the Scheduler is trying
    /// to stop, and should return from the idle Fiber as soon as possible.
    ///
    /// Also, this function should be implemented if the derived class has
    /// any additional work to do in the idle Fiber that the Scheduler is not
    /// aware of.
    virtual bool stopping();

    /// The function called (in its own Fiber) when there is no work scheduled
    /// on the Scheduler.  The Scheduler is not considered stopped until the
    /// idle Fiber has terminated.
    ///
    /// Implementors should Fiber::yield() when it believes there is work
    /// scheduled on the Scheduler.
    virtual void idle() = 0;
    /// The Scheduler wants to force the idle fiber to Fiber::yield(), because
    /// new work has been scheduled.
    virtual void tickle() = 0;

    bool hasWorkToDo();
    virtual bool hasIdleThreads() const { return m_idleThreadCount != 0; }

    /// determine whether tickle() is needed, to be invoked in schedule()
    /// @param empty whether m_fibers is empty before the new task is scheduled
    virtual bool shouldTickle(bool empty) const
    { return empty && Scheduler::getThis() != this; }

    /// set `this' to TLS so that getThis() can get correct Scheduler
    void setThis() { t_scheduler = this; }

private:
    void yieldTo(bool yieldToCallerOnTerminate);
    void run();

    /// @pre @c fd should be valid
    /// @pre the task to be scheduled is not thread-targeted, or this scheduler
    ///      owns the targeted thread.
    template <class FiberOrDg>
        bool scheduleNoLock(FiberOrDg fd,
                            tid_t thread = emptytid()) {
        bool tickleMe = m_fibers.empty();
        m_fibers.push_back(FiberAndThread(fd, thread));
        return tickleMe;
    }

private:
    struct FiberAndThread {
        std::shared_ptr<Fiber> fiber;
        std::function<void ()> dg;
        tid_t thread;
        FiberAndThread(std::shared_ptr<Fiber> f, tid_t th)
            : fiber(f), thread(th) {}
        FiberAndThread(std::shared_ptr<Fiber>* f, tid_t th)
            : thread(th) {
            fiber.swap(*f);
        }
        FiberAndThread(std::function<void ()> d, tid_t th)
            : dg(d), thread(th) {}
        FiberAndThread(std::function<void ()> *d, tid_t th)
            : thread(th) {
            dg.swap(*d);
        }
    };
    static ThreadLocalStorage<Scheduler *> t_scheduler;
    static ThreadLocalStorage<Fiber *> t_fiber;
    std::mutex m_mutex;
    std::list<FiberAndThread> m_fibers;
    tid_t m_rootThread;
    std::shared_ptr<Fiber> m_rootFiber;
    std::shared_ptr<Fiber> m_callingFiber;
    std::vector<std::shared_ptr<Thread> > m_threads;
    size_t m_threadCount, m_activeThreadCount, m_idleThreadCount;
    bool m_stopping;
    bool m_autoStop;
    size_t m_batchSize;
};

/// Automatic Scheduler switcher

/// Automatically returns to Scheduler::getThis() when goes out of scope
/// (by calling Scheduler::switchTo())
struct SchedulerSwitcher : public Mordor::noncopyable
{
public:
    /// Captures Scheduler::getThis(), and optionally calls target->switchTo()
    /// if target != NULL
    SchedulerSwitcher(Scheduler *target = NULL);
    /// Calls switchTo() on the Scheduler captured in the constructor
    /// @post Scheduler::getThis() == the Scheduler captured in the constructor
    ~SchedulerSwitcher();

private:
    Scheduler *m_caller;
};

}

#endif

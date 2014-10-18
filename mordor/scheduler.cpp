// Copyright (c) 2009 - Mozy, Inc.

#include "scheduler.h"

#include "atomic.h"
#include "assert.h"
#include "fiber.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:scheduler");

ThreadLocalStorage<Scheduler *> Scheduler::t_scheduler;
ThreadLocalStorage<Fiber *> Scheduler::t_fiber;

Scheduler::Scheduler(size_t threads, bool useCaller, size_t batchSize)
    : m_activeThreadCount(0),
      m_idleThreadCount(0),
      m_stopping(true),
      m_autoStop(false),
      m_batchSize(batchSize)
{
    MORDOR_ASSERT(threads >= 1);
    if (useCaller) {
        --threads;
        MORDOR_ASSERT(getThis() == NULL);
        t_scheduler = this;
        m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this)));
        t_scheduler = this;
        t_fiber = m_rootFiber.get();
        m_rootThread = gettid();
    } else {
        m_rootThread = emptytid();
    }
    m_threadCount = threads;
}

Scheduler::~Scheduler()
{
    MORDOR_ASSERT(m_stopping);
    if (getThis() == this) {
        t_scheduler = NULL;
    }
}

Scheduler *
Scheduler::getThis()
{
    return t_scheduler.get();
}

void
Scheduler::start()
{
    MORDOR_LOG_VERBOSE(g_log) << this << " starting " << m_threadCount << " threads";
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_stopping)
        return;
    // TODO: There may be a race condition here if one thread calls stop(),
    // and another thread calls start() before the worker threads for this
    // scheduler actually exit; they may resurrect themselves, and the stopping
    // thread would block waiting for the thread to exit

    m_stopping = false;
    MORDOR_ASSERT(m_threads.empty());
    m_threads.resize(m_threadCount);
    for (size_t i = 0; i < m_threadCount; ++i) {
        m_threads[i] = std::shared_ptr<Thread>(new Thread(
            std::bind(&Scheduler::run, this)));
    }
}

bool
Scheduler::hasWorkToDo()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_fibers.empty();
}

void
Scheduler::stop()
{
    // Already stopped
    if (m_rootFiber &&
        m_threadCount == 0 &&
        (m_rootFiber->state() == Fiber::TERM || m_rootFiber->state() == Fiber::INIT)) {
        MORDOR_LOG_VERBOSE(g_log) << this << " stopped";
        m_stopping = true;
        // A derived class may inhibit stopping while it has things to do in
        // its idle loop, so we can't break early
        if (stopping())
            return;
    }

    bool exitOnThisFiber = false;
    if (m_rootThread != emptytid()) {
        // A thread-hijacking scheduler must be stopped
        // from within itself to return control to the
        // original thread
        MORDOR_ASSERT(Scheduler::getThis() == this);
        if (Fiber::getThis() == m_callingFiber) {
            exitOnThisFiber = true;
            // First switch to the correct thread
            MORDOR_LOG_DEBUG(g_log) << this
                << " switching to root thread to stop";
            switchTo(m_rootThread);
        }
        if (!m_callingFiber)
            exitOnThisFiber = true;
    } else {
        // A spawned-threads only scheduler cannot be stopped from within
        // itself... who would get control?
        MORDOR_ASSERT(Scheduler::getThis() != this);
    }
    m_stopping = true;
    for (size_t i = 0; i < m_threadCount; ++i)
        tickle();
    if (m_rootFiber && (m_threadCount != 0u || Scheduler::getThis() != this))
        tickle();
    // Wait for all work to stop on this thread
    if (exitOnThisFiber) {
        while (!stopping()) {
            // Give this thread's run fiber a chance to kill itself off
            MORDOR_LOG_DEBUG(g_log) << this
                << " yielding to this thread to stop";
            yieldTo(true);
        }
    }
    // Wait for other threads to stop
    if (exitOnThisFiber ||
        Scheduler::getThis() != this) {
        MORDOR_LOG_DEBUG(g_log) << this
            << " waiting for other threads to stop";
        std::vector<std::shared_ptr<Thread> > threads;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            threads.swap(m_threads);
        }
        for (std::vector<std::shared_ptr<Thread> >::const_iterator it
            (threads.begin());
            it != threads.end();
            ++it) {
            (*it)->join();
        }
    }
    MORDOR_LOG_VERBOSE(g_log) << this << " stopped";
}

bool
Scheduler::stopping()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stopping && m_fibers.empty() && m_activeThreadCount == 0;
}

void
Scheduler::switchTo(tid_t thread)
{
    MORDOR_ASSERT(Scheduler::getThis() != NULL);
    if (Scheduler::getThis() == this) {
        if (thread == emptytid() || thread == gettid())
            return;
    }
    MORDOR_LOG_DEBUG(g_log) << this << " switching to thread " << thread;
    schedule(Fiber::getThis(), thread);
    Scheduler::yieldTo();
}

void
Scheduler::yieldTo()
{
    Scheduler *self = Scheduler::getThis();
    MORDOR_ASSERT(self);
    MORDOR_LOG_DEBUG(g_log) << self << " yielding to scheduler";
    MORDOR_ASSERT(t_fiber.get());
    if (self->m_rootThread == gettid() &&
        (t_fiber->state() == Fiber::INIT || t_fiber->state() == Fiber::TERM)) {
        self->m_callingFiber = Fiber::getThis();
        self->yieldTo(true);
    } else {
        self->yieldTo(false);
    }
}

void
Scheduler::yield()
{
    MORDOR_ASSERT(Scheduler::getThis());
    Scheduler::getThis()->schedule(Fiber::getThis());
    yieldTo();
}

void
Scheduler::dispatch()
{
    MORDOR_LOG_DEBUG(g_log) << this << " dispatching";
    MORDOR_ASSERT(m_rootThread == gettid() && m_threadCount == 0);
    m_stopping = true;
    m_autoStop = true;
    yieldTo();
    m_autoStop = false;
}

void
Scheduler::threadCount(size_t threads)
{
    MORDOR_ASSERT(threads >= 1);
    if (m_rootFiber)
        --threads;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (threads == m_threadCount) {
        return;
    } else if (threads > m_threadCount) {
        m_threads.resize(threads);
        for (size_t i = m_threadCount; i < threads; ++i)
            m_threads[i] = std::shared_ptr<Thread>(new Thread(
            std::bind(&Scheduler::run, this)));
    }
    m_threadCount = threads;
}

void
Scheduler::yieldTo(bool yieldToCallerOnTerminate)
{
    MORDOR_ASSERT(t_fiber.get());
    MORDOR_ASSERT(Scheduler::getThis() == this);
    if (yieldToCallerOnTerminate)
        MORDOR_ASSERT(m_rootThread == gettid());
    if (t_fiber->state() != Fiber::HOLD) {
        m_stopping = m_autoStop || m_stopping;
        // XXX: is t_fiber the hijacked thread ?
        t_fiber->reset(std::bind(&Scheduler::run, this));
    }
    t_fiber->yieldTo(yieldToCallerOnTerminate);
}

void
Scheduler::run()
{
    setThis();
    if (gettid() != m_rootThread) {
        // Running in own thread
        t_fiber = Fiber::getThis().get();
    } else {
        // Hijacked a thread
        MORDOR_ASSERT(t_fiber.get() == Fiber::getThis().get());
    }
    Fiber::ptr idleFiber(new Fiber(std::bind(&Scheduler::idle, this)));
    MORDOR_LOG_VERBOSE(g_log) << this << " starting thread with idle fiber " << idleFiber;
    Fiber::ptr dgFiber;
    // use a vector for O(1) .size()
    std::vector<FiberAndThread> batch;
    batch.reserve(m_batchSize);
    bool isActive = false;
    while (true) {
        MORDOR_ASSERT(batch.empty());
        bool dontIdle = false;
        bool tickleMe = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            // Kill ourselves off if needed
            if (m_threads.size() > m_threadCount && gettid() != m_rootThread) {
                // Accounting
                if (isActive)
                    --m_activeThreadCount;
                // Kill off the idle fiber
                try {
                    throw new OperationAbortedException();
                } catch(...) {
                    idleFiber->inject(std::current_exception());
                }
                // Detach our thread
                for (std::vector<std::shared_ptr<Thread> >
                    ::iterator it = m_threads.begin();
                    it != m_threads.end();
                    ++it)
                    if ((*it)->tid() == gettid()) {
                        m_threads.erase(it);
                        if (m_threads.size() > m_threadCount)
                            tickle();
                        return;
                    }
                MORDOR_NOTREACHED();
            }

            std::list<FiberAndThread>::iterator it(m_fibers.begin());
            while (it != m_fibers.end()) {
                // If we've met our batch size, and we're not checking to see
                // if we need to tickle another thread, then break
                if ( (tickleMe || m_activeThreadCount == threadCount()) &&
                    batch.size() == m_batchSize)
                    break;
                if (it->thread != emptytid() && it->thread != gettid()) {
                    MORDOR_LOG_DEBUG(g_log) << this
                        << " skipping item scheduled for thread "
                        << it->thread;

                    // Wake up another thread to hopefully service this
                    tickleMe = true;
                    dontIdle = true;
                    ++it;
                    continue;
                }
                MORDOR_ASSERT(it->fiber || it->dg);
                // This fiber is still executing; probably just some race
                // race condition that it needs to yield on one thread
                // before running on another thread
                if (it->fiber && it->fiber->state() == Fiber::EXEC) {
                    MORDOR_LOG_DEBUG(g_log) << this
                        << " skipping executing fiber " << it->fiber;
                    ++it;
                    dontIdle = true;
                    continue;
                }
                // We were just checking if there is more work; there is, so
                // set the flag and don't actually take this piece of work
                if (batch.size() == m_batchSize) {
                    tickleMe = true;
                    break;
                }
                batch.push_back(*it);
                it = m_fibers.erase(it);
                if (!isActive) {
                    ++m_activeThreadCount;
                    isActive = true;
                }
            }
            if (batch.empty() && isActive) {
                --m_activeThreadCount;
                isActive = false;
            }
        }
        if (tickleMe)
            tickle();
        MORDOR_LOG_DEBUG(g_log) << this
            << " got " << batch.size() << " fiber/dgs to process (max: "
            << m_batchSize << ", active: " << isActive << ")";
        MORDOR_ASSERT(isActive == !batch.empty());

        if (batch.empty()) {
            if (dontIdle)
                continue;

            if (idleFiber->state() == Fiber::TERM) {
                MORDOR_LOG_DEBUG(g_log) << this << " idle fiber terminated";
                if (gettid() == m_rootThread)
                    m_callingFiber.reset();
                // Unblock the next thread
                if (threadCount() > 1)
                    tickle();
                return;
            }
            MORDOR_LOG_DEBUG(g_log) << this << " idling";
            atomicIncrement(m_idleThreadCount);
            idleFiber->call();
            atomicDecrement(m_idleThreadCount);
            continue;
        }

        while (!batch.empty()) {
            FiberAndThread& ft = batch.back();
            Fiber::ptr f = ft.fiber;
            std::function<void ()> dg = ft.dg;
            batch.pop_back();

            try {
                if (f && f->state() != Fiber::TERM) {
                    MORDOR_LOG_DEBUG(g_log) << this << " running " << f;
                    f->yieldTo();
                } else if (dg) {
                    if (dgFiber)
                        dgFiber->reset(dg);
                    else
                        dgFiber.reset(new Fiber(dg));
                    // MORDOR_LOG_DEBUG(g_log) << this << " running " << dg; // Z
                    dg = NULL;
                    dgFiber->yieldTo();
                    if (dgFiber->state() != Fiber::TERM)
                        dgFiber.reset();
                    else
                        dgFiber->reset(NULL);
                }
            } catch (...) {
                try {
                    MORDOR_LOG_FATAL(Log::root());
                }
                catch(...) {
                    // Swallow any exceptions that might occur while trying to log the current fiber state #98680
                }

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    // push all un-executed fibers back to m_fibers
                    copy(batch.begin(), batch.end(), back_inserter(m_fibers));
                    batch.clear();
                    // decrease the activeCount as this thread is in exception
                    isActive = false;
                    --m_activeThreadCount;
                }
                throw;
            }
        }
    }
}

SchedulerSwitcher::SchedulerSwitcher(Scheduler *target)
{
    m_caller = Scheduler::getThis();
    if (target)
        target->switchTo();
}

SchedulerSwitcher::~SchedulerSwitcher()
{
    if (m_caller)
        m_caller->switchTo();
}

}

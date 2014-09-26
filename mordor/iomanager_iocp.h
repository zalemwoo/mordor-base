#ifndef __MORDOR_IOMANAGER_IOCP_H__
#define __MORDOR_IOMANAGER_IOCP_H__

#include <map>

#include <mutex>

#include "scheduler.h"
#include "timer.h"
#include "version.h"

#ifndef WINDOWS
#error IOManagerIOCP is Windows only
#endif

namespace Mordor {

class Fiber;

struct AsyncEvent
{
    AsyncEvent();

    OVERLAPPED overlapped;

    Scheduler  *m_scheduler;
    tid_t m_thread;
    std::shared_ptr<Fiber> m_fiber;
};

class IOManager : public Scheduler, public TimerManager
{
    friend class WaitBlock;
private:
    class WaitBlock : public std::enable_shared_from_this<WaitBlock>
    {
    public:
        typedef std::shared_ptr<WaitBlock> ptr;
    public:
        WaitBlock(IOManager &outer);
        ~WaitBlock();

        bool registerEvent(HANDLE handle, std::function<void ()> dg,
            bool recurring);
        size_t unregisterEvent(HANDLE handle);

    private:
        void run();
        void removeEntry(int index);

    private:
        std::mutex m_mutex;
        IOManager &m_outer;
        HANDLE m_reconfigured;
        HANDLE m_handles[MAXIMUM_WAIT_OBJECTS];
        Scheduler *m_schedulers[MAXIMUM_WAIT_OBJECTS];
        std::shared_ptr<Fiber> m_fibers[MAXIMUM_WAIT_OBJECTS];
        std::function<void ()> m_dgs[MAXIMUM_WAIT_OBJECTS];
        bool m_recurring[MAXIMUM_WAIT_OBJECTS];
        int m_inUseCount;
    };

public:
    IOManager(size_t threads = 1, bool useCaller = true, bool autoStart = true);
    ~IOManager();

    bool stopping();

    // Associate the handle with the IOManagers completion port
    // This must be called one per handle before making Windows
    // system calls that use asynchronous IO
    void registerFile(HANDLE handle);

    // Callers who have registered a handle with registerFile() must
    // call this method to prepare the IOManager before performing a
    // Windows system call on that handle that expects a OVERLAPPED structure
    // (e.g. ConnectEx, WSASend, ReadDirectoryChanges etc)
    // The IOManager will add context information to the AsyncEvent structure.
    // The caller must then pass the AsyncEvent::overlapped member 
    // as the lpOverlapped argument for the async IO call.
    // After making the async call the caller will normally call yieldTo() to stop
    // execution.  The IOManager will resume the caller fiber when the IO call completes.
    // At that point the caller can use the AsyncEvent::overlapped member to learn
    // the result of the IO call.
    void registerEvent(AsyncEvent *e);

    // If a caller has called registerEvent to prepare for an Async IO call
    // but then the Async IO call fails this must be called so that the IOManager
    // does not wait for the Async IO call to complete.
    // This does not work to cancel a successfully launched Async IO call.
    void unregisterEvent(AsyncEvent *e);

    // Register a handle to an Windows Event.
    // The callback "dg" will be scheduled once the event
    // is signalled.
    void registerEvent(HANDLE handle, std::function<void ()> dg,
        bool recurring = false);

    // Register a handle to an Windows Event.
    // Use this method when a fiber wants to sleep until an event is signalled.
    // The caller will typically yield its fiber immediately after
    // calling this method and the fiber will be rescheduled as
    // soon as the event is signalled.
    // (see CreateEventW, WaitForMultipleObjects, WSAEventSelect)
    // Note: See FiberEvent for a cross platform event primitive.
    void registerEvent(HANDLE handle, bool recurring = false)
    { registerEvent(handle, NULL, recurring); }

    // Cancel the registration of event handle that was previously
    // registered with the IOManager
    size_t unregisterEvent(HANDLE handle);

    // Cancel an Async IO call that has already been successfully launched.
    // If successfully the fiber that is waiting for the result
    // will be resumed and the AsyncEvent::overlapped will have the
    // ERROR_OPERATION_ABORTED result.  
    void cancelEvent(HANDLE hFile, AsyncEvent *e);

    // #111932
    // HACK (hopefully temporary).
    // This method allows the caller to specify the number of errors to ignore when
    // calling PostQueuedCompletionStatus in the tickle() method. The default value is 0.
    // The Sync product has experienced some "Insufficient system resources" errors
    // returned by PostQueuedCompletionStatus, possibly indicating the the IOCP queue is full.
    // Be careful when using this method as it is possible for the corresponding
    // GetQueuedCompletionStatusEx call in idle() to wait infinitely, which could cause deadlock if
    // the PostQueuedCompletionStatus failure is ignored. (Sync makes use of many Timers which 
    // should ensure that GetQueuedCompletionStatusEx will not ever wait infinitely.)
    // Parameters
    //   count: The number of errors to allow
    //   seconds: The time span within which the number of errors must exceed the maximum specified in count.
    static void setIOCPErrorTolerance(size_t count, size_t seconds);

protected:
    bool stopping(unsigned long long &nextTimeout);
    void idle();
    void tickle();

    // Call when a new timer is added that will be the next timer 
    // to expire.  We have to tickle() the IOManager so that it can 
    // adjust the timeout value in its blocking call to GetQueuedCompletionStatusEx
    // so that it doesn't miss the timer
    void onTimerInsertedAtFront() { tickle(); }

private:
    HANDLE m_hCompletionPort;
#ifndef NDEBUG
    std::map<OVERLAPPED *, AsyncEvent*> m_pendingEvents;
#endif
    size_t m_pendingEventCount;
    std::mutex m_mutex;
    std::list<WaitBlock::ptr> m_waitBlocks;

    // These variables are part of the hack for #111932.
    // See the comment for setIOCPErrorTolerance().
    static std::mutex m_errorMutex;
    static size_t m_iocpAllowedErrorCount;
    static size_t m_iocpErrorCountWindowInSeconds;
    static size_t m_errorCount;
    static unsigned long long m_firstErrorTime;
};

}

#endif

// Copyright (c) 2009 - Mozy, Inc.

#ifdef WINDOWS

#include "mordor/iomanager.h"
#include "mordor/test/test.h"

using namespace Mordor;
using namespace Mordor::Test;

static void handleEvent(bool &fired)
{
    fired = true;
}

static void signalEvent(HANDLE hEvent, int delay)
{
    Sleep(delay);
    SetEvent(hEvent);
}

static void unregisterEvent(IOManager &ioManager, HANDLE hEvent, bool &fired)
{
    ioManager.unregisterEvent(hEvent);
    fired = true;
}

MORDOR_UNITTEST(IOManager, eventPreventsStop)
{
    HANDLE hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hEvent)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
    bool fired = false;
    try {
        IOManager ioManager;
        ioManager.registerEvent(hEvent, std::bind(&handleEvent,
            std::ref(fired)));
        Thread thread(std::bind(&signalEvent, hEvent, 500));
    } catch (...) {
        CloseHandle(hEvent);
    }
    MORDOR_TEST_ASSERT(fired);
    CloseHandle(hEvent);
}

// This test can only truly be tested under a debugger, by freezing
// the unregisterEvent call after removing it from the array, but
// before it can signal the event for the waitblock thread to reconfigure
// itself, and making sure that the signalEvent here causes the waitblock
// thread to receive the event when it is not in the array.
// Beyond that, this test will also exercise events being unregistered
// before the wait thread even begins.
MORDOR_UNITTEST(IOManager, waitBlockUnregisterAndFire)
{
    HANDLE hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hEvent)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
    bool fired = false;
    IOManager ioManager;
    ioManager.registerEvent(hEvent, std::bind(&handleEvent,
        std::ref(fired)));
    Thread thread(std::bind(&signalEvent, hEvent, 0));
    ioManager.unregisterEvent(hEvent);
    ioManager.dispatch();
    CloseHandle(hEvent);
}

// This test can also only be truly tested under a debugger, by freezing
// the run thread after it has processed the event, but before it terminates
MORDOR_UNITTEST(IOManager, waitBlockFireThenUnregister)
{
    HANDLE hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hEvent)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
    bool fired = false;
    IOManager ioManager;
    ioManager.registerEvent(hEvent, std::bind(&handleEvent,
        std::ref(fired)));
    SetEvent(hEvent);
    while (!fired) Scheduler::yield();
    ioManager.unregisterEvent(hEvent);
    CloseHandle(hEvent);
}

MORDOR_UNITTEST(IOManager, waitBlockUnregisterOtherFires)
{
    HANDLE hEvent1 = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hEvent1)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
    HANDLE hEvent2 = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hEvent2) {
        CloseHandle(hEvent1);
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
    }
    bool fired1 = false, fired2 = false;
    IOManager ioManager;
    ioManager.registerEvent(hEvent1, std::bind(&handleEvent,
        std::ref(fired1)));
    ioManager.registerEvent(hEvent2, std::bind(&handleEvent,
        std::ref(fired2)));
    ioManager.unregisterEvent(hEvent1);
    SetEvent(hEvent2);
    ioManager.dispatch();
    CloseHandle(hEvent1);
    CloseHandle(hEvent2);
    MORDOR_ASSERT(!fired1);
    MORDOR_ASSERT(fired2);
}

MORDOR_UNITTEST(IOManager, waitBlockMultipleFire)
{
    HANDLE hEvent1 = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hEvent1)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
    HANDLE hEvent2 = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hEvent2) {
        CloseHandle(hEvent1);
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
    }
    HANDLE hEvent3 = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hEvent3) {
        CloseHandle(hEvent1);
        CloseHandle(hEvent2);
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
    }
    bool fired2 = false, fired3 = false;
    IOManager ioManager;
    ioManager.registerEvent(hEvent1, std::bind(&signalEvent,
        hEvent3, 0));
    ioManager.registerEvent(hEvent2, std::bind(&handleEvent,
        std::ref(fired2)));
    ioManager.registerEvent(hEvent3, std::bind(&unregisterEvent,
        std::ref(ioManager), hEvent2, std::ref(fired3)));
    SetEvent(hEvent1);
    ioManager.dispatch();
    CloseHandle(hEvent1);
    CloseHandle(hEvent2);
    CloseHandle(hEvent3);
    MORDOR_ASSERT(!fired2);
    MORDOR_ASSERT(fired3);
}

MORDOR_UNITTEST(IOManager, waitBlockDuplicate)
{
    HANDLE hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hEvent)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
    bool fired1 = false, fired2 = false;
    IOManager ioManager;
    ioManager.registerEvent(hEvent, std::bind(&handleEvent,
        std::ref(fired1)));
    ioManager.registerEvent(hEvent, std::bind(&handleEvent,
        std::ref(fired2)));
    SetEvent(hEvent);
    ioManager.dispatch();
    MORDOR_ASSERT(fired1);
    MORDOR_ASSERT(fired2);
    CloseHandle(hEvent);
}

MORDOR_UNITTEST(IOManager, waitBlockDuplicateUnregister)
{
    HANDLE hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hEvent)
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API("CreateEventW");
    bool fired1 = false, fired2 = false;
    IOManager ioManager;
    ioManager.registerEvent(hEvent, std::bind(&handleEvent,
        std::ref(fired1)));
    ioManager.registerEvent(hEvent, std::bind(&handleEvent,
        std::ref(fired2)));
    MORDOR_TEST_ASSERT_EQUAL(ioManager.unregisterEvent(hEvent), 2u);
    ioManager.dispatch();
    MORDOR_ASSERT(!fired1);
    MORDOR_ASSERT(!fired2);
    CloseHandle(hEvent);
}

#endif

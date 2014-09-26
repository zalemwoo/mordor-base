#ifndef __MORDOR_PARALLEL_H__
#define __MORDOR_PARALLEL_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <vector>
#include <mutex>

#include <boost/bind.hpp>
#include <boost/function.hpp>

#include "atomic.h"
#include "fiber.h"
#include "log.h"
#include "scheduler.h"

namespace Mordor {

/// @defgroup parallel_do
/// @brief Execute multiple functors in parallel
///
/// Execute multiple functors in parallel by scheduling them all on the current
/// Scheduler.  Concurrency is achieved either because the Scheduler is running
/// on multiple threads, or because the functors will yield to the Scheduler
/// during execution, instead of blocking.
///
/// If there is no Scheduler associated with the current thread, the functors
/// are simply executed sequentially.
///
/// If any of the functors throw an uncaught exception, the first uncaught
/// exception is rethrown to the caller.

/// @ingroup parallel_do
/// @param dgs The functors to execute
/// @param parallelism How many @p dgs could be executed in parallel at most
/// @note  By default, all the @p dgs would be scheduled together and run with
///        whatever concurrency is available from the Scheduler.
///        If @p parallelism > 0, only at most @p parallelism @p dgs could be
///        scheduled into Scheduler with later dgs not invoked until earlier
///        dgs have completed.
void
parallel_do(const std::vector<boost::function<void ()> > &dgs, int parallelism = -1);
/// @ingroup parallel_do
/// @param dgs The functors to execute
/// @param fibers The Fibers to use to execute the functors
/// @param parallelism How many @p fibers could be executed in parallel at most
/// @pre dgs.size() <= fibers.size()
void
parallel_do(const std::vector<boost::function<void ()> > &dgs,
            std::vector<Fiber::ptr> &fibers,
            int parallelism = -1);

namespace Detail {

template<class Iterator, class Functor>
static
void
parallel_foreach_impl(Iterator &begin, Iterator &end, Functor &functor,
                      std::mutex &mutex, boost::exception_ptr &exception,
                      Scheduler *scheduler, Fiber::ptr caller, int &count)
{
    while (true) {
        try {
            Iterator it;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (begin == end || exception)
                    break;
                it = begin++;
            }
            functor(*it);
        } catch (boost::exception &ex) {
            removeTopFrames(ex);
            std::lock_guard<std::mutex> lock(mutex);
            exception = boost::current_exception();
            break;
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex);
            exception = boost::current_exception();
            break;
        }
    }
    // Don't want to own the mutex here, because another thread could pick up
    // caller immediately, and return from parallel_for before this thread has
    // a chance to unlock it
    if (atomicDecrement(count) == 0)
        scheduler->schedule(caller);
}

Logger::ptr getLogger();

}

/// Execute a functor for multiple objects in parallel

/// @ingroup parallel_do
/// Execute a functor for multiple objects in parallel by scheduling up to
/// parallelism at a time on the current Scheduler.  Concurrency is achieved
/// either because the Scheduler is running on multiple threads, or because the
/// the functor yields to the Scheduler during execution, instead of blocking.
/// @tparam Iterator The type of the iterator for the collection
/// @tparam T The type returned by dereferencing the Iterator, and then passed
/// to the functor
/// @param begin The beginning of the collection
/// @param end The end of the collection
/// @param dg The functor to be passed each object in the collection
/// @param parallelism How many objects to Schedule in parallel
template<class Iterator, class Functor>
void
parallel_foreach(Iterator begin, Iterator end, Functor functor,
    int parallelism = -1)
{
    if (parallelism == -1)
        parallelism = 4;
    Scheduler *scheduler = Scheduler::getThis();

    if (parallelism == 1 || !scheduler) {
        MORDOR_LOG_DEBUG(Detail::getLogger())
            << " running parallel_for sequentially";
        while (begin != end)
            functor(*begin++);
        return;
    }

    std::mutex mutex;
    boost::exception_ptr exception;
    MORDOR_LOG_DEBUG(Detail::getLogger()) << " running parallel_for with "
        << parallelism << " fibers";
    int count = parallelism;
    for (int i = 0; i < parallelism; ++i) {
        scheduler->schedule(boost::bind(
            &Detail::parallel_foreach_impl<Iterator, Functor>,
            boost::ref(begin), boost::ref(end), boost::ref(functor),
            boost::ref(mutex), boost::ref(exception), scheduler,
            Fiber::getThis(), boost::ref(count)));
    }
    Scheduler::yieldTo();

    if (exception)
        Mordor::rethrow_exception(exception);
}

}

#endif

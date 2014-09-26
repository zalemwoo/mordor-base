#ifndef __MORDOR_COROUTINE_H__
#define __MORDOR_COROUTINE_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "util.h"
#include "exception.h"
#include "fiber.h"

namespace Mordor {

struct DummyVoid;

struct CoroutineAbortedException : virtual OperationAbortedException {};

template <class Result, class Arg = DummyVoid>
class Coroutine : Mordor::noncopyable
{
public:
    Coroutine()
    {
        m_fiber = Fiber::ptr(new Fiber(std::bind(&Coroutine::run, this)));
    }

    Coroutine(std::function<void (Coroutine &, Arg)> dg)
        : m_dg(dg)
    {
        m_fiber = Fiber::ptr(new Fiber(std::bind(&Coroutine::run, this)));
    }

    ~Coroutine()
    {
        reset();
    }

    void reset()
    {
        if (m_fiber->state() == Fiber::HOLD) {
            try {
                throw boost::enable_current_exception(CoroutineAbortedException());
            } catch (...) {
                m_fiber->inject(boost::current_exception());
            }
        }
        m_fiber->reset(std::bind(&Coroutine::run, this));
    }

    void reset(std::function<void (Coroutine &, Arg)> dg)
    {
        reset();
        m_dg = dg;
    }

    Result call(const Arg &arg)
    {
        m_arg = arg;
        m_fiber->call();
        return m_result;
    }

    Arg yield(const Result &result)
    {
        m_result = result;
        Fiber::yield();
        return m_arg;
    }

    Fiber::State state() const
    {
        return m_fiber->state();
    }

private:
    void run()
    {
        try {
            m_dg(*this, m_arg);
            m_result = Result();
        } catch (CoroutineAbortedException &) {
        }
    }

private:
    std::function<void (Coroutine &, Arg)> m_dg;
    Result m_result;
    Arg m_arg;
    Fiber::ptr m_fiber;
};


template <class Result>
class Coroutine<Result, DummyVoid> : Mordor::noncopyable
{
public:
    Coroutine()
    {
        m_fiber = Fiber::ptr(new Fiber(std::bind(&Coroutine::run, this)));
    }

    Coroutine(std::function<void (Coroutine &)> dg)
        : m_dg(dg)
    {
        m_fiber = Fiber::ptr(new Fiber(std::bind(&Coroutine::run, this)));
    }

    ~Coroutine()
    {
        reset();
    }

    void reset()
    {
        if (m_fiber->state() == Fiber::HOLD) {
            try {
                throw boost::enable_current_exception(CoroutineAbortedException());
            } catch (...) {
                m_fiber->inject(boost::current_exception());
            }
        }
        m_fiber->reset(std::bind(&Coroutine::run, this));
    }

    void reset(std::function<void (Coroutine &)> dg)
    {
        reset();
        m_dg = dg;
    }

    Result call()
    {
        m_fiber->call();
        return m_result;
    }

    void yield(const Result &result)
    {
        m_result = result;
        Fiber::yield();
    }

    Fiber::State state() const
    {
        return m_fiber->state();
    }

private:
    void run()
    {
        try {
            m_dg(*this);
            m_result = Result();
        } catch (CoroutineAbortedException &) {
        }
    }

private:
    std::function<void (Coroutine &)> m_dg;
    Result m_result;
    Fiber::ptr m_fiber;
};

template <class Arg>
class Coroutine<void, Arg> : Mordor::noncopyable
{
public:
    Coroutine()
    {
        m_fiber = Fiber::ptr(new Fiber(std::bind(&Coroutine::run, this)));
    }

    Coroutine(std::function<void (Coroutine &, Arg)> dg)
        : m_dg(dg)
    {
        m_fiber = Fiber::ptr(new Fiber(std::bind(&Coroutine::run, this)));
    }

    ~Coroutine()
    {
        reset();
    }

    void reset()
    {
        if (m_fiber->state() == Fiber::HOLD) {
            try {
                throw boost::enable_current_exception(CoroutineAbortedException());
            } catch (...) {
                m_fiber->inject(boost::current_exception());
            }
        }
        m_fiber->reset(std::bind(&Coroutine::run, this));
    }

    void reset(std::function<void (Coroutine &, Arg)> dg)
    {
        reset();
        m_dg = dg;
    }

    void call(const Arg &arg)
    {
        m_arg = arg;
        m_fiber->call();
    }

    Arg yield()
    {
        Fiber::yield();
        return m_arg;
    }

    Fiber::State state() const
    {
        return m_fiber->state();
    }

private:
    void run()
    {
        try {
            m_dg(*this, m_arg);
        } catch (CoroutineAbortedException &) {
        }
    }

private:
    std::function<void (Coroutine &, Arg)> m_dg;
    Arg m_arg;
    Fiber::ptr m_fiber;
};

}

#endif

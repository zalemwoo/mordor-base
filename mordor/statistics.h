#ifndef __MORDOR_STATISTICS_H__
#define __MORDOR_STATISTICS_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "predef.h"

#include <limits>
#include <ostream>

#include "assert.h"
#include "atomic.h"
#include "timer.h"

namespace Mordor {

struct Statistic
{
    Statistic(const char *_units = NULL) : units(_units) {}
    virtual ~Statistic() {}
    const char *units;

    void dump(std::ostream &);
    virtual void reset() = 0;
    virtual std::ostream &serialize(std::ostream &os) const { return os; }

    virtual const Statistic *begin() const { return NULL; }
    virtual const Statistic *next(const Statistic *) const { MORDOR_NOTREACHED(); return NULL;}
};

inline std::ostream &operator <<(std::ostream &os, const Statistic &stat)
{ return stat.serialize(os); }

template <class T>
struct CountStatistic : Statistic
{
    typedef T value_type;

    CountStatistic(const char *units = NULL)
        : Statistic(units),
          count(T())
    {}

    volatile value_type count;

    void reset() { count = T(); }

    std::ostream &serialize(std::ostream &os) const
    { return os << count; }

    void increment() { atomicIncrement(count); }
    void decrement() { atomicDecrement(count); }
    void add(value_type value) { atomicAdd(count, value); }
    void merge(const CountStatistic<T> &stat) { add(stat.count); }
};

template <class T>
struct SumStatistic : Statistic
{
    typedef T value_type;

    SumStatistic(const char *units = NULL)
        : Statistic(units),
          sum(T())
    {}

    volatile value_type sum;

    void reset() { sum = T(); }

    std::ostream &serialize(std::ostream &os) const
    { return os << sum; }

    void add(value_type value) { atomicAdd(sum, value); }
    void merge(const SumStatistic<T> &stat) { add(stat.sum); }
};

template <class T>
struct MinStatistic : Statistic
{
    typedef T value_type;

    MinStatistic(const char *units = NULL)
        : Statistic(units),
        minimum((std::numeric_limits<T>::max)())
    {}

    volatile value_type minimum;

    void reset() { minimum = (std::numeric_limits<T>::max)(); }

    std::ostream &serialize(std::ostream &os) const
    { return os << minimum; }

    void update(value_type value)
    {
        value_type oldval = minimum;
        do {
            if (oldval < value)
                break;
        } while (value != (oldval = atomicCompareAndSwap(minimum, value, oldval)));
    }

    void merge(const MinStatistic<T> &stat) { update(stat.minimum); }
};


template <class T>
struct MaxStatistic : Statistic
{
    typedef T value_type;

    MaxStatistic(const char *units = NULL)
        : Statistic(units),
          maximum((std::numeric_limits<T>::min)())
    {}
    volatile value_type maximum;

    void reset() { maximum = (std::numeric_limits<T>::min)(); }

    std::ostream &serialize(std::ostream &os) const
    { return os << maximum; }

    void update(value_type value)
    {
        value_type oldval = maximum;
        do {
            if (oldval > value)
                break;
        } while (value != (oldval = atomicCompareAndSwap(maximum, value, oldval)));
    }

    void merge(const MaxStatistic<T> &stat) { update(stat.maximum); }
};

template <class T>
struct AverageStatistic : Statistic
{
    typedef T value_type;

    AverageStatistic(const char *sumunits = NULL, const char *countunits = NULL)
        : Statistic(sumunits),
          count(countunits),
          sum(sumunits)
    {}

    CountStatistic<T> count;
    SumStatistic<T> sum;

    void reset()
    {
        count.reset();
        sum.reset();
    }

    void update(T value)
    {
        count.increment();
        sum.add(value);
    }

    std::ostream &serialize(std::ostream &os) const
    {
        T localcount = count.count;
        T localsum = sum.sum;
        if (localcount)
            os << localsum / localcount;
        else
            os << localcount;
        return os;
    }

    const Statistic *begin() const { return &count; }
    const Statistic *next(const Statistic *previous) const
    {
        if (previous == &count)
            return &sum;
        else if (previous == &sum)
            return NULL;
        MORDOR_NOTREACHED();
    }

    void merge(const AverageStatistic<T> &stat)
    {
        count.merge(stat.count);
        sum.merge(stat.sum);
    }
};

template <class T>
struct AverageMinMaxStatistic : AverageStatistic<T>
{
    AverageMinMaxStatistic(const char *sumunits = NULL, const char *countunits = NULL)
        : AverageStatistic<T>(sumunits, countunits),
          minimum(sumunits),
          maximum(sumunits)
    {}

    MinStatistic<T> minimum;
    MaxStatistic<T> maximum;

    void reset()
    {
        AverageStatistic<T>::reset();
        minimum.reset();
        maximum.reset();
    }

    void update(T value)
    {
        AverageStatistic<T>::update(value);
        minimum.update(value);
        maximum.update(value);
    }

    const Statistic *begin() const { return &minimum; }
    const Statistic *next(const Statistic *previous) const
    {
        if (previous == &minimum)
            return &maximum;
        else if (previous == &maximum)
            return AverageStatistic<T>::begin();
        else
            return AverageStatistic<T>::next(previous);
    }

    void merge(const AverageMinMaxStatistic<T> &stat)
    {
        minimum.merge(stat.minimum);
        maximum.merge(stat.maximum);
        AverageStatistic<T>::merge(stat);
    }
};

template <class T, class U>
struct ThroughputStatistic : Statistic
{
    AverageMinMaxStatistic<T> size;
    AverageMinMaxStatistic<U> time;

    ThroughputStatistic(const char *sumunits = NULL, const char *timeunits = NULL, const char *countunits = NULL)
        : size(sumunits, countunits),
          time(timeunits, countunits)
    {
        m_units = sumunits;
        m_units.append(1, '/');
        m_units.append(timeunits);
        units = m_units.c_str();
    }
    ThroughputStatistic(const ThroughputStatistic &copy)
      : size(copy.size),
        time(copy.time),
        m_units(copy.m_units)
    {
        units = m_units.c_str();
    }

    void reset()
    {
        size.reset();
        time.reset();
    }

    void update(T sizevalue, U timevalue)
    {
        size.update(sizevalue);
        time.update(timevalue);
    }

    const Statistic *begin() const { return &size; }
    const Statistic *next(const Statistic *previous) const
    {
        if (previous == &size)
            return &time;
        else
            return NULL;
    }

    std::ostream &serialize(std::ostream &os) const
    {
        T localsize = size.sum.sum;
        U localtime = time.sum.sum;
        if (localtime)
            os << (double)localsize / localtime;
        else
            os << localtime;
        return os;
    }

private:
    std::string m_units;
};


class Statistics
{
public:
    typedef std::map<std::string,
        std::pair<std::string, std::shared_ptr<Statistic> > >
        StatisticsCache;

    struct StatisticsDumper {};

private:
    Statistics();

public:
    template <class T>
    static T *lookup(const std::string &name)
    {
        StatisticsCache::const_iterator it = statistics().find(name);
        if (it == statistics().end()) {
            return NULL;
        } else {
            T *stat = dynamic_cast<T *>(it->second.second.get());
            MORDOR_ASSERT(stat);
            return stat;
        }
    }

    template <class T>
    static T &registerStatistic(const std::string &name, const T& t,
        const std::string &description = "")
    {
        MORDOR_ASSERT(statistics().find(name) == statistics().end());
        std::shared_ptr<T> ptr(new T(t));
        stats()[name] = std::make_pair(description, ptr);
        return *ptr;
    }

    static Statistic *lookup(const std::string &name);
    static std::ostream &dump(std::ostream &os);
    static StatisticsDumper dump() { return StatisticsDumper(); }

    static const StatisticsCache &statistics()
    {
        return stats();
    }

private:
    static StatisticsCache &stats()
    {
        static StatisticsCache stats;
        return stats;
    }
};

inline std::ostream &operator <<(std::ostream &os,
    const Statistics::StatisticsDumper &d)
{ return Statistics::dump(os); }

template <class T>
class TimeStatistic
{
public:
    TimeStatistic(T &stat)
        : m_stat(&stat),
          m_start(TimerManager::now())
    {}
    ~TimeStatistic()
    { finish(); }

    void finish()
    {
        if (m_stat) {
            m_stat->update((typename T::value_type)(TimerManager::now() - m_start));
            m_stat = NULL;
        }
    }

private:
    T *m_stat;
    unsigned long long m_start;
};

}

#endif

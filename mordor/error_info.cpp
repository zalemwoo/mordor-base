
#include "scheduler.h"

template <typename Exception>
const char* Mordor::ErrorInfo<Exception>::what() const throw()
{
    ::std::ostringstream oss;
    Mordor::tid_t tid = Mordor::gettid();
    Mordor::tid_t root_tid = 0;
    Mordor::Scheduler* sched = Mordor::Scheduler::getThis();
    if(sched)
        root_tid = sched->rootThreadId();

    oss << "==> Exception : " << exception_.what() << ::std::endl
        << "    Error     : " << "\"" << strerror((int)error_) << "(" << (int)error_ << ")\""
        << ", TID: " << tid << ", Root TID: " << root_tid << ::std::endl
        << "    File      : " << file_ << "(" << line_ << ") :: " << func_ << "()" << ::std::endl;
    oss << Mordor::dump_backtrace();
    err_str_ = oss.str();
    return err_str_.c_str();
}

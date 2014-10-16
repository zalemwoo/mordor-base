#ifndef __MORDOR_EXCEPTION_H__
#define __MORDOR_EXCEPTION_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "predef.h"

#include <string.h>

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <exception>

#include "version.h"
#include "type_name.h"

#ifdef WINDOWS
#include <windows.h>
#include <winerror.h>
#else
#include <errno.h>
#endif

namespace Mordor {

#ifdef POSIX
std::string dump_backtrace();
#else
#define dump_backtrace Mordor::to_string(Mordor::backtrace(1))
#endif

std::string to_string( const std::vector<void *> bt );
std::vector<void *> backtrace(int framesToSkip = 0);

template <typename T>
struct ErrorInfo : public std::exception
{
    ErrorInfo(T const & exception, const char* file, int line)
            :file_(file), line_(line), exception_(exception), error_(0){};

    virtual ~ErrorInfo(){}

    void setError(error_t const & error) {
        error_ = error;
    }

    virtual const char* what() const throw() override
    {
        std::ostringstream oss;
        oss <<  "==> Exception: " << exception_.what()
                << ", File: " << file_ << "(" << line_
                << "), Error: " << "\"" << strerror((int)error_) << "(" << (int)error_ << ")\"" << std::endl;
        oss << Mordor::dump_backtrace();
        oss << "<==" << std::endl;
        err_str_ = oss.str();
        return err_str_.c_str();
    }

private:
    const char* file_;
    int line_;
    T exception_;
    error_t error_;
    mutable std::string err_str_;
};

template <typename T>
inline
const ErrorInfo<T>&
operator<<(const ErrorInfo<T>& x, error_t const & error ){
    const_cast<ErrorInfo<T>&>(x).setError(error);
    return x;
}

template <typename T>
inline
std::ostream &operator <<(std::ostream &os, const ErrorInfo<T>& ex)
{
    return os << "Exception:: " << ex.what();
}

#define MORDOR_THROW_EXCEPTION(x)  throw ::Mordor::ErrorInfo<decltype(x)>(x, __FILE__, __LINE__)
#define MORDOR_THROW_EXCEPTION_WITH_ERROR(x)  MORDOR_THROW_EXCEPTION((x)) << error

void rethrow_exception(std::exception_ptr const & ep);

struct Exception : virtual std::exception {
    virtual const char* what() const throw() override
    {
        return Mordor::type_name(*this).c_str();
    }
};

struct StreamException : virtual Exception {};
struct UnexpectedEofException : virtual StreamException {};
struct WriteBeyondEofException : virtual StreamException {};
struct BufferOverflowException : virtual StreamException {};

struct NativeException : virtual Exception {};

#ifdef WINDOWS
// error_t is a struct so that operator <<(ostream, error_t) is not ambiguous
struct error_t
{
    error_t(DWORD v = 0u) : value(v) {}
    operator DWORD() { return value; }

    DWORD value;
};
#else
struct error_t
{
    error_t(int v = 0) : value(v) {}
    operator int() { return value; }

    int value;
};
#endif

struct OperationNotSupportedException : virtual NativeException {};
struct FileNotFoundException : virtual NativeException {};
struct AccessDeniedException : virtual NativeException {};
struct BadHandleException : virtual NativeException {};
struct OperationAbortedException : virtual NativeException {};
struct BrokenPipeException : virtual NativeException {};
struct SharingViolation : virtual NativeException {};
struct UnresolvablePathException : virtual NativeException {};
struct IsDirectoryException : virtual UnresolvablePathException {};
struct IsNotDirectoryException : virtual UnresolvablePathException {};
struct TooManySymbolicLinksException : virtual UnresolvablePathException {};
struct OutOfDiskSpaceException : virtual NativeException {};
struct InvalidUnicodeException : virtual NativeException {};

error_t lastError();
void lastError(error_t error);

std::ostream &operator <<(std::ostream &os, error_t error);

void throwExceptionFromLastError(error_t lastError);

#define MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR()                                \
        ::Mordor::throwExceptionFromLastError(::Mordor::lastError())

#define MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API(api)                         \
        MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR()

#define MORDOR_THROW_EXCEPTION_FROM_ERROR(error)                                \
        ::Mordor::throwExceptionFromLastError(error)

#define MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, api)                       \
        ::Mordor::throwExceptionFromLastError(error)
}

#endif

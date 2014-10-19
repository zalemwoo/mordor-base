#ifndef __MORDOR_EXCEPTION_H__
#define __MORDOR_EXCEPTION_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "predef.h"

#include <string.h>

#include <sstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <exception>

#include "version.h"
#include "type_name.h"
#include "thread.h"

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

#ifdef WINDOWS
// error_t is a struct so that operator <<(ostream, error_t) is not ambiguous
struct error_t
{
    error_t(DWORD v = 0u) : value(v) {}
    operator DWORD() const { return value; }

    DWORD value;
};
#else
struct error_t
{
    error_t(int v = 0) : value(v) {}
    operator int() const { return value; }

    int value;
};
#endif

struct ErrorInfoBase : virtual std::exception
{
};

template <typename Exception>
struct ErrorInfo : public ErrorInfoBase
{
    typedef  Exception exception_type;
    ErrorInfo(Exception const & exception, const char* file, int line, const char* func)
            : exception_(exception), file_(file), line_(line), func_(func), error_(0){};

    void setError(error_t const & error) {
        error_ = error;
    }

    virtual const char* what() const throw() override;

    Exception exception_;
private:
    const char* file_;
    int line_;
    const char* func_;
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

#define MORDOR_THROW_EXCEPTION_FL(x, file, line, func) \
    do{                                                                             \
        try{                                                                        \
            throw ::Mordor::ErrorInfo<decltype(x)>(x, file, line, func);                 \
        }catch(const ::Mordor::ErrorInfo<decltype(x)> & ex){                      \
            std::cerr << ex.what() << std::flush;                                    \
            std::rethrow_exception(std::make_exception_ptr(ex.exception_));          \
        }                                                                            \
    }while(0)

#define MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(x, file, line, func)                           \
    do{                                                                             \
        try{                                                                          \
            throw (::Mordor::ErrorInfo<decltype(x)>(x, file, line, func) << error);       \
        }catch(const ::Mordor::ErrorInfo<decltype(x)> & ex){                        \
            std::cerr << ex.what() << std::flush;                                      \
            std::rethrow_exception(std::make_exception_ptr(ex.exception_));          \
        }                                                                            \
    }while(0)

#define MORDOR_THROW_EXCEPTION(x)  MORDOR_THROW_EXCEPTION_FL(x, __FILE__, __LINE__, __func__)
#define MORDOR_THROW_EXCEPTION_WITH_ERROR(x) MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(x, __FILE__, __LINE__, __func__)

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

void throwExceptionFromLastError(error_t lastError, const char* file, int line, const char* func);

#define MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR()                                \
        ::Mordor::throwExceptionFromLastError(::Mordor::lastError(), __FILE__, __LINE__, __func__);

#define MORDOR_THROW_EXCEPTION_FROM_ERROR(error)                                \
        ::Mordor::throwExceptionFromLastError(error, __FILE__, __LINE__, __func__);

#define MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API(api)                         \
    do{                                                                             \
        try{                                                                         \
            MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR();                                \
        }catch(...){                                                                \
            std::cerr << std::endl << "==> Exception API( " << api << " ) <==\n";        \
            throw;                                                                  \
        }                                                                            \
    }while(0)

#define MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, api)                       \
    do{                                                                             \
        try{                                                                     \
            MORDOR_THROW_EXCEPTION_FROM_ERROR(error);                            \
        }catch(...){                                        \
            std::cerr << std::endl << "==> Exception API( " << api << " ) <==\n";        \
            throw;                                                              \
        }                                                                        \
    }while(0)

} // namespace Mordor

extern template struct Mordor::ErrorInfo<std::runtime_error>;
extern template struct Mordor::ErrorInfo<std::bad_alloc>;
extern template struct Mordor::ErrorInfo<std::out_of_range>;
extern template struct Mordor::ErrorInfo<std::invalid_argument>;
extern template struct Mordor::ErrorInfo<Mordor::StreamException>;
extern template struct Mordor::ErrorInfo<Mordor::UnexpectedEofException>;
extern template struct Mordor::ErrorInfo<Mordor::WriteBeyondEofException>;
extern template struct Mordor::ErrorInfo<Mordor::BufferOverflowException>;
extern template struct Mordor::ErrorInfo<Mordor::NativeException>;
extern template struct Mordor::ErrorInfo<Mordor::OperationNotSupportedException>;
extern template struct Mordor::ErrorInfo<Mordor::FileNotFoundException>;
extern template struct Mordor::ErrorInfo<Mordor::AccessDeniedException>;
extern template struct Mordor::ErrorInfo<Mordor::BadHandleException>;
extern template struct Mordor::ErrorInfo<Mordor::OperationAbortedException>;
extern template struct Mordor::ErrorInfo<Mordor::BrokenPipeException>;
extern template struct Mordor::ErrorInfo<Mordor::SharingViolation>;
extern template struct Mordor::ErrorInfo<Mordor::UnresolvablePathException>;
extern template struct Mordor::ErrorInfo<Mordor::IsDirectoryException>;
extern template struct Mordor::ErrorInfo<Mordor::IsNotDirectoryException>;
extern template struct Mordor::ErrorInfo<Mordor::TooManySymbolicLinksException>;
extern template struct Mordor::ErrorInfo<Mordor::OutOfDiskSpaceException>;
extern template struct Mordor::ErrorInfo<Mordor::InvalidUnicodeException>;

#endif

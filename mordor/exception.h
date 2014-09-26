#ifndef __MORDOR_EXCEPTION_H__
#define __MORDOR_EXCEPTION_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "predef.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <exception>

#include <boost/exception/all.hpp>

#include "version.h"

#ifdef WINDOWS
#include <windows.h>
#include <winerror.h>
#else
#include <errno.h>
#endif

namespace Mordor {

typedef boost::error_info<struct tag_backtrace, std::vector<void *> > errinfo_backtrace;
#ifdef WINDOWS
typedef boost::error_info<struct tag_lasterror, DWORD> errinfo_lasterror;
std::string to_string( errinfo_lasterror const & e );
typedef errinfo_lasterror errinfo_nativeerror;
#else
typedef boost::errinfo_errno errinfo_nativeerror;
#endif

std::string to_string( const std::vector<void *> bt );
std::string to_string( errinfo_backtrace const &bt );

std::vector<void *> backtrace(int framesToSkip = 0);
void removeTopFrames(boost::exception &ex, int framesToSkip = 0);

#define MORDOR_THROW_EXCEPTION(x)                                               \
    throw ::boost::enable_current_exception(::boost::enable_error_info(x))      \
        << ::boost::throw_function(BOOST_CURRENT_FUNCTION)                      \
        << ::boost::throw_file(__FILE__)                                        \
        << ::boost::throw_line((int)__LINE__)                                   \
        << ::Mordor::errinfo_backtrace(::Mordor::backtrace())

void rethrow_exception(boost::exception_ptr const & ep);

struct Exception : virtual boost::exception, virtual std::exception {};

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
    try {                                                                       \
        ::Mordor::throwExceptionFromLastError(::Mordor::lastError());           \
    } catch (::boost::exception &ex) {                                          \
        ex << ::boost::throw_function(BOOST_CURRENT_FUNCTION)                   \
            << ::boost::throw_file(__FILE__)                                    \
            << ::boost::throw_line((int)__LINE__)                               \
            << ::Mordor::errinfo_backtrace(::Mordor::backtrace());              \
        throw;                                                                  \
    }

#define MORDOR_THROW_EXCEPTION_FROM_LAST_ERROR_API(api)                         \
    try {                                                                       \
        ::Mordor::throwExceptionFromLastError(::Mordor::lastError());           \
    } catch (::boost::exception &ex) {                                          \
        ex << ::boost::throw_function(BOOST_CURRENT_FUNCTION)                   \
            << ::boost::throw_file(__FILE__)                                    \
            << ::boost::throw_line((int)__LINE__)                               \
            << ::boost::errinfo_api_function(api)                               \
            << ::Mordor::errinfo_backtrace(::Mordor::backtrace());              \
        throw;                                                                  \
    }

#define MORDOR_THROW_EXCEPTION_FROM_ERROR(error)                                \
    try {                                                                       \
        ::Mordor::throwExceptionFromLastError(error);                           \
    } catch (::boost::exception &ex) {                                          \
        ex << ::boost::throw_function(BOOST_CURRENT_FUNCTION)                   \
            << ::boost::throw_file(__FILE__)                                    \
            << ::boost::throw_line((int)__LINE__)                               \
            << ::Mordor::errinfo_backtrace(::Mordor::backtrace());              \
        throw;                                                                  \
    }

#define MORDOR_THROW_EXCEPTION_FROM_ERROR_API(error, api)                       \
    try {                                                                       \
        ::Mordor::throwExceptionFromLastError(error);                           \
    } catch (::boost::exception &ex) {                                          \
        ex << ::boost::throw_function(BOOST_CURRENT_FUNCTION)                   \
            << ::boost::throw_file(__FILE__)                                    \
            << ::boost::throw_line((int)__LINE__)                               \
            << ::boost::errinfo_api_function(api)                               \
            << ::Mordor::errinfo_backtrace(::Mordor::backtrace());              \
        throw;                                                                  \
    }

}

#endif

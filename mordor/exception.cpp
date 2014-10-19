// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/exception.h"
#include "mordor/assert.h"

#include <iomanip> // for std::setw

#ifdef POSIX
#include <cxxabi.h>
#endif

#ifdef WINDOWS
#include <dbghelp.h>

#include "runtime_linking.h"

#pragma comment(lib, "dbghelp")
#else
#include <errno.h>
#include <execinfo.h>
#include <netdb.h>
#include <string.h>
#endif

#include <mutex>

#include "socket.h"

namespace Mordor {

#ifdef WINDOWS
static BOOL g_useSymbols;

namespace {

static struct Initializer {
    Initializer()
    {
        SymSetOptions(SYMOPT_DEFERRED_LOADS |
            SYMOPT_FAIL_CRITICAL_ERRORS |
            SYMOPT_LOAD_LINES |
            SYMOPT_NO_PROMPTS);
        g_useSymbols = SymInitialize(GetCurrentProcess(), NULL, TRUE);
    }

    ~Initializer()
    {
        if (g_useSymbols)
            SymCleanup(GetCurrentProcess());
    }
} g_init;

}
#endif

#ifdef POSIX
std::string dump_backtrace()
{
    std::ostringstream os;
    void* trace[100];
    int size = ::backtrace(trace, 100);
    char** symbols = ::backtrace_symbols(trace, size);
    os << "=== C stack trace ===============================\n";
    if (size == 0) {
        os << "(empty)\n";
    } else if (symbols == NULL) {
        os << "(no symbols)\n";
    } else {
        for (int i = 1; i < size; ++i) {
            os << std::fixed << std::setw(2) << std::setfill('0') << i << " : ";
            char mangled[201];
            if (sscanf(symbols[i], "%*[^(]%*[(]%200[^)+]", mangled) == 1) {  // NOLINT
                int status;
                size_t length;
                char* demangled = abi::__cxa_demangle(mangled, NULL, &length, &status);
                os << ((demangled != NULL) ? demangled : mangled);
                os << std::endl;
                free(demangled);
            } else {
                os << "??" << std::endl;
            }
        }
    }
    free(symbols);
    return os.str();
}
#endif

std::string to_string(const std::vector<void *> backtrace)
{
#ifdef WINDOWS
    static std::mutex s_mutex;
    std::lock_guard<std::mutex> lock(s_mutex);
#endif
    std::ostringstream os;
#ifdef POSIX
    std::shared_ptr<char *> symbols(backtrace_symbols(&backtrace[0],
        backtrace.size()), &free);
#endif
    for (size_t i = 0; i < backtrace.size(); ++i) {
        if (i != 0)
            os << std::endl;
#ifdef WINDOWS
        os << backtrace[i];
        if (g_useSymbols) {
            char buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME - 1];
            SYMBOL_INFO *symbol = (SYMBOL_INFO*)buf;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;
            DWORD64 displacement64 = 0;
            if (pSymFromAddr(GetCurrentProcess(), (DWORD64)backtrace[i],
                &displacement64, symbol)) {
                os << ": " << symbol->Name << "+" << displacement64;
            }
            IMAGEHLP_LINE64 line;
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD displacement = 0;
            if (pSymGetLineFromAddr64(GetCurrentProcess(),
                (DWORD64)backtrace[i], &displacement, &line)) {
                os << ": " << line.FileName << "(" << line.LineNumber << ")+"
                    << displacement;
            }
        }
#else
        if (symbols)
            os << symbols.get()[i];
        else
            os << backtrace[i];
#endif
    }
    return os.str();
}

//std::string to_string( errinfo_gaierror const &e)
//{
//    std::ostringstream os;
//    os << e.value() << ", \"" << gai_strerror(e.value()) << "\"";
//    return os.str();
//}

std::vector<void *> backtrace(int framesToSkip)
{
    std::vector<void *> result;
#ifdef WINDOWS
    result.resize(64);
    WORD count = pRtlCaptureStackBackTrace(1 + framesToSkip, 61 - framesToSkip, &result[0], NULL);
    result.resize(count);
#else
    result.resize(64);
    int count = ::backtrace(&result[0], 64);
    result.resize(count);
    framesToSkip = std::min(count, framesToSkip + 1);
    result.erase(result.begin(), result.begin() + framesToSkip);
#endif
    return result;
}

void rethrow_exception(std::exception_ptr const & ep)
{
    // Take the backtrace from here, to avoid additional frames from the
    // exception handler
    std::vector<void *> bt = backtrace(1);
    try {
        std::rethrow_exception(ep);
    } catch (std::exception &e) {
        throw;
    }
}

#ifdef WINDOWS
#define WSA(error) WSA ## error
#else
#define WSA(error) error
#endif

static void throwSocketException(error_t error, const char* file, int line, const char* func)
{
    switch (error) {
        case WSA(EAFNOSUPPORT):
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(OperationNotSupportedException(), file, line, func);
            break;
        case WSA(EADDRINUSE):
#ifdef WINDOWS
        // WSAEACESS is returned from bind when you set SO_REUSEADDR, and
        // another socket has set SO_EXCLUSIVEADDRUSE
        case WSAEACCES:
        case ERROR_ADDRESS_ALREADY_ASSOCIATED:
#endif
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(AddressInUseException(), file, line, func);
            break;
        case WSA(ECONNABORTED):
#ifdef WINDOWS
        case ERROR_CONNECTION_ABORTED:
#endif
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(ConnectionAbortedException(), file, line, func);
            break;
        case WSA(ECONNRESET):
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(ConnectionResetException(), file, line, func);
            break;
        case WSA(ECONNREFUSED):
#ifdef WINDOWS
        case ERROR_CONNECTION_REFUSED:
#endif
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(ConnectionRefusedException(), file, line, func);
            break;
        case WSA(EHOSTDOWN):
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(HostDownException(), file, line, func);
            break;
        case WSA(EHOSTUNREACH):
#ifdef WINDOWS
        case ERROR_HOST_UNREACHABLE:
#endif
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(HostUnreachableException(), file, line, func);
            break;
        case WSA(ENETDOWN):
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(NetworkDownException(), file, line, func);
            break;
        case WSA(ENETRESET):
#ifdef WINDOWS
        case ERROR_NETNAME_DELETED:
#endif
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(NetworkResetException(), file, line, func);
            break;
        case WSA(ENETUNREACH):
#ifdef WINDOWS
        case ERROR_NETWORK_UNREACHABLE:
#endif
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(NetworkUnreachableException(), file, line, func);
            break;
        case WSA(ETIMEDOUT):
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(TimedOutException(), file, line, func);
            break;
        default:
            break;
    }
}

#ifdef WINDOWS
error_t lastError()
{
    return GetLastError();
}

void lastError(error_t error)
{
    SetLastError(error);
}

std::ostream &operator <<(std::ostream &os, error_t error)
{
    if (error == ERROR_SUCCESS)
        return os << "0, \"The operation completed successfully.\"";
    os << (DWORD)error;
    std::string result;
    char *desc;
    DWORD numChars = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error, 0,
        (char*)&desc, 0, NULL);
    if (numChars > 0) {
        if (desc[numChars - 1] == '\n') {
            desc[numChars - 1] = '\0';
            if (desc[numChars - 2] == '\r')
                desc[numChars - 2] = '\0';
        }
        try {
            os << ", \"" << desc << "\"";
        } catch (...) {
            LocalFree((HANDLE)desc);
            throw;
        }
        LocalFree((HANDLE)desc);
    }
    return os;
}

void throwExceptionFromLastError(error_t error, const char* file, int line, const char* func)
{
    switch (error) {
        case ERROR_INVALID_HANDLE:
        case WSAENOTSOCK:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(BadHandleException(), file, line, func);
            break;
        case ERROR_FILE_NOT_FOUND:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(FileNotFoundException(), file, line, func);
            break;
        case ERROR_ACCESS_DENIED:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(AccessDeniedException(), file, line, func);
            break;
        case ERROR_OPERATION_ABORTED:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(OperationAbortedException(), file, line, func);
            break;
        case ERROR_BROKEN_PIPE:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(UnexpectedEofException(), file, line, func);
            break;
        case WSAESHUTDOWN:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(BrokenPipeException(), file, line, func);
            break;
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(SharingViolation(), file, line, func);
            break;
        case ERROR_CANT_RESOLVE_FILENAME:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(UnresolvablePathException(), file, line, func);
            break;
        case ERROR_DISK_FULL:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(OutOfDiskSpaceException(), file, line, func);
            break;
        case ERROR_NO_UNICODE_TRANSLATION:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(InvalidUnicodeException(), file, line, func);
            break;
        default:
            throwSocketException(error, file, line, func);
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(NativeException(), file, line, func);
    }
}
#else

error_t lastError()
{
    return errno;
}

void lastError(error_t error)
{
    errno = error;
}

std::ostream &operator <<(std::ostream &os, error_t error)
{
    return os << (int)error << ", \"" << strerror(error) << "\"";
}

void throwExceptionFromLastError(error_t error, const char* file, int line, const char* func)
{
    switch (error) {
        case EBADF:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(BadHandleException(), file, line, func);
            break;
        case ENOENT:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(FileNotFoundException(), file, line, func);
            break;
        case EACCES:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(AccessDeniedException(), file, line, func);
            break;
        case ECANCELED:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(OperationAbortedException(), file, line, func);
            break;
        case EPIPE:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(BrokenPipeException(), file, line, func);
            break;
        case EISDIR:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(IsDirectoryException(), file, line, func);
            break;
        case ENOTDIR:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(IsNotDirectoryException(), file, line, func);
            break;
        case ELOOP:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(TooManySymbolicLinksException(), file, line, func);
            break;
        case ENOSPC:
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(OutOfDiskSpaceException(), file, line, func);
            break;
        default:
            throwSocketException(error,file, line, func);
            MORDOR_THROW_EXCEPTION_WITH_ERROR_FL(NativeException(), file, line, func);
    }
}
#endif

} // namespace Mordor

#include "error_info.cpp"

template struct Mordor::ErrorInfo<std::runtime_error>;
template struct Mordor::ErrorInfo<std::bad_alloc>;
template struct Mordor::ErrorInfo<std::out_of_range>;
template struct Mordor::ErrorInfo<std::invalid_argument>;
template struct Mordor::ErrorInfo<Mordor::StreamException>;
template struct Mordor::ErrorInfo<Mordor::UnexpectedEofException>;
template struct Mordor::ErrorInfo<Mordor::WriteBeyondEofException>;
template struct Mordor::ErrorInfo<Mordor::BufferOverflowException>;
template struct Mordor::ErrorInfo<Mordor::NativeException>;
template struct Mordor::ErrorInfo<Mordor::OperationNotSupportedException>;
template struct Mordor::ErrorInfo<Mordor::FileNotFoundException>;
template struct Mordor::ErrorInfo<Mordor::AccessDeniedException>;
template struct Mordor::ErrorInfo<Mordor::BadHandleException>;
template struct Mordor::ErrorInfo<Mordor::OperationAbortedException>;
template struct Mordor::ErrorInfo<Mordor::BrokenPipeException>;
template struct Mordor::ErrorInfo<Mordor::SharingViolation>;
template struct Mordor::ErrorInfo<Mordor::UnresolvablePathException>;
template struct Mordor::ErrorInfo<Mordor::IsDirectoryException>;
template struct Mordor::ErrorInfo<Mordor::IsNotDirectoryException>;
template struct Mordor::ErrorInfo<Mordor::TooManySymbolicLinksException>;
template struct Mordor::ErrorInfo<Mordor::OutOfDiskSpaceException>;
template struct Mordor::ErrorInfo<Mordor::InvalidUnicodeException>;

#ifndef __MORDOR_LOG_FILE_H__
#define __MORDOR_LOG_FILE_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "log.h"
namespace Mordor {

/// A LogSink that appends messages to a file
///
/// The file is opened in append mode, so multiple processes and threads can
/// log to the same file simultaneously, without fear of corrupting each
/// others' messages.  The messages will still be intermingled, but each one
/// will be atomic
class FileLogSink : public LogSink
{
public:
    /// @param file The file to open and log to.  If it does not exist, it is
    /// created.
    FileLogSink(const std::string &file);

    void log(const std::string &logger,
            std::chrono::system_clock::time_point now, unsigned long long elapsed,
        tid_t thread, void *fiber,
        Log::Level level, const std::string &str,
        const char *file, int line);

    std::string file() const { return m_file; }

private:
    std::string m_file;
    std::shared_ptr<Stream> m_stream;
};

}


#endif

// Copyright (c) 2009 - Mozy, Inc.

#include "log_file.h"

#include <iostream>

#include "mordor/config.h"
#include "mordor/streams/file.h"

namespace Mordor {

static void enableFileLogging();

static ConfigVar<std::string>::ptr g_logFile =
    Config::lookup("log.file", std::string(), "Log to file");


namespace {

static struct LogInitializer
{
    LogInitializer()
    {
        g_logFile->monitor(&enableFileLogging);
    }
} g_init;

}

static void enableFileLogging()
{
    static LogSink::ptr fileSink;
    std::string file = g_logFile->val();
    if (fileSink.get() && file.empty()) {
        Log::root()->removeSink(fileSink);
        fileSink.reset();
    } else if (!file.empty()) {
        if (fileSink.get()) {
            if (static_cast<FileLogSink*>(fileSink.get())->file() == file)
                return;
            Log::root()->removeSink(fileSink);
            fileSink.reset();
        }
        fileSink.reset(new FileLogSink(file));
        Log::root()->addSink(fileSink);
    }
}

FileLogSink::FileLogSink(const std::string &file)
{
    m_stream.reset(new FileStream(file, FileStream::APPEND,
        FileStream::OPEN_OR_CREATE));
    m_file = file;
}

void
FileLogSink::log(const std::string &logger,
        std::chrono::system_clock::time_point now, unsigned long long elapsed,
        tid_t thread, void *fiber,
        Log::Level level, const std::string &str,
        const char *file, int line)
{
    std::ostringstream os;
    os << std::chrono::system_clock::to_time_t(now) << " " << elapsed << " " << level << " " << thread << " "
        << fiber << " " << logger << " " << file << ":" << line << " "
        << str << std::endl;
    std::string logline = os.str();
    m_stream->write(logline.c_str(), logline.size());
    m_stream->flush();
}

}

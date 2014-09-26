// Copyright (c) 2009 - Mozy, Inc.

#include <mutex>

#include "pipe.h"

#include "buffer.h"
#include "mordor/assert.h"
#include "mordor/fiber.h"
#include "mordor/scheduler.h"
#include "stream.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:streams:pipe");

class PipeStream : public Stream
{
    friend std::pair<Stream::ptr, Stream::ptr> pipeStream(size_t);
public:
    typedef std::shared_ptr<PipeStream> ptr;
    typedef std::weak_ptr<PipeStream> weak_ptr;

public:
    PipeStream(size_t bufferSize);
    ~PipeStream();

    bool supportsHalfClose() { return true; }
    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }

    void close(CloseType type = BOTH);
    using Stream::read;
    size_t read(Buffer &b, size_t len);
    void cancelRead();
    using Stream::write;
    size_t write(const Buffer &b, size_t len);
    void cancelWrite();
    void flush(bool flushParent = true);

    boost::signals2::connection onRemoteClose(
        const boost::signals2::slot<void ()> &slot);

private:
    PipeStream::weak_ptr m_otherStream;
    std::shared_ptr<std::mutex> m_mutex;
    Buffer m_readBuffer;
    size_t m_bufferSize;
    bool m_cancelledRead, m_cancelledWrite;
    CloseType m_closed, m_otherClosed;
    Scheduler *m_pendingWriterScheduler, *m_pendingReaderScheduler;
    std::shared_ptr<Fiber> m_pendingWriter, m_pendingReader;
    boost::signals2::signal<void ()> m_onRemoteClose;
};

std::pair<Stream::ptr, Stream::ptr> pipeStream(size_t bufferSize)
{
    if (bufferSize == ~0u)
        bufferSize = 65536;
    std::pair<PipeStream::ptr, PipeStream::ptr> result;
    result.first.reset(new PipeStream(bufferSize));
    result.second.reset(new PipeStream(bufferSize));
    MORDOR_LOG_VERBOSE(g_log) << "pipeStream(" << bufferSize << "): {"
        << result.first << ", " << result.second << "}";
    result.first->m_otherStream = result.second;
    result.second->m_otherStream = result.first;
    result.first->m_mutex.reset(new std::mutex());
    result.second->m_mutex = result.first->m_mutex;
    return result;
}

PipeStream::PipeStream(size_t bufferSize)
: m_bufferSize(bufferSize),
  m_cancelledRead(false),
  m_cancelledWrite(false),
  m_closed(NONE),
  m_otherClosed(NONE),
  m_pendingWriterScheduler(NULL),
  m_pendingReaderScheduler(NULL)
{}

PipeStream::~PipeStream()
{
    MORDOR_LOG_VERBOSE(g_log) << this << " destructing";
    PipeStream::ptr otherStream = m_otherStream.lock();
    std::lock_guard<std::mutex> lock(*m_mutex);
    if (otherStream) {
        MORDOR_ASSERT(!otherStream->m_pendingReader);
        MORDOR_ASSERT(!otherStream->m_pendingReaderScheduler);
        MORDOR_ASSERT(!otherStream->m_pendingWriter);
        MORDOR_ASSERT(!otherStream->m_pendingWriterScheduler);
        if (!m_readBuffer.readAvailable())
            otherStream->m_otherClosed = (CloseType)(otherStream->m_otherClosed | READ);
        else
            otherStream->m_otherClosed = (CloseType)(otherStream->m_otherClosed & ~READ);
        otherStream->m_onRemoteClose();
    }
    if (m_pendingReader) {
        MORDOR_ASSERT(m_pendingReaderScheduler);
        MORDOR_LOG_DEBUG(g_log) << otherStream << " scheduling read";
        m_pendingReaderScheduler->schedule(m_pendingReader);
        m_pendingReader.reset();
        m_pendingReaderScheduler = NULL;
    }
    if (m_pendingWriter) {
        MORDOR_ASSERT(m_pendingWriterScheduler);
        MORDOR_LOG_DEBUG(g_log) << otherStream << " scheduling write";
        m_pendingWriterScheduler->schedule(m_pendingWriter);
        m_pendingWriter.reset();
        m_pendingWriterScheduler = NULL;
    }
}

void
PipeStream::close(CloseType type)
{
    PipeStream::ptr otherStream = m_otherStream.lock();
    std::lock_guard<std::mutex> lock(*m_mutex);
    bool closeWriteFirstTime = !(m_closed & WRITE) && (type & WRITE);
    m_closed = (CloseType)(m_closed | type);
    if (otherStream) {
        otherStream->m_otherClosed = m_closed;
        if (closeWriteFirstTime)
            otherStream->m_onRemoteClose();
    }
    if (m_pendingReader && (m_closed & WRITE)) {
        MORDOR_ASSERT(m_pendingReaderScheduler);
        MORDOR_LOG_DEBUG(g_log) << otherStream << " scheduling read";
        m_pendingReaderScheduler->schedule(m_pendingReader);
        m_pendingReader.reset();
        m_pendingReaderScheduler = NULL;
    }
    if (m_pendingWriter && (m_closed & READ)) {
        MORDOR_ASSERT(m_pendingWriterScheduler);
        MORDOR_LOG_DEBUG(g_log) << otherStream << " scheduling write";
        m_pendingWriterScheduler->schedule(m_pendingWriter);
        m_pendingWriter.reset();
        m_pendingWriterScheduler = NULL;
    }
}

size_t
PipeStream::read(Buffer &b, size_t len)
{
    MORDOR_ASSERT(len != 0);
    while (true) {
        {
            PipeStream::ptr otherStream = m_otherStream.lock();
            std::lock_guard<std::mutex> lock(*m_mutex);
            if (m_closed & READ)
                MORDOR_THROW_EXCEPTION(BrokenPipeException());
            if (!otherStream && !(m_otherClosed & WRITE))
                MORDOR_THROW_EXCEPTION(BrokenPipeException());
            size_t avail = m_readBuffer.readAvailable();
            if (avail > 0) {
                size_t todo = (std::min)(len, avail);
                b.copyIn(m_readBuffer, todo);
                m_readBuffer.consume(todo);
                if (m_pendingWriter) {
                    MORDOR_ASSERT(m_pendingWriterScheduler);
                    MORDOR_LOG_DEBUG(g_log) << otherStream << " scheduling write";
                    m_pendingWriterScheduler->schedule(m_pendingWriter);
                    m_pendingWriter.reset();
                    m_pendingWriterScheduler = NULL;
                }
                MORDOR_LOG_TRACE(g_log) << this << " read(" << len << "): "
                    << todo;
                return todo;
            }

            if (m_otherClosed & WRITE) {
                MORDOR_LOG_TRACE(g_log) << this << " read(" << len << "): "
                    << 0;
                return 0;
            }

            if (m_cancelledRead)
                MORDOR_THROW_EXCEPTION(OperationAbortedException());

            // Wait for the other stream to schedule us
            MORDOR_ASSERT(!otherStream->m_pendingReader);
            MORDOR_ASSERT(!otherStream->m_pendingReaderScheduler);
            MORDOR_LOG_DEBUG(g_log) << this << " waiting to read";
            otherStream->m_pendingReader = Fiber::getThis();
            otherStream->m_pendingReaderScheduler = Scheduler::getThis();
        }
        try {
            Scheduler::yieldTo();
        } catch (...) {
            PipeStream::ptr otherStream = m_otherStream.lock();
            std::lock_guard<std::mutex> lock(*m_mutex);
            if (otherStream && otherStream->m_pendingReader == Fiber::getThis()) {
                MORDOR_ASSERT(otherStream->m_pendingReaderScheduler == Scheduler::getThis());
                otherStream->m_pendingReader.reset();
                otherStream->m_pendingReaderScheduler = NULL;
            }
            throw;
        }
    }
}

void
PipeStream::cancelRead()
{
    PipeStream::ptr otherStream = m_otherStream.lock();
    std::lock_guard<std::mutex> lock(*m_mutex);
    m_cancelledRead = true;
    if (otherStream && otherStream->m_pendingReader) {
        MORDOR_ASSERT(otherStream->m_pendingReaderScheduler);
        MORDOR_LOG_DEBUG(g_log) << this << " cancelling read";
        otherStream->m_pendingReaderScheduler->schedule(otherStream->m_pendingReader);
        otherStream->m_pendingReader.reset();
        otherStream->m_pendingReaderScheduler = NULL;
    }
}

size_t
PipeStream::write(const Buffer &b, size_t len)
{
    MORDOR_ASSERT(len != 0);
    while (true) {
        {
            PipeStream::ptr otherStream = m_otherStream.lock();
            std::lock_guard<std::mutex> lock(*m_mutex);
            if (m_closed & WRITE)
                MORDOR_THROW_EXCEPTION(BrokenPipeException());
            if (!otherStream || (otherStream->m_closed & READ))
                MORDOR_THROW_EXCEPTION(BrokenPipeException());

            size_t available = otherStream->m_readBuffer.readAvailable();
            size_t todo = (std::min)(m_bufferSize - available, len);
            if (todo != 0) {
                otherStream->m_readBuffer.copyIn(b, todo);
                if (m_pendingReader) {
                    MORDOR_ASSERT(m_pendingReaderScheduler);
                    MORDOR_LOG_DEBUG(g_log) << otherStream << " scheduling read";
                    m_pendingReaderScheduler->schedule(m_pendingReader);
                    m_pendingReader.reset();
                    m_pendingReaderScheduler = NULL;
                }
                MORDOR_LOG_TRACE(g_log) << this << " write(" << len << "): "
                    << todo;
                return todo;
            }

            if (m_cancelledWrite)
                MORDOR_THROW_EXCEPTION(OperationAbortedException());

            // Wait for the other stream to schedule us
            MORDOR_ASSERT(!otherStream->m_pendingWriter);
            MORDOR_ASSERT(!otherStream->m_pendingWriterScheduler);
            MORDOR_LOG_DEBUG(g_log) << this << " waiting to write";
            otherStream->m_pendingWriter = Fiber::getThis();
            otherStream->m_pendingWriterScheduler = Scheduler::getThis();
        }
        try {
            Scheduler::yieldTo();
        } catch (...) {
            PipeStream::ptr otherStream = m_otherStream.lock();
            std::lock_guard<std::mutex> lock(*m_mutex);
            if (otherStream && otherStream->m_pendingWriter == Fiber::getThis()) {
                MORDOR_ASSERT(otherStream->m_pendingWriterScheduler == Scheduler::getThis());
                otherStream->m_pendingWriter.reset();
                otherStream->m_pendingWriterScheduler = NULL;
            }
            throw;
        }
    }
}

void
PipeStream::cancelWrite()
{
    PipeStream::ptr otherStream = m_otherStream.lock();
    std::lock_guard<std::mutex> lock(*m_mutex);
    m_cancelledWrite = true;
    if (otherStream && otherStream->m_pendingWriter) {
        MORDOR_ASSERT(otherStream->m_pendingWriterScheduler);
        MORDOR_LOG_DEBUG(g_log) << this << " cancelling write";
        otherStream->m_pendingWriterScheduler->schedule(otherStream->m_pendingWriter);
        otherStream->m_pendingWriter.reset();
        otherStream->m_pendingWriterScheduler = NULL;
    }
}

void
PipeStream::flush(bool flushParent)
{
    while (true) {
        {
            PipeStream::ptr otherStream = m_otherStream.lock();
            std::lock_guard<std::mutex> lock(*m_mutex);
            if (m_cancelledWrite)
                MORDOR_THROW_EXCEPTION(OperationAbortedException());
            if (!otherStream) {
                // See if they read everything before destructing
                if (m_otherClosed & READ)
                    return;
                MORDOR_THROW_EXCEPTION(BrokenPipeException());
            }

            if (otherStream->m_readBuffer.readAvailable() == 0)
                return;
            if (otherStream->m_closed & READ)
                MORDOR_THROW_EXCEPTION(BrokenPipeException());
            // Wait for the other stream to schedule us
            MORDOR_ASSERT(!otherStream->m_pendingWriter);
            MORDOR_ASSERT(!otherStream->m_pendingWriterScheduler);
            MORDOR_LOG_DEBUG(g_log) << this << " waiting to flush";
            otherStream->m_pendingWriter = Fiber::getThis();
            otherStream->m_pendingWriterScheduler = Scheduler::getThis();
        }
        try {
            Scheduler::yieldTo();
        } catch (...) {
            PipeStream::ptr otherStream = m_otherStream.lock();
            std::lock_guard<std::mutex> lock(*m_mutex);
            if (otherStream && otherStream->m_pendingWriter == Fiber::getThis()) {
                MORDOR_ASSERT(otherStream->m_pendingWriterScheduler == Scheduler::getThis());
                otherStream->m_pendingWriter.reset();
                otherStream->m_pendingWriterScheduler = NULL;
            }
            throw;
        }
    }
}

boost::signals2::connection
PipeStream::onRemoteClose(const boost::signals2::slot<void ()> &slot)
{
    return m_onRemoteClose.connect(slot);
}

}

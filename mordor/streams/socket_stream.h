#ifndef __MORDOR_SOCKET_STREAM_H__
#define __MORDOR_SOCKET_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "stream.h"

namespace Mordor {

class Socket;

class SocketStream : public Stream
{
public:
    typedef std::shared_ptr<SocketStream> ptr;

public:
    SocketStream(std::shared_ptr<Socket> socket, bool own = true);

    bool supportsHalfClose() { return true; }
    bool supportsRead() { return true; }
    bool supportsWrite() { return true; }
    bool supportsCancel() { return true; }

    void close(CloseType type = BOTH);

    size_t read(Buffer &buffer, size_t length);
    size_t read(void *buffer, size_t length);
    void cancelRead();
    size_t write(const Buffer &buffer, size_t length);
    size_t write(const void *buffer, size_t length);
    void cancelWrite();

    boost::signals2::connection onRemoteClose(
        const boost::signals2::slot<void ()> &slot);

    std::shared_ptr<Socket> socket() { return m_socket; }

private:
    std::shared_ptr<Socket> m_socket;
    bool m_own;
};

}

#endif


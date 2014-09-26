#ifndef __MORDOR_SSL_STREAM_H__
#define __MORDOR_SSL_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "filter.h"

#include <mutex>
#include <vector>

#include <openssl/ssl.h>

#include "buffer.h"

namespace Mordor {

class OpenSSLException : public std::runtime_error
{
public:
    OpenSSLException(const std::string &message)
        : std::runtime_error(message)
    {}

    OpenSSLException();   // queries OpenSSL for the error code
};

class CertificateVerificationException : public OpenSSLException
{
public:
    CertificateVerificationException(long verifyResult)
        : OpenSSLException(constructMessage(verifyResult)),
          m_verifyResult(verifyResult)
    {}
    CertificateVerificationException(long verifyResult,
        const std::string &message)
        : OpenSSLException(message),
          m_verifyResult(verifyResult)
    {}

    long verifyResult() const { return m_verifyResult; }

private:
    static std::string constructMessage(long verifyResult);

private:
    long m_verifyResult;
};

class SSLStream : public MutatingFilterStream
{
public:
    typedef std::shared_ptr<SSLStream> ptr;

public:
    SSLStream(Stream::ptr parent, bool client = true, bool own = true, SSL_CTX *ctx = NULL);

    bool supportsHalfClose() { return false; }

    void close(CloseType type = BOTH);
    using MutatingFilterStream::read;
    size_t read(void *buffer, size_t length);
    size_t write(const Buffer &buffer, size_t length);
    size_t write(const void *buffer, size_t length);
    void flush(bool flushParent = true);

    void accept();
    void connect();

    void serverNameIndication(const std::string &hostname);

    void verifyPeerCertificate();
    void verifyPeerCertificate(const std::string &hostname);
    void clearSSLError();

private:
    void wantRead();
    int sslCallWithLock(boost::function<int ()> dg, unsigned long *error);

private:
    std::mutex m_mutex;
    std::shared_ptr<SSL_CTX> m_ctx;
    std::shared_ptr<SSL> m_ssl;
    Buffer m_readBuffer, m_writeBuffer;
    BIO *m_readBio, *m_writeBio;
};

}

#endif

#ifndef __MORDOR_PIPE_STREAM_H__
#define __MORDOR_PIPE_STREAM_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <utility>

namespace Mordor {

class Stream;

std::pair<std::shared_ptr<Stream>, std::shared_ptr<Stream> >
    pipeStream(size_t bufferSize = ~0);

}

#endif

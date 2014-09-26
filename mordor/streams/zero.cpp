#include "zero.h"

#include <string.h>

namespace Mordor {

ZeroStream ZeroStream::s_zeroStream;

size_t
ZeroStream::read(void *buffer, size_t length)
{
    memset(buffer, 0, length);
    return length;
}

long long
ZeroStream::seek(long long offset, Anchor anchor)
{
    return 0;
}

}

#ifndef __MORDOR_IOMANAGER_H__
#define __MORDOR_IOMANAGER_H__
// Copyright (c) 2009 - Mozy, Inc.

#include "version.h"

#ifdef WINDOWS
#include "iomanager_iocp.h"
#elif defined(LINUX)
#include "iomanager_epoll.h"
#elif defined(BSD)
#include "iomanager_kqueue.h"
#else
#error Unsupported Platform
#endif

#endif

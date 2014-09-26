#ifndef __MORDOR_DATE_TIME_H__
#define __MORDOR_DATE_TIME_H__
// Copyright (c) 2009 - Mozy, Inc.

#include <chrono>

namespace Mordor {
   inline time_t toTimeT(const std::chrono::system_clock::time_point &ptime)
   {
       return std::chrono::system_clock::to_time_t(ptime);
   }
};

#endif

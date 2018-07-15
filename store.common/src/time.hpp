#pragma once

#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>

using namespace std;

namespace store::common {
  string getCurrentTimeString(bool ISO_8601_fmt = false) {
    std::stringstream ss;
    tm localTime;
    std::chrono::system_clock::time_point t = std::chrono::system_clock::now();
    std::time_t now = std::chrono::system_clock::to_time_t(t);
    localtime_r(&now, &localTime);

    const std::chrono::duration<double> tse = t.time_since_epoch();
    std::chrono::seconds::rep milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(tse).count() % 1000;

    ss << (1900 + localTime.tm_year) << '-'
      << std::setfill('0') << std::setw(2) << (localTime.tm_mon + 1) << '-'
      << std::setfill('0') << std::setw(2) << localTime.tm_mday << (ISO_8601_fmt ? 'T' : ' ')
      << std::setfill('0') << std::setw(2) << localTime.tm_hour << ':'
      << std::setfill('0') << std::setw(2) << localTime.tm_min << ':'
      << std::setfill('0') << std::setw(2) << localTime.tm_sec << '.'
      << std::setfill('0') << std::setw(3) << milliseconds;
    
    if (ISO_8601_fmt)
      ss << 'Z';
  
    return ss.str();
  }

  // Reference: https://stackoverflow.com/questions/13804095/get-the-time-zone-gmt-offset-in-c
  int getTimezoneOffsetSeconds() {
    std::time_t gmt, rawtime = time(NULL);
    struct tm *ptm;

  #if !defined(WIN32)
    struct tm gbuf;
    ptm = gmtime_r(&rawtime, &gbuf);
  #else
    ptm = gmtime(&rawtime);
  #endif
    // Request that mktime() looksup dst in timezone database
    ptm->tm_isdst = -1;
    gmt = mktime(ptm);

    return (int)difftime(rawtime, gmt);
  }

  uint64_t getCurrentTimeMilliseconds() {
    string c;
    std::time_t rawtime;
    std::tm* timeinfo;

    std::time(&rawtime);
    timeinfo = std::localtime(&rawtime);

    std::time_t timeSinceEpoch = mktime(timeinfo);

    return 1000 * timeSinceEpoch;
  }
}

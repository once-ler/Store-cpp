#pragma once

#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>

using namespace std;

namespace store::common {
  string getTimeString(int64_t timestamp, bool ISO_8601_fmt = false) {
    std::time_t tt = 0.001 * timestamp;
    std::stringstream ss;
    tm localTime;
    localtime_r(&tt, &localTime);

    std::chrono::system_clock::time_point t = std::chrono::system_clock::now();
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

  string getCurrentTimeString(bool ISO_8601_fmt = false) {
    std::chrono::system_clock::time_point t = std::chrono::system_clock::now();
    std::time_t now = std::chrono::system_clock::to_time_t(t);
    
    return getTimeString(1000 * now, ISO_8601_fmt);
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

  int64_t getCurrentTimeMilliseconds() {
    string c;
    std::time_t rawtime;
    std::tm* timeinfo;

    std::time(&rawtime);
    timeinfo = std::localtime(&rawtime);

    std::time_t timeSinceEpoch = mktime(timeinfo);

    return 1000 * timeSinceEpoch;
  }

  // Expect format: "%Y-%m-%dT%H:%M:%S" 
  int64_t getTimeMilliseconds(const string& dateString, const string& format = "%Y-%m-%dT%H:%M:%s") {
    struct std::tm tm;
    std::istringstream ss(dateString);
    ss >> std::get_time(&tm, format.c_str());
    std::time_t time = mktime(&tm);

    return 1000 * (time + getTimezoneOffsetSeconds());
  }

}

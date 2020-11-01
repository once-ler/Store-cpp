#pragma once

#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include "date/date.h"

using namespace std;
using namespace std::chrono;
using namespace date;

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

  string getCurrentDate(bool with_time = false) {
    time_t now = time(0);
    struct tm tstruct;
    char buf[21];
    tstruct = *localtime(&now);
    const char* format = with_time ? "%F %T" : "%F";   
    strftime(buf, sizeof(buf), format, &tstruct);

    return buf;
  }

  int64_t getMillisecondsFromTimePoint(const system_clock::time_point& nextDate) {
    system_clock::duration dtn = nextDate.time_since_epoch();
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(dtn);
    auto nextDateMilli = ms.count();
    return nextDateMilli;
  }

  enum Direction {
    Forward,
    Backward
  };

  system_clock::time_point addDaysToDate(const system_clock::time_point& dt, int numOfDays, Direction direction = Forward) {
    auto dyz = days{numOfDays};
    return direction == Forward ? dt + dyz : dt - dyz;
  }

  system_clock::time_point daysAgo(int numOfDays) {
    auto today = date::floor<days>(std::chrono::system_clock::now());
    return addDaysToDate(today, numOfDays, Direction::Backward);
  }

  system_clock::time_point parseDate(const string& dtstr, const char* dtfmt = "%Y%m%d") {
    istringstream iss{dtstr};
    system_clock::time_point tp;
    iss >> date::parse(dtfmt, tp);
    return tp;
  }

  std::function<std::string(int)> parseAndAddDaysToDate(const string& dtstr, const char* dtfmt = "%Y%m%d") {
    return [=](int numOfDays) -> std::string {
      auto tp = parseDate(dtstr, dtfmt);
      auto tp1 = addDaysToDate(tp, numOfDays);
      return date::format(dtfmt, tp1);
    };
  }

  int64_t dateDiff(system_clock::time_point lhsTp, system_clock::time_point rhsTp) {
    using MS = std::chrono::milliseconds;
    auto lhsInt64 = std::chrono::duration_cast<MS>(lhsTp.time_since_epoch()).count();
    auto rhsInt64 = std::chrono::duration_cast<MS>(rhsTp.time_since_epoch()).count();
    return lhsInt64 - rhsInt64; 
  }

  int64_t dateDiff(const string& lhsStr, const string& rhsStr, const char* dtfmt = "%Y%m%d") {
    system_clock::time_point lhsTp = parseDate(lhsStr, dtfmt);
    system_clock::time_point rhsTp = parseDate(rhsStr, dtfmt);
    return dateDiff(lhsTp, rhsTp); 
  }

}

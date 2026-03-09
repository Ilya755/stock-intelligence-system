#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <format>
#include <sstream>
#include <iomanip>
#include <exception>

using Timestamp = std::chrono::system_clock::time_point;
using Date = std::chrono::year_month_day;

class TimeUtils {
public:
    static std::string TimestampToString(const Timestamp& timestamp);

    static std::string DateToString(const Date& date);

    static Timestamp StringToTimestamp(const std::string& str);

    static Date StringToDate(const std::string& str);
};
#include "TimeUtils.hpp"

#include "Logger.hpp"

std::string TimeUtils::TimestampToString(const Timestamp& timestamp) {
    return std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::floor<std::chrono::seconds>(timestamp));
}

std::string TimeUtils::DateToString(const Date& date) {
    return std::format("{:%Y-%m-%d}", date);
}

Timestamp TimeUtils::StringToTimestamp(const std::string& str) {
    std::chrono::system_clock::time_point timestamp;
    
     const std::vector<const char*> formats = {"%Y-%m-%d %H:%M:%S",  
                                                "%Y-%m-%dT%H:%M:%SZ",
                                                "%Y-%m-%dT%H:%M:%S",  
                                                "%Y-%m-%d"};

    for (const auto& fmt : formats) {
        std::istringstream ss(str);
        if (ss >> std::chrono::parse(fmt, timestamp)) {
            return timestamp;
        }
    }

    std::string err = "[TimeUtils] Failed to parse timestamp: " + str;
    Logger::Error("{}", err); 
    throw std::runtime_error(err);
}

Date TimeUtils::StringToDate(const std::string& str) {
    int year = 0, month = 0, day = 0;
    char dash1 = 0, dash2 = 0;
    
    std::istringstream ss(str);
    if (ss >> year >> dash1 >> month >> dash2 >> day && dash1 == '-' && dash2 == '-') {
        if (month < 1 || month > 12 || day < 1 || day > 31) {
            std::string err = "[TimeUtils] Invalid date values: " + str;
            Logger::Error("{}", err);
            throw std::runtime_error(err);
        }
        
        return Date(std::chrono::year{year},
                        std::chrono::month{static_cast<unsigned int>(month)},
                        std::chrono::day{static_cast<unsigned int>(day)});
    }
    
    try {
        auto timestamp = StringToTimestamp(str);
        return std::chrono::year_month_day{std::chrono::floor<std::chrono::days>(timestamp)};
    } catch (...) {
        std::string err = "[TimeUtils] Failed to parse date: " + str;
        Logger::Error("{}", err); 
        throw std::runtime_error(err);
    }
}
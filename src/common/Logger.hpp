#pragma once

#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype> 
#include <iterator>
#include <format>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"

class Logger {
public:
    static void InitConsole(const std::string& pattern = "%^[%l] %v%$");

    static void InitFile(const std::string& path, const std::string& level_str,
                            size_t max_size = 1024 * 1024 * 5, size_t max_files = 5,
                            const std::string& pattern = "[%Y-%m-%d %H:%M:%S] [%l] %v");

    template <typename... Args>
    static void Info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        spdlog::info(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void Error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        spdlog::error(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void Debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        spdlog::debug(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void Warn(spdlog::format_string_t<Args...> fmt, Args&&... args) { 
        spdlog::warn(fmt, std::forward<Args>(args)...); 
    }

    template <typename... Args>
    static void Critical(spdlog::format_string_t<Args...> fmt, Args&&... args) { 
        spdlog::critical(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void Trace(spdlog::format_string_t<Args...> fmt, Args&&... args) { 
        spdlog::trace(fmt, std::forward<Args>(args)...);
    }

private:
    static spdlog::level::level_enum ParseLevel(const std::string& level_str);
};
#include "Logger.hpp"

void Logger::InitConsole(const std::string& pattern) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    console_sink->set_color(spdlog::level::info, "\033[1;32m");
    console_sink->set_color(spdlog::level::err, "\033[1;31m");
    console_sink->set_color(spdlog::level::warn, "\033[1;33m");
    console_sink->set_color(spdlog::level::debug, "\033[1;36m");
    console_sink->set_color(spdlog::level::critical, "\033[1;35m");

    console_sink->set_pattern(pattern);

    auto logger = std::make_shared<spdlog::logger>("console", console_sink);
    logger->set_level(spdlog::level::info);
    spdlog::set_default_logger(logger);

    Logger::Info("[Logger] Console logger initialized!");
}

void Logger::InitFile(const std::string& path, const std::string& level_str,
                        size_t max_size, size_t max_files, const std::string& pattern) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    console_sink->set_color(spdlog::level::info, "\033[1;32m");
    console_sink->set_color(spdlog::level::err, "\033[1;31m");
    console_sink->set_color(spdlog::level::warn, "\033[1;33m");
    console_sink->set_color(spdlog::level::debug, "\033[1;36m");
    console_sink->set_color(spdlog::level::critical, "\033[1;35m");

    console_sink->set_pattern("%^[%l] %v%$");

    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(path, max_size, max_files);

    file_sink->set_pattern(pattern);

    std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};
    auto logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());

    logger->set_level(ParseLevel(level_str));
    logger->flush_on(spdlog::level::err);

    spdlog::set_default_logger(logger);

    Logger::Info("[Logger] Logger initialized with rotation. Max size: {} bytes, max files: {}", max_size, max_files);
}

spdlog::level::level_enum Logger::ParseLevel(const std::string& level_str) {
    std::string lower_level_str;
    lower_level_str.reserve(level_str.size());
    std::transform(level_str.begin(), level_str.end(), 
                        std::back_inserter(lower_level_str),
                        [&](unsigned char c) { return std::tolower(c); });
    return spdlog::level::from_str(lower_level_str);
}
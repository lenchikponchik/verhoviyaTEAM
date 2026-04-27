#include "logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace web_agent {

namespace {

std::string timestamp_now() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &time);
#else
    localtime_r(&time, &local_tm);
#endif
    std::ostringstream out;
    out << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

} // namespace

Logger::Logger(const std::string &path) : output_(path, std::ios::app) {
    if (!output_) {
        throw std::runtime_error("cannot open log file: " + path);
    }
}

void Logger::info(const std::string &message) {
    write("INFO", message);
}

void Logger::error(const std::string &message) {
    write("ERROR", message);
}

void Logger::debug(const std::string &message) {
    write("DEBUG", message);
}

void Logger::write(const std::string &level, const std::string &message) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string line = timestamp_now() + " [" + level + "] " + message;
    output_ << line << '\n';
    output_.flush();
    std::cout << line << std::endl;
}

} // namespace web_agent

#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace web_agent {

class Logger {
public:
    explicit Logger(const std::string &path);

    void info(const std::string &message);
    void error(const std::string &message);
    void debug(const std::string &message);

private:
    void write(const std::string &level, const std::string &message);

    std::mutex mutex_;
    std::ofstream output_;
};

} // namespace web_agent

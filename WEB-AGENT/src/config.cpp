#include "config.h"

#include "json.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace web_agent {

namespace {

std::string read_file(const std::string &path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open config file: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string read_string(const JsonValue::object &obj, const std::string &key, const std::string &fallback = {}) {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->second.is_string()) {
        return fallback;
    }
    return it->second.as_string();
}

int read_int(const JsonValue::object &obj, const std::string &key, int fallback) {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return fallback;
    }
    return it->second.is_number() ? it->second.as_int(fallback) : fallback;
}

bool read_bool(const JsonValue::object &obj, const std::string &key, bool fallback) {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return fallback;
    }
    return it->second.is_bool() ? it->second.as_bool(fallback) : fallback;
}

} // namespace

AgentConfig load_config(const std::string &path) {
    const JsonValue root = JsonValue::parse(read_file(path));
    if (!root.is_object()) {
        throw std::runtime_error("config root must be a JSON object");
    }

    const auto &obj = root.as_object();
    AgentConfig config;
    config.uid = read_string(obj, "uid");
    config.description = read_string(obj, "description", config.description);
    config.server_url = read_string(obj, "server_url");
    config.access_code = read_string(obj, "access_code");
    config.poll_path = read_string(obj, "poll_path", config.poll_path);
    config.result_path = read_string(obj, "result_path", config.result_path);
    config.register_path = read_string(obj, "register_path", config.register_path);
    config.task_directory = read_string(obj, "task_directory", config.task_directory);
    config.result_directory = read_string(obj, "result_directory", config.result_directory);
    config.log_file = read_string(obj, "log_file", config.log_file);
    config.poll_interval_sec = read_int(obj, "poll_interval_sec", config.poll_interval_sec);
    config.request_timeout_sec = read_int(obj, "request_timeout_sec", config.request_timeout_sec);
    config.max_parallel_tasks = read_int(obj, "max_parallel_tasks", config.max_parallel_tasks);
    config.backoff_max_sec = read_int(obj, "backoff_max_sec", config.backoff_max_sec);
    config.register_on_startup = read_bool(obj, "register_on_startup", config.register_on_startup);

    if (config.uid.empty()) {
        throw std::runtime_error("config field 'uid' is required");
    }
    if (config.server_url.empty()) {
        throw std::runtime_error("config field 'server_url' is required");
    }
    if (config.poll_interval_sec < 1) {
        config.poll_interval_sec = 1;
    }
    if (config.request_timeout_sec < 1) {
        config.request_timeout_sec = 1;
    }
    if (config.max_parallel_tasks < 1) {
        config.max_parallel_tasks = 1;
    }
    if (config.backoff_max_sec < config.poll_interval_sec) {
        config.backoff_max_sec = config.poll_interval_sec;
    }

    return config;
}

} // namespace web_agent

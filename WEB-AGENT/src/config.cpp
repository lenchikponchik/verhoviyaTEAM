#include "config.h"

#include "json.h"

#include <cstdlib>
#include <cctype>
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

std::string join_server_url(const JsonValue::object &obj) {
    const std::string protocol = read_string(obj, "protocol", "https");
    const std::string host = read_string(obj, "server");
    const int port = read_int(obj, "port", 0);

    if (host.empty()) {
        return {};
    }

    if (port > 0) {
        return protocol + "://" + host + ":" + std::to_string(port);
    }
    return protocol + "://" + host;
}

bool read_bool(const JsonValue::object &obj, const std::string &key, bool fallback) {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return fallback;
    }
    return it->second.is_bool() ? it->second.as_bool(fallback) : fallback;
}

bool starts_with(const std::string &value, const std::string &prefix) {
    return value.rfind(prefix, 0) == 0;
}

bool is_valid_env_name(const std::string &name) {
    if (name.empty()) {
        return false;
    }
    const unsigned char first = static_cast<unsigned char>(name.front());
    if (!(std::isalpha(first) || first == '_')) {
        return false;
    }
    for (const char ch : name) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (!(std::isalnum(uch) || ch == '_')) {
            return false;
        }
    }
    return true;
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
    if (config.server_url.empty()) {
        config.server_url = join_server_url(obj);
    }
    config.access_code = read_string(obj, "access_code");
    config.access_code_env = read_string(obj, "access_code_env", config.access_code_env);
    if (!config.access_code_env.empty() && !is_valid_env_name(config.access_code_env)) {
        throw std::runtime_error("config field 'access_code_env' must be a valid environment variable name");
    }
    if (!config.access_code_env.empty()) {
        if (const char *env_access_code = std::getenv(config.access_code_env.c_str())) {
            if (*env_access_code != '\0') {
                config.access_code = env_access_code;
            }
        }
    }
    config.poll_path = read_string(obj, "poll_path", config.poll_path);
    config.result_path = read_string(obj, "result_path", config.result_path);
    config.register_path = read_string(obj, "register_path", config.register_path);
    config.task_directory = read_string(obj, "task_directory", config.task_directory);
    config.result_directory = read_string(obj, "result_directory", config.result_directory);
    config.log_file = read_string(obj, "log_file", config.log_file);
    config.ca_cert_file = read_string(obj, "ca_cert_file", config.ca_cert_file);
    config.ca_cert_dir = read_string(obj, "ca_cert_dir", config.ca_cert_dir);
    config.poll_interval_sec = read_int(obj, "poll_interval_sec", config.poll_interval_sec);
    if (obj.find("interval") != obj.end()) {
        config.poll_interval_sec = read_int(obj, "interval", config.poll_interval_sec);
    }
    config.request_timeout_sec = read_int(obj, "request_timeout_sec", config.request_timeout_sec);
    config.max_parallel_tasks = read_int(obj, "max_parallel_tasks", config.max_parallel_tasks);
    config.backoff_max_sec = read_int(obj, "backoff_max_sec", config.backoff_max_sec);
    config.register_on_startup = read_bool(obj, "register_on_startup", config.register_on_startup);
    config.allow_insecure_http = read_bool(obj, "allow_insecure_http", config.allow_insecure_http);

    if (config.uid.empty()) {
        throw std::runtime_error("config field 'uid' is required");
    }
    if (config.server_url.empty()) {
        throw std::runtime_error("config field 'server_url' is required");
    }
    if (!starts_with(config.server_url, "https://") &&
        !(config.allow_insecure_http && starts_with(config.server_url, "http://"))) {
        throw std::runtime_error("config field 'server_url' must use https:// (or explicitly enable allow_insecure_http for http://)");
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

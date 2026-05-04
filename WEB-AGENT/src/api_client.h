#pragma once

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include "httplib.h"

#include "config.h"
#include "json.h"
#include "logger.h"
#include "task_executor.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace web_agent {

class ApiClient {
public:
    ApiClient(const AgentConfig &config, Logger &logger);

    bool probe_server();
    bool register_agent();
    std::optional<TaskInstruction> fetch_task();
    bool send_result(const TaskInstruction &task, const ExecutionResult &result);
    const std::string &access_code() const;

private:
    JsonValue post_json(const std::string &path, const JsonValue &payload);
    std::optional<JsonValue> try_parse_json(const std::string &body) const;

    AgentConfig config_;
    Logger &logger_;
    std::unique_ptr<httplib::Client> client_;
    std::mutex client_mutex_;
    std::string access_code_;
};

} // namespace web_agent

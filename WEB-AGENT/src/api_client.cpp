#include "api_client.h"

#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace web_agent {

namespace {

std::string string_field(const JsonValue::object &obj, const std::string &key) {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->second.is_string()) {
        return {};
    }
    return it->second.as_string();
}

bool looks_like_no_task(const JsonValue::object &obj) {
    const std::string status = string_field(obj, "status");
    const std::string code = string_field(obj, "code_responce");
    const std::string msg = string_field(obj, "msg");
    return status == "WAIT" || code == "-11" || msg.find("нет") != std::string::npos;
}

} // namespace

ApiClient::ApiClient(const AgentConfig &config, Logger &logger)
    : config_(config), logger_(logger), client_(std::make_unique<httplib::Client>(config.server_url)), access_code_(config.access_code) {
    client_->set_connection_timeout(config_.request_timeout_sec);
    client_->set_read_timeout(config_.request_timeout_sec);
    client_->set_write_timeout(config_.request_timeout_sec);
    client_->set_follow_location(true);
}

bool ApiClient::probe_server() {
    const auto result = client_->Get("/");
    if (!result) {
        logger_.error("server probe failed");
        return false;
    }
    logger_.debug("server probe HTTP status " + std::to_string(result->status));
    return result->status >= 200 && result->status < 500;
}

bool ApiClient::register_agent() {
    JsonValue::object payload{
        {"UID", config_.uid},
        {"descr", config_.description},
    };

    try {
        const JsonValue response = post_json(config_.register_path, payload);
        if (!response.is_object()) {
            throw std::runtime_error("registration response is not an object");
        }

        const auto &obj = response.as_object();
        const std::string remote_access_code = string_field(obj, "access_code");
        if (!remote_access_code.empty()) {
            access_code_ = remote_access_code;
            logger_.info("agent registered successfully");
            return true;
        }

        logger_.error("registration response does not contain access_code");
    } catch (const std::exception &ex) {
        logger_.error(std::string("registration failed: ") + ex.what());
    }

    if (!access_code_.empty()) {
        logger_.info("falling back to access_code from config");
        return true;
    }

    return false;
}

std::optional<TaskInstruction> ApiClient::fetch_task() {
    JsonValue::object payload{
        {"UID", config_.uid},
        {"descr", config_.description},
        {"access_code", access_code_},
    };

    const JsonValue response = post_json(config_.poll_path, payload);
    if (!response.is_object()) {
        throw std::runtime_error("task response is not a JSON object");
    }

    const auto &obj = response.as_object();
    if (looks_like_no_task(obj)) {
        return std::nullopt;
    }

    return parse_task_instruction(response, std::filesystem::path(config_.result_directory));
}

bool ApiClient::send_result(const TaskInstruction &task, const ExecutionResult &result) {
    JsonValue::object result_json{
        {"UID", config_.uid},
        {"access_code", access_code_},
        {"message", result.message},
        {"files", static_cast<int>(result.files.size())},
        {"session_id", task.session_id},
    };

    httplib::UploadFormDataItems items;
    httplib::FormDataProviderItems provider_items;
    items.push_back({"result_code", std::to_string(result.result_code), "", ""});
    items.push_back({"result", JsonValue(result_json).dump(), "", "application/json"});

    int index = 1;
    for (const auto &file : result.files) {
        provider_items.push_back(httplib::make_file_provider(
            "file" + std::to_string(index++),
            file.string(),
            file.filename().string(),
            "application/octet-stream"));
    }

    const auto response = client_->Post(config_.result_path, httplib::Headers{}, items, provider_items);
    if (!response) {
        logger_.error("failed to send task result for session " + task.session_id);
        return false;
    }

    logger_.info("result delivered for session " + task.session_id + " with HTTP " + std::to_string(response->status));
    return response->status >= 200 && response->status < 300;
}

const std::string &ApiClient::access_code() const {
    return access_code_;
}

JsonValue ApiClient::post_json(const std::string &path, const JsonValue &payload) {
    const auto response = client_->Post(path, payload.dump(), "application/json");
    if (!response) {
        throw std::runtime_error("HTTP request failed for " + path);
    }
    if (response->status < 200 || response->status >= 300) {
        throw std::runtime_error("HTTP status " + std::to_string(response->status) + " for " + path);
    }

    const auto parsed = try_parse_json(response->body);
    if (!parsed.has_value()) {
        throw std::runtime_error("server did not return JSON for " + path);
    }
    return *parsed;
}

std::optional<JsonValue> ApiClient::try_parse_json(const std::string &body) const {
    try {
        return JsonValue::parse(body);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace web_agent

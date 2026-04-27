#pragma once

#include "json.h"

#include <filesystem>
#include <string>
#include <vector>

namespace web_agent {

struct TaskInstruction {
    std::string session_id;
    std::string task_code;
    std::string command;
    std::string working_directory;
    std::vector<std::string> result_files;
    int result_wait_timeout_sec = 5;
};

struct ExecutionResult {
    int result_code = 0;
    std::string message;
    std::vector<std::filesystem::path> files;
};

TaskInstruction parse_task_instruction(const JsonValue &json, const std::filesystem::path &default_result_dir);
ExecutionResult execute_task(
    const TaskInstruction &task,
    const std::filesystem::path &task_root,
    const std::filesystem::path &result_root);

} // namespace web_agent

#include "task_executor.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace web_agent {

namespace fs = std::filesystem;

namespace {

std::string json_string(const JsonValue::object &obj, const std::vector<std::string> &keys) {
    for (const auto &key : keys) {
        const auto it = obj.find(key);
        if (it != obj.end() && it->second.is_string()) {
            return it->second.as_string();
        }
    }
    return {};
}

int json_int(const JsonValue::object &obj, const std::vector<std::string> &keys, int fallback) {
    for (const auto &key : keys) {
        const auto it = obj.find(key);
        if (it != obj.end() && it->second.is_number()) {
            return it->second.as_int(fallback);
        }
    }
    return fallback;
}

std::vector<std::string> json_string_list(const JsonValue::object &obj, const std::vector<std::string> &keys) {
    for (const auto &key : keys) {
        const auto it = obj.find(key);
        if (it == obj.end()) {
            continue;
        }
        if (it->second.is_string()) {
            return {it->second.as_string()};
        }
        if (it->second.is_array()) {
            std::vector<std::string> items;
            for (const auto &entry : it->second.as_array()) {
                if (entry.is_string()) {
                    items.push_back(entry.as_string());
                }
            }
            return items;
        }
    }
    return {};
}

std::string shell_escape(const std::string &value) {
    std::string escaped = "'";
    for (const char ch : value) {
        if (ch == '\'') {
            escaped += "'\\''";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped.push_back('\'');
    return escaped;
}

int normalize_exit_code(int status) {
#ifdef _WIN32
    return status;
#else
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return status;
#endif
}

std::string read_text_file(const fs::path &path) {
    std::ifstream input(path);
    if (!input) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::vector<fs::path> collect_generated_files(
    const fs::path &result_root,
    const std::vector<std::string> &requested_files,
    const fs::file_time_type &started_at) {
    std::vector<fs::path> files;

    if (!requested_files.empty()) {
        for (const auto &name : requested_files) {
            const fs::path path = result_root / name;
            if (fs::exists(path) && fs::is_regular_file(path)) {
                files.push_back(path);
            }
        }
        return files;
    }

    if (!fs::exists(result_root)) {
        return files;
    }

    for (const auto &entry : fs::directory_iterator(result_root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.last_write_time() >= started_at) {
            files.push_back(entry.path());
        }
    }
    return files;
}

std::string join_output(const std::string &stdout_text, const std::string &stderr_text) {
    if (stdout_text.empty() && stderr_text.empty()) {
        return "task completed";
    }

    std::ostringstream out;
    if (!stdout_text.empty()) {
        out << stdout_text;
    }
    if (!stderr_text.empty()) {
        if (!stdout_text.empty() && stdout_text.back() != '\n') {
            out << '\n';
        }
        out << "[stderr]\n" << stderr_text;
    }
    return out.str();
}

} // namespace

TaskInstruction parse_task_instruction(const JsonValue &json, const fs::path &default_result_dir) {
    if (!json.is_object()) {
        throw std::runtime_error("task response is not a JSON object");
    }

    const auto &obj = json.as_object();
    TaskInstruction task;
    task.session_id = json_string(obj, {"session_id", "session", "sessionId"});
    task.task_code = json_string(obj, {"task_code", "task", "type"});
    task.command = json_string(obj, {"command", "cmd", "task_cmd", "task_command"});

    const std::string executable = json_string(obj, {"program", "executable", "file", "path"});
    const std::string arguments = json_string(obj, {"arguments", "args", "params"});
    if (task.command.empty() && !executable.empty()) {
        task.command = shell_escape(executable);
        if (!arguments.empty()) {
            task.command += " " + arguments;
        }
    }

    task.working_directory = json_string(obj, {"working_directory", "workdir", "cwd"});
    if (task.working_directory.empty()) {
        task.working_directory = default_result_dir.string();
    }

    task.result_files = json_string_list(obj, {"result_files", "files", "file", "file1"});
    task.result_wait_timeout_sec = json_int(obj, {"result_wait_timeout_sec", "result_timeout_sec", "timeout_sec"}, 5);

    if (task.session_id.empty()) {
        throw std::runtime_error("task response missing session_id");
    }
    if (task.task_code.empty()) {
        throw std::runtime_error("task response missing task_code");
    }
    if (task.command.empty()) {
        throw std::runtime_error("task response missing executable command");
    }

    return task;
}

ExecutionResult execute_task(const TaskInstruction &task, const fs::path &task_root, const fs::path &result_root) {
    fs::create_directories(task_root);
    fs::create_directories(result_root);

    const fs::path stdout_path = task_root / (task.session_id + ".stdout.log");
    const fs::path stderr_path = task_root / (task.session_id + ".stderr.log");
    const auto started_at = fs::file_time_type::clock::now();

#ifdef _WIN32
    std::string shell_command = "cmd /C \"cd /D " + task.working_directory + " && " + task.command +
                                " > " + stdout_path.string() + " 2> " + stderr_path.string() + "\"";
#else
    std::string shell_command;
    if (!task.working_directory.empty()) {
        shell_command = "cd " + shell_escape(task.working_directory) + " && ";
    }
    shell_command += task.command + " > " + shell_escape(stdout_path.string()) + " 2> " + shell_escape(stderr_path.string());
    shell_command = "/bin/sh -lc " + shell_escape(shell_command);
#endif

    const int status = std::system(shell_command.c_str());
    const int exit_code = normalize_exit_code(status);

    if (task.result_wait_timeout_sec > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(task.result_wait_timeout_sec));
    }

    ExecutionResult result;
    result.result_code = exit_code;
    result.files = collect_generated_files(result_root, task.result_files, started_at);
    result.message = join_output(read_text_file(stdout_path), read_text_file(stderr_path));

    if (result.result_code != 0 && result.message.empty()) {
        result.message = "task failed with exit code " + std::to_string(result.result_code);
    }

    return result;
}

} // namespace web_agent

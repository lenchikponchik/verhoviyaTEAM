#include "task_executor.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace web_agent {

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kMaxCapturedOutputBytes = 256 * 1024;

std::string safe_session_name(const std::string &session_id) {
    std::string safe;
    safe.reserve(session_id.size());
    for (const char ch : session_id) {
        const bool is_alnum = (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
        safe.push_back(is_alnum || ch == '-' || ch == '_' || ch == '.' ? ch : '_');
    }
    return safe.empty() ? "session" : safe;
}

std::string trim_copy(const std::string &value) {
    const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::string upper_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return value;
}

std::string json_scalar_string(const JsonValue &value) {
    if (value.is_string()) {
        return value.as_string();
    }
    if (value.is_number()) {
        return std::to_string(value.as_int());
    }
    if (value.is_bool()) {
        return value.as_bool() ? "true" : "false";
    }
    return {};
}

std::string json_string(const JsonValue::object &obj, const std::vector<std::string> &keys) {
    for (const auto &key : keys) {
        const auto it = obj.find(key);
        if (it != obj.end()) {
            const std::string value = json_scalar_string(it->second);
            if (!value.empty()) {
                return value;
            }
        }
    }
    return {};
}

int json_int(const JsonValue::object &obj, const std::vector<std::string> &keys, int fallback) {
    for (const auto &key : keys) {
        const auto it = obj.find(key);
        if (it == obj.end()) {
            continue;
        }
        if (it->second.is_number()) {
            return it->second.as_int(fallback);
        }
        if (it->second.is_string()) {
            try {
                return std::stoi(it->second.as_string());
            } catch (...) {
                return fallback;
            }
        }
    }
    return fallback;
}

std::vector<std::string> json_string_list(const JsonValue::object &obj, const std::vector<std::string> &keys) {
    std::vector<std::string> numbered_items;
    for (int index = 1;; ++index) {
        const auto it = obj.find("file" + std::to_string(index));
        if (it == obj.end()) {
            break;
        }
        const std::string value = json_scalar_string(it->second);
        if (!value.empty()) {
            numbered_items.push_back(value);
        }
    }
    if (!numbered_items.empty()) {
        return numbered_items;
    }

    for (const auto &key : keys) {
        const auto it = obj.find(key);
        if (it == obj.end()) {
            continue;
        }
        const std::string scalar = json_scalar_string(it->second);
        if (!scalar.empty()) {
            return {scalar};
        }
        if (it->second.is_array()) {
            std::vector<std::string> items;
            for (const auto &entry : it->second.as_array()) {
                const std::string value = json_scalar_string(entry);
                if (!value.empty()) {
                    items.push_back(value);
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

bool is_safe_relative_path(const fs::path &path) {
    if (path.empty() || path.is_absolute()) {
        return false;
    }
    for (const auto &part : path) {
        if (part == ".." || part == ".") {
            return false;
        }
    }
    return true;
}

bool path_has_prefix(const fs::path &path, const fs::path &prefix) {
    auto path_it = path.begin();
    auto prefix_it = prefix.begin();
    while (prefix_it != prefix.end()) {
        if (path_it == path.end() || *path_it != *prefix_it) {
            return false;
        }
        ++path_it;
        ++prefix_it;
    }
    return true;
}

std::optional<fs::path> normalized_path(const fs::path &path) {
    std::error_code ec;
    const fs::path normalized = fs::weakly_canonical(path, ec);
    if (ec) {
        return std::nullopt;
    }
    return normalized;
}

bool is_within_root(const fs::path &path, const fs::path &root) {
    const auto normalized_root = normalized_path(root);
    const auto normalized_candidate = normalized_path(path);
    if (!normalized_root.has_value() || !normalized_candidate.has_value()) {
        return false;
    }
    return path_has_prefix(*normalized_candidate, *normalized_root);
}

bool is_safe_result_file(const fs::path &path, const fs::path &result_root) {
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(path, ec);
    if (ec || !fs::exists(status) || !fs::is_regular_file(status) || fs::is_symlink(status)) {
        return false;
    }
    return is_within_root(path, result_root);
}

std::optional<fs::path> resolve_working_directory(const std::string &requested_workdir, const fs::path &result_root_abs) {
    fs::path candidate = result_root_abs;
    if (!requested_workdir.empty()) {
        const fs::path requested_path(requested_workdir);
        candidate = requested_path.is_absolute() ? requested_path : (result_root_abs / requested_path);
    }
    std::error_code ec;
    fs::create_directories(candidate, ec);
    if (ec || !is_within_root(candidate, result_root_abs)) {
        return std::nullopt;
    }
    return normalized_path(candidate);
}

#ifdef _WIN32
std::string win_quote(const std::string &value) {
    std::string escaped = "\"";
    for (const char ch : value) {
        if (ch == '"') {
            escaped += "\\\"";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped.push_back('"');
    return escaped;
}
#endif

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
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }

    std::string content(kMaxCapturedOutputBytes, '\0');
    input.read(&content[0], static_cast<std::streamsize>(content.size()));
    content.resize(static_cast<std::size_t>(input.gcount()));
    if (!input.eof()) {
        content += "\n[output truncated]\n";
    }
    return content;
}

std::vector<fs::path> collect_generated_files(
    const fs::path &result_root,
    const std::vector<std::string> &requested_files,
    const fs::file_time_type &started_at) {
    std::vector<fs::path> files;

    if (!requested_files.empty()) {
        for (const auto &name : requested_files) {
            const fs::path relative_path(name);
            if (!is_safe_relative_path(relative_path)) {
                continue;
            }
            const fs::path path = result_root / relative_path;
            if (is_safe_result_file(path, result_root)) {
                files.push_back(path);
            }
        }
        return files;
    }

    if (!fs::exists(result_root)) {
        return files;
    }

    for (const auto &entry : fs::directory_iterator(result_root)) {
        if (!is_safe_result_file(entry.path(), result_root)) {
            continue;
        }
        if (entry.last_write_time() >= started_at) {
            files.push_back(entry.path());
        }
    }
    return files;
}

std::vector<fs::path> wait_for_generated_files(
    const fs::path &result_root,
    const std::vector<std::string> &requested_files,
    const fs::file_time_type &started_at,
    int timeout_sec) {
    if (timeout_sec <= 0) {
        return collect_generated_files(result_root, requested_files, started_at);
    }

    if (requested_files.empty()) {
        std::this_thread::sleep_for(std::chrono::seconds(timeout_sec));
        return collect_generated_files(result_root, requested_files, started_at);
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_sec);
    std::vector<fs::path> files;
    do {
        files = collect_generated_files(result_root, requested_files, started_at);
        if (files.size() >= requested_files.size()) {
            return files;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    } while (std::chrono::steady_clock::now() < deadline);

    return files;
}

void append_unique(std::vector<std::string> &target, const std::vector<std::string> &items) {
    for (const auto &item : items) {
        if (std::find(target.begin(), target.end(), item) == target.end()) {
            target.push_back(item);
        }
    }
}

void apply_task_fields(TaskInstruction &task, const JsonValue::object &obj, const fs::path &default_result_dir, bool identity_fields) {
    if (identity_fields) {
        task.task_id = json_string(obj, {"task_id", "taskId", "id"});
        task.session_id = json_string(obj, {"session_id", "session", "sessionId"});
        task.task_code = json_string(obj, {"task_code", "task", "type"});
    }

    if (task.command.empty()) {
        task.command = json_string(obj, {"command", "cmd", "task_cmd", "task_command"});
    }

    const std::string executable = json_string(obj, {"program", "executable", "file", "path"});
    const std::string arguments = json_string(obj, {"arguments", "args", "params"});
    if (task.command.empty() && !executable.empty()) {
        task.command = shell_escape(executable);
        if (!arguments.empty()) {
            task.command += " " + arguments;
        }
    }

    const std::string working_directory = json_string(obj, {"working_directory", "workdir", "cwd"});
    if (!working_directory.empty()) {
        task.working_directory = working_directory;
    } else if (task.working_directory.empty()) {
        task.working_directory = default_result_dir.string();
    }

    append_unique(task.result_files, json_string_list(obj, {"result_files", "files", "file"}));
    task.result_wait_timeout_sec = json_int(obj, {"result_wait_timeout_sec", "result_timeout_sec", "timeout_sec"}, task.result_wait_timeout_sec);
}

void apply_options_field(TaskInstruction &task, const JsonValue::object &obj, const fs::path &default_result_dir) {
    const auto it = obj.find("options");
    if (it == obj.end()) {
        return;
    }

    if (it->second.is_object()) {
        apply_task_fields(task, it->second.as_object(), default_result_dir, false);
        return;
    }

    if (!it->second.is_string()) {
        return;
    }

    const std::string options = trim_copy(it->second.as_string());
    if (options.empty()) {
        return;
    }

    if (options.front() == '{') {
        try {
            const JsonValue parsed = JsonValue::parse(options);
            if (parsed.is_object()) {
                apply_task_fields(task, parsed.as_object(), default_result_dir, false);
                return;
            }
        } catch (...) {
            // Treat non-JSON options as a shell command below.
        }
    }

    if (task.command.empty()) {
        task.command = options;
    }
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
    apply_task_fields(task, obj, default_result_dir, true);
    apply_options_field(task, obj, default_result_dir);

    if (task.working_directory.empty()) {
        task.working_directory = default_result_dir.string();
    }

    if (task.session_id.empty()) {
        throw std::runtime_error("task response missing session_id");
    }
    if (task.task_code.empty()) {
        throw std::runtime_error("task response missing task_code");
    }

    return task;
}

ExecutionResult execute_task(const TaskInstruction &task, const fs::path &task_root, const fs::path &result_root) {
    const fs::path task_root_abs = fs::absolute(task_root);
    const fs::path result_root_abs = fs::absolute(result_root);
    fs::create_directories(task_root_abs);
    fs::create_directories(result_root_abs);

    const auto started_at = fs::file_time_type::clock::now();
    const bool file_only_task = upper_copy(task.task_code) == "FILE" && task.command.empty();
    if (file_only_task) {
        ExecutionResult result;
        result.files = wait_for_generated_files(result_root_abs, task.result_files, started_at, task.result_wait_timeout_sec);
        result.result_code = result.files.empty() && !task.result_files.empty() ? -4 : 0;
        result.message = result.result_code == 0 ? "files collected" : "requested result files were not found";
        return result;
    }

    if (task.command.empty()) {
        ExecutionResult result;
        result.result_code = -2;
        result.message = "task command is empty";
        return result;
    }

    const std::string session_name = safe_session_name(task.session_id);
    const fs::path stdout_path = task_root_abs / (session_name + ".stdout.log");
    const fs::path stderr_path = task_root_abs / (session_name + ".stderr.log");
    const auto working_directory = resolve_working_directory(task.working_directory, result_root_abs);
    if (!working_directory.has_value()) {
        ExecutionResult result;
        result.result_code = -5;
        result.message = "working_directory must stay inside result_directory";
        return result;
    }
    fs::create_directories(*working_directory);

#ifdef _WIN32
    std::string shell_command = "cmd /C \"cd /D " + win_quote(working_directory->string()) + " && " + task.command +
                                " > " + win_quote(stdout_path.string()) + " 2> " + win_quote(stderr_path.string()) + "\"";
#else
    std::string shell_command = "cd " + shell_escape(working_directory->string()) + " && ";
    shell_command += task.command + " > " + shell_escape(stdout_path.string()) + " 2> " + shell_escape(stderr_path.string());
    shell_command = "/bin/sh -lc " + shell_escape(shell_command);
#endif

    const int status = std::system(shell_command.c_str());
    const int exit_code = normalize_exit_code(status);

    ExecutionResult result;
    result.result_code = exit_code;
    result.files = wait_for_generated_files(result_root_abs, task.result_files, started_at, task.result_wait_timeout_sec);
    result.message = join_output(read_text_file(stdout_path), read_text_file(stderr_path));

    if (result.result_code != 0 && result.message.empty()) {
        result.message = "task failed with exit code " + std::to_string(result.result_code);
    }

    return result;
}

} // namespace web_agent

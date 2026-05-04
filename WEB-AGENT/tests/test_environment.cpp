#include "../src/config.h"
#include "../src/json.h"
#include "../src/task_executor.h"

#include <cassert>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace web_agent;

int main() {
    {
        const JsonValue root = JsonValue::parse("{\"status\":\"RUN\",\"files\":[\"a.txt\",\"b.txt\"],\"code\":1}");
        assert(root.is_object());
        assert(root.find("status") != nullptr);
        assert(root.find("status")->as_string() == "RUN");
        assert(root.find("files")->as_array().size() == 2);
    }

    {
        const JsonValue root = JsonValue::parse("{\"msg\":\"\\u041e\\u043a\"}");
        assert(root.find("msg")->as_string() == u8"\u041e\u043a");
    }

    {
        const fs::path config_path = fs::temp_directory_path() / "web_agent_test_config.json";
        std::ofstream(config_path) << R"({
          "uid": "test-agent",
          "server_url": "https://example.com",
          "access_code": "secret",
          "poll_interval_sec": 7
        })";
        const AgentConfig config = load_config(config_path.string());
        assert(config.uid == "test-agent");
        assert(config.server_url == "https://example.com");
        assert(config.access_code == "secret");
        assert(config.access_code_env == "WEB_AGENT_ACCESS_CODE");
        assert(config.register_path == "/app/webagent1/api/wa_reg/");
        assert(config.poll_interval_sec == 7);
        fs::remove(config_path);
    }

    {
        const fs::path config_path = fs::temp_directory_path() / "web_agent_legacy_config.json";
        std::ofstream(config_path) << R"({
          "uid": "legacy-agent",
          "server": "example.com",
          "port": 8443,
          "interval": 9
        })";
        const AgentConfig config = load_config(config_path.string());
        assert(config.server_url == "https://example.com:8443");
        assert(config.poll_interval_sec == 9);
        fs::remove(config_path);
    }

    {
        const fs::path config_path = fs::temp_directory_path() / "web_agent_http_config.json";
        std::ofstream(config_path) << R"({
          "uid": "http-agent",
          "server_url": "http://example.com"
        })";
        bool rejected = false;
        try {
            (void)load_config(config_path.string());
        } catch (const std::exception &) {
            rejected = true;
        }
        assert(rejected);
        fs::remove(config_path);
    }

    {
        const fs::path config_path = fs::temp_directory_path() / "web_agent_http_allowed_config.json";
        std::ofstream(config_path) << R"({
          "uid": "http-agent",
          "server_url": "http://example.com",
          "allow_insecure_http": true
        })";
        const AgentConfig config = load_config(config_path.string());
        assert(config.allow_insecure_http);
        assert(config.server_url == "http://example.com");
        fs::remove(config_path);
    }

    {
        const fs::path config_path = fs::temp_directory_path() / "web_agent_invalid_env_config.json";
        std::ofstream(config_path) << R"({
          "uid": "invalid-env-agent",
          "server_url": "https://example.com",
          "access_code_env": "INVALID-NAME"
        })";
        bool rejected = false;
        try {
            (void)load_config(config_path.string());
        } catch (const std::exception &) {
            rejected = true;
        }
        assert(rejected);
        fs::remove(config_path);
    }

    {
        const JsonValue task_json = JsonValue::parse(R"({
          "status":"RUN",
          "session_id":"session-task",
          "task_code":"TASK",
          "command":"echo task-ok",
          "result_wait_timeout_sec":0
        })");
        const fs::path root = fs::temp_directory_path() / "web_agent_task_mode_test";
        fs::create_directories(root / "tasks");
        fs::create_directories(root / "results");
        const TaskInstruction task = parse_task_instruction(task_json, root / "results");
        const ExecutionResult result = execute_task(task, root / "tasks", root / "results");
        assert(result.result_code == 0);
        assert(result.message.find("task-ok") != std::string::npos);
        fs::remove_all(root);
    }

    {
        const JsonValue task_json = JsonValue::parse(R"({
          "code_responce":"1",
          "status":"RUN",
          "session_id":"session-options",
          "task_code":"CONF",
          "options":"echo ok>generated.txt",
          "result_files":["generated.txt"],
          "result_wait_timeout_sec":0
        })");
        const fs::path root = fs::temp_directory_path() / "web_agent_options_test";
        fs::create_directories(root / "tasks");
        fs::create_directories(root / "results");
        const TaskInstruction task = parse_task_instruction(task_json, root / "results");
        assert(task.command == "echo ok>generated.txt");
        const ExecutionResult result = execute_task(task, root / "tasks", root / "results");
        assert(result.result_code == 0);
        assert(result.files.size() == 1);
        fs::remove_all(root);
    }

    {
        const JsonValue task_json = JsonValue::parse(R"({
          "task_id":"task-42",
          "status":"RUN",
          "session_id":"session-file-option",
          "task_code":"TASK",
          "options":{
            "file":"tool with space.exe",
            "args":["--input","source file.txt"],
            "result_files":["out.txt"]
          }
        })");
        const fs::path root = fs::temp_directory_path() / "web_agent_file_option_test";
        fs::create_directories(root / "results");
        const TaskInstruction task = parse_task_instruction(task_json, root / "results");
        assert(task.task_id == "task-42");
#ifdef _WIN32
        assert(task.command == "\"tool with space.exe\" \"--input\" \"source file.txt\"");
#else
        assert(task.command == "'tool with space.exe' '--input' 'source file.txt'");
#endif
        assert(task.result_files.size() == 1);
        const JsonValue no_result_files_json = JsonValue::parse(R"({
          "status":"RUN",
          "session_id":"session-file-option-no-results",
          "task_code":"TASK",
          "options":{"file":"tool.exe"}
        })");
        const TaskInstruction no_result_files_task = parse_task_instruction(no_result_files_json, root / "results");
        assert(no_result_files_task.result_files.empty());
        const JsonValue command_file_json = JsonValue::parse(R"({
          "status":"RUN",
          "session_id":"session-command-file-result",
          "task_code":"TASK",
          "command":"echo ok",
          "file":"out.txt"
        })");
        const TaskInstruction command_file_task = parse_task_instruction(command_file_json, root / "results");
        assert(command_file_task.result_files.size() == 1);
        fs::remove_all(root);
    }
    {
        const JsonValue task_json = JsonValue::parse(R"({
          "status":"RUN",
          "session_id":"session-1",
          "task_code":"FILE",
          "command":"echo ok>generated.txt",
          "result_files":["generated.txt"],
          "result_wait_timeout_sec":0
        })");
        const fs::path root = fs::temp_directory_path() / "web_agent_task_test";
        fs::create_directories(root / "tasks");
        fs::create_directories(root / "results");
        const TaskInstruction task = parse_task_instruction(task_json, root / "results");
        assert(task.session_id == "session-1");
        const ExecutionResult result = execute_task(task, root / "tasks", root / "results");
        assert(result.result_code == 0);
        assert(result.files.size() == 1);
        fs::remove_all(root);
    }

    {
        const JsonValue task_json = JsonValue::parse(R"({
          "status":"RUN",
          "session_id":"session-relative",
          "task_code":"TASK",
          "command":"echo ok",
          "result_wait_timeout_sec":0
        })");
        const fs::path root = fs::temp_directory_path() / "web_agent_relative_path_test";
        fs::create_directories(root);
        const fs::path previous_cwd = fs::current_path();
        fs::current_path(root);
        const TaskInstruction task = parse_task_instruction(task_json, fs::path("./results"));
        const ExecutionResult result = execute_task(task, fs::path("./tasks"), fs::path("./results"));
        assert(result.result_code == 0);
        fs::current_path(previous_cwd);
        fs::remove_all(root);
    }

    {
        const JsonValue task_json = JsonValue::parse(R"({
          "status":"RUN",
          "session_id":"session-file",
          "task_code":"FILE",
          "result_files":["ready.txt"],
          "result_wait_timeout_sec":0
        })");
        const fs::path root = fs::temp_directory_path() / "web_agent_file_test";
        fs::create_directories(root / "tasks");
        fs::create_directories(root / "results");
        std::ofstream(root / "results" / "ready.txt") << "ready";
        const TaskInstruction task = parse_task_instruction(task_json, root / "results");
        const ExecutionResult result = execute_task(task, root / "tasks", root / "results");
        assert(result.result_code == 0);
        assert(result.files.size() == 1);
        fs::remove_all(root);
    }

    {
        const JsonValue task_json = JsonValue::parse(R"({
          "status":"RUN",
          "session_id":"session-empty",
          "task_code":"TASK",
          "result_wait_timeout_sec":0
        })");
        const fs::path root = fs::temp_directory_path() / "web_agent_empty_test";
        fs::create_directories(root / "tasks");
        fs::create_directories(root / "results");
        const TaskInstruction task = parse_task_instruction(task_json, root / "results");
        const ExecutionResult result = execute_task(task, root / "tasks", root / "results");
        assert(result.result_code < 0);
        fs::remove_all(root);
    }

    {
        const JsonValue task_json = JsonValue::parse(R"({
          "status":"RUN",
          "session_id":"session-unsafe-workdir",
          "task_code":"TASK",
          "command":"echo ok",
          "working_directory":"../outside",
          "result_wait_timeout_sec":0
        })");
        const fs::path root = fs::temp_directory_path() / "web_agent_unsafe_workdir_test";
        fs::create_directories(root / "tasks");
        fs::create_directories(root / "results");
        const TaskInstruction task = parse_task_instruction(task_json, root / "results");
        const ExecutionResult result = execute_task(task, root / "tasks", root / "results");
        assert(result.result_code == -5);
        fs::remove_all(root);
    }

#ifndef _WIN32
    {
        const JsonValue task_json = JsonValue::parse(R"({
          "status":"RUN",
          "session_id":"session-symlink-file",
          "task_code":"FILE",
          "result_files":["escape-link"],
          "result_wait_timeout_sec":0
        })");
        const fs::path root = fs::temp_directory_path() / "web_agent_symlink_file_test";
        fs::create_directories(root / "tasks");
        fs::create_directories(root / "results");
        const fs::path outside_file = root / "outside-secret.txt";
        std::ofstream(outside_file) << "secret";
        fs::create_symlink(outside_file, root / "results" / "escape-link");
        const TaskInstruction task = parse_task_instruction(task_json, root / "results");
        const ExecutionResult result = execute_task(task, root / "tasks", root / "results");
        assert(result.result_code == -4);
        assert(result.files.empty());
        fs::remove_all(root);
    }
#endif

    return 0;
}

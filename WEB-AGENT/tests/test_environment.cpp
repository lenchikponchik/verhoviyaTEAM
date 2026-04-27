#include "../src/config.h"
#include "../src/json.h"
#include "../src/task_executor.h"

#include <cassert>
#include <filesystem>
#include <fstream>

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
        assert(config.poll_interval_sec == 7);
        fs::remove(config_path);
    }

    {
        const JsonValue task_json = JsonValue::parse(R"({
          "status":"RUN",
          "session_id":"session-1",
          "task_code":"FILE",
          "command":"printf 'ok' > generated.txt",
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

    return 0;
}

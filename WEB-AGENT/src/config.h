#pragma once

#include <string>

namespace web_agent {

struct AgentConfig {
    std::string uid;
    std::string description = "web-agent";
    std::string server_url;
    std::string access_code;
    std::string access_code_env = "WEB_AGENT_ACCESS_CODE";
    std::string poll_path = "/app/webagent1/api/wa_task/";
    std::string result_path = "/app/webagent1/api/wa_result/";
    std::string register_path = "/app/webagent1/api/wa_reg/";
    std::string task_directory = "./tasks";
    std::string result_directory = "./results";
    std::string log_file = "./agent.log";
    std::string ca_cert_file;
    std::string ca_cert_dir;
    int poll_interval_sec = 5;
    int request_timeout_sec = 15;
    int max_parallel_tasks = 4;
    int backoff_max_sec = 60;
    bool register_on_startup = true;
    bool allow_insecure_http = false;
};

AgentConfig load_config(const std::string &path);

} // namespace web_agent

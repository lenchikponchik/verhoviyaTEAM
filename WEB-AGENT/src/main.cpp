#include "api_client.h"
#include "config.h"
#include "logger.h"

#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace web_agent;

#ifndef WEB_AGENT_DEFAULT_CONFIG
#define WEB_AGENT_DEFAULT_CONFIG "config/agent_config.json"
#endif

#ifndef WEB_AGENT_VERSION
#define WEB_AGENT_VERSION "dev"
#endif

#ifndef WEB_AGENT_PLATFORM
#define WEB_AGENT_PLATFORM "unknown-platform"
#endif

namespace {

class SessionGuard {
public:
    SessionGuard(std::set<std::string> &active_sessions, std::mutex &mutex, std::string session_id)
        : active_sessions_(active_sessions), mutex_(mutex), session_id_(std::move(session_id)) {}

    ~SessionGuard() {
        std::lock_guard<std::mutex> lock(mutex_);
        active_sessions_.erase(session_id_);
    }

private:
    std::set<std::string> &active_sessions_;
    std::mutex &mutex_;
    std::string session_id_;
};

void prune_completed(
    std::vector<std::future<void>> &workers) {
    for (auto it = workers.begin(); it != workers.end();) {
        if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            it->get();
            it = workers.erase(it);
        } else {
            ++it;
        }
    }
}

struct CommandLineOptions {
    std::string config_path = WEB_AGENT_DEFAULT_CONFIG;
    bool show_help = false;
    bool show_version = false;
};

void print_usage(const char *program_name) {
    std::cout << "Usage: " << program_name << " [--config <file>] [--version]" << std::endl;
    std::cout << "       " << program_name << " <config-file>" << std::endl;
}

CommandLineOptions parse_command_line(int argc, char **argv) {
    CommandLineOptions options;
    bool config_was_set = false;

    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--help" || arg == "-h") {
            options.show_help = true;
            continue;
        }
        if (arg == "--version") {
            options.show_version = true;
            continue;
        }
        if (arg == "--config" || arg == "-c") {
            if (index + 1 >= argc) {
                throw std::runtime_error("missing file path after " + arg);
            }
            options.config_path = argv[++index];
            config_was_set = true;
            continue;
        }
        if (arg.rfind("--config=", 0) == 0) {
            options.config_path = arg.substr(std::string("--config=").size());
            if (options.config_path.empty()) {
                throw std::runtime_error("--config requires a non-empty file path");
            }
            config_was_set = true;
            continue;
        }
        if (!arg.empty() && arg.front() == '-') {
            throw std::runtime_error("unknown option: " + arg);
        }
        if (config_was_set) {
            throw std::runtime_error("config file is specified more than once");
        }
        options.config_path = arg;
        config_was_set = true;
    }

    return options;
}

} // namespace

#ifndef _WIN32
void secure_directory_permissions(const fs::path &path) {
    std::error_code ec;
    fs::permissions(path, fs::perms::owner_all, fs::perm_options::replace, ec);
    if (ec) {
        throw std::runtime_error("cannot set secure permissions for directory: " + path.string());
    }
}
#endif

int main(int argc, char **argv) {
    try {
        const CommandLineOptions cli = parse_command_line(argc, argv);
        if (cli.show_help) {
            print_usage(argv[0]);
            return 0;
        }
        if (cli.show_version) {
            std::cout << "web-agent " << WEB_AGENT_VERSION << " (" << WEB_AGENT_PLATFORM << ")" << std::endl;
            return 0;
        }

        const std::string config_path = cli.config_path;
        AgentConfig config = load_config(config_path);

        fs::create_directories(config.task_directory);
        fs::create_directories(config.result_directory);
#ifndef _WIN32
        secure_directory_permissions(config.task_directory);
        secure_directory_permissions(config.result_directory);
#endif

        Logger logger(config.log_file);
        logger.info("agent startup, UID=" + config.uid);

        ApiClient client(config, logger);
        if (!client.probe_server()) {
            logger.error("server is unavailable on startup");
        }

        if (config.register_on_startup || config.access_code.empty()) {
            if (!client.register_agent()) {
                logger.error("agent registration failed and no usable access_code available");
                return 1;
            }
        }

        std::vector<std::future<void>> workers;
        std::set<std::string> active_sessions;
        std::mutex active_sessions_mutex;
        int current_interval = config.poll_interval_sec;

        while (true) {
            prune_completed(workers);

            if (static_cast<int>(workers.size()) >= config.max_parallel_tasks) {
                logger.debug("max parallel tasks reached, waiting for free slot");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            try {
                std::optional<TaskInstruction> task = client.fetch_task();
                if (!task.has_value()) {
                    logger.debug("no task available");
                    current_interval = config.poll_interval_sec;
                    std::this_thread::sleep_for(std::chrono::seconds(current_interval));
                    continue;
                }

                {
                    std::lock_guard<std::mutex> lock(active_sessions_mutex);
                    if (active_sessions.find(task->session_id) != active_sessions.end()) {
                        logger.error("duplicate session skipped: " + task->session_id);
                        task.reset();
                    } else {
                        active_sessions.insert(task->session_id);
                    }
                }

                if (!task.has_value()) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                logger.info("task received: session=" + task->session_id + ", type=" + task->task_code);

                workers.push_back(std::async(std::launch::async, [&client, &logger, &config, &active_sessions, &active_sessions_mutex, task]() {
                    SessionGuard session_guard(active_sessions, active_sessions_mutex, task->session_id);
                    try {
                        const ExecutionResult result = execute_task(
                            *task,
                            fs::path(config.task_directory),
                            fs::path(config.result_directory));
                        logger.info("task finished: session=" + task->session_id + ", code=" + std::to_string(result.result_code));
                        if (!client.send_result(*task, result)) {
                            logger.error("failed to deliver result for session " + task->session_id);
                        }
                    } catch (const std::exception &ex) {
                        logger.error("task execution failed for session " + task->session_id + ": " + ex.what());
                        ExecutionResult error_result;
                        error_result.result_code = -1;
                        error_result.message = ex.what();
                        client.send_result(*task, error_result);
                    }
                }));

                current_interval = config.poll_interval_sec;
            } catch (const std::exception &ex) {
                logger.error(std::string("polling error: ") + ex.what());
                current_interval = std::min(current_interval * 2, config.backoff_max_sec);
            }

            std::this_thread::sleep_for(std::chrono::seconds(current_interval));
        }
    } catch (const std::exception &ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return 1;
    }
}

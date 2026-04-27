#include "api_client.h"
#include "config.h"
#include "logger.h"

#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace web_agent;

namespace {

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

} // namespace

int main(int argc, char **argv) {
    try {
        const std::string config_path = argc > 1 ? argv[1] : "config/agent_config.json";
        AgentConfig config = load_config(config_path);

        fs::create_directories(config.task_directory);
        fs::create_directories(config.result_directory);

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
                auto task = client.fetch_task();
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
                    std::lock_guard<std::mutex> lock(active_sessions_mutex);
                    active_sessions.erase(task->session_id);
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

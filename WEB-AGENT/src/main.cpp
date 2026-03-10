#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {

    httplib::SSLClient cli("xdev.arkcom.ru", 9999);

    std::string uid = "777";
    std::string access_code = "e8725a-44a6-4a80-d26c-27dd5a84";

    while (true) {

        std::cout << "\nChecking task...\n";

        std::string task_json =
        "{"
        "\"UID\":\"" + uid + "\","
        "\"descr\":\"web-agent\","
        "\"access_code\":\"" + access_code + "\""
        "}";

        auto task = cli.Post(
            "/app/webagent1/api/wa_task/",
            task_json,
            "application/json"
        );

        if (!task) {
            std::cout << "Task request error\n";
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        std::cout << "SERVER RESPONSE:\n" << task->body << std::endl;

        if (task->body.find("\"status\":\"RUN\"") != std::string::npos) {

            std::string session_id;

            size_t pos = task->body.find("session_id");
            if (pos != std::string::npos) {
                size_t start = task->body.find("\"", pos + 12) + 1;
                size_t end = task->body.find("\"", start);
                session_id = task->body.substr(start, end - start);
            }

            std::cout << "SESSION: " << session_id << std::endl;

            bool is_file = task->body.find("\"task_code\":\"FILE\"") != std::string::npos;

            std::this_thread::sleep_for(std::chrono::seconds(2));

            int files_count = is_file ? 1 : 0;

            std::string result_json =
            "{"
            "\"UID\":\"" + uid + "\","
            "\"access_code\":\"" + access_code + "\","
            "\"message\":\"task completed\","
            "\"files\":" + std::to_string(files_count) + ","
            "\"session_id\":\"" + session_id + "\""
            "}";

            httplib::UploadFormDataItems items;

            items.push_back({ "result_code", "0", "", "" });
            items.push_back({ "result", result_json, "", "" });

            if (is_file) {
                items.push_back({ "file1", "example file content", "result.txt", "text/plain" });
            }

            auto res = cli.Post(
                "/app/webagent1/api/wa_result/",
                items
            );

            if (res)
                std::cout << "RESULT RESPONSE:\n" << res->body << std::endl;
            else
                std::cout << "Result send error\n";

            std::cout << "Task completed\n";
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}
cmake_minimum_required(VERSION 3.16)
project(WebAgent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(web_agent
    src/api_client.cpp
    src/config.cpp
    src/json.cpp
    src/logger.cpp
    src/main.cpp
    src/task_executor.cpp
)

target_link_libraries(web_agent ssl crypto)

enable_testing()
add_subdirectory(tests)

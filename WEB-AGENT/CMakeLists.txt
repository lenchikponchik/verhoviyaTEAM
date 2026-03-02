cmake_minimum_required(VERSION 3.16)
project(WebAgent)

set(CMAKE_CXX_STANDARD 17)

add_executable(web_agent src/main.cpp)

enable_testing()
add_subdirectory(tests)
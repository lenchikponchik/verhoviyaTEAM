add_executable(test_environment
    test_environment.cpp
    ../src/config.cpp
    ../src/json.cpp
    ../src/task_executor.cpp
)
add_test(NAME EnvironmentTest COMMAND test_environment)

cmake_minimum_required(VERSION 3.16)
project(WebAgent VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

option(WEB_AGENT_BUILD_TESTS "Build WEB-AGENT tests" ON)
option(WEB_AGENT_ENABLE_INSTALL "Enable install and CPack packaging rules" ON)
option(WEB_AGENT_STRICT_WARNINGS "Enable strict compiler warnings" OFF)

set(WEB_AGENT_DEFAULT_CONFIG "config/agent_config.json" CACHE FILEPATH "Default JSON config file used when --config is not passed")
set(WEB_AGENT_PACKAGE_PLATFORM "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}" CACHE STRING "Platform suffix used in package names")
set(WEB_AGENT_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_SYSCONFDIR}/web-agent" CACHE PATH "Install directory for the example config")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)

add_executable(web_agent
    src/api_client.cpp
    src/config.cpp
    src/json.cpp
    src/logger.cpp
    src/main.cpp
    src/task_executor.cpp
)

target_compile_features(web_agent PRIVATE cxx_std_17)
target_compile_definitions(web_agent PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    WEB_AGENT_VERSION="${PROJECT_VERSION}"
    WEB_AGENT_PLATFORM="${WEB_AGENT_PACKAGE_PLATFORM}"
    WEB_AGENT_DEFAULT_CONFIG="${WEB_AGENT_DEFAULT_CONFIG}"
)
target_link_libraries(web_agent PRIVATE OpenSSL::SSL OpenSSL::Crypto)

set_target_properties(web_agent PROPERTIES OUTPUT_NAME "web-agent")

if(WEB_AGENT_STRICT_WARNINGS)
    if(MSVC)
        target_compile_options(web_agent PRIVATE /W4 /permissive-)
    else()
        target_compile_options(web_agent PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(WEB_AGENT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(WEB_AGENT_ENABLE_INSTALL)
    install(TARGETS web_agent
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(FILES config/agent_config.example.json
        DESTINATION ${WEB_AGENT_CONFIG_INSTALL_DIR}
        RENAME agent_config.json
    )
    install(DIRECTORY docs/
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
    )
    install(DIRECTORY deploy/
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/deploy
    )

    set(CPACK_PACKAGE_NAME "web-agent")
    set(CPACK_PACKAGE_VENDOR "verhoviyaTEAM")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "web-agent-${PROJECT_VERSION}-${WEB_AGENT_PACKAGE_PLATFORM}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform WEB-AGENT executable")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE")
    set(CPACK_GENERATOR "TGZ")
    if(WIN32)
        set(CPACK_GENERATOR "ZIP")
    endif()

    include(CPack)
endif()

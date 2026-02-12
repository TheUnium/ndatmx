cmake_minimum_required(VERSION 3.10)
project(ndatmx)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DAEMON_SOURCES
    daemon/config.cpp
    daemon/ipc.cpp
    daemon/main.cpp
    daemon/mgit.cpp
    daemon/ndxparser.cpp
    daemon/platform.cpp
    daemon/project.cpp
    daemon/runner.cpp
    daemon/scheduler.cpp
)

set(CLI_SOURCES
    cli/main.cpp
)

add_executable(ndatmxd ${DAEMON_SOURCES})
target_link_libraries(ndatmxd pthread)

add_executable(ndatmx ${CLI_SOURCES})
target_link_libraries(ndatmx pthread)

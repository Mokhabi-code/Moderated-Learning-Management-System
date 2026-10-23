cmake_minimum_required(VERSION 3.16)
project(MLMS CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include directories
include_directories(include)

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Core Library (Domain, Services, Persistence)
file(GLOB_RECURSE SOURCES "src/domain/*.cpp" "src/services/*.cpp" "src/persistence/*.cpp")
add_library(mlms_core ${SOURCES})
target_link_libraries(mlms_core PUBLIC SQLite3::SQLite3)

# Main Application (UI and main.cpp)
file(GLOB_RECURSE APP_SOURCES "src/ui/*.cpp" "src/main.cpp")
add_executable(mlms_app ${APP_SOURCES})
target_link_libraries(mlms_app PRIVATE mlms_core)

# Tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
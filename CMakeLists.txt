
cmake_minimum_required(VERSION 3.20)
project(MLMS VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MLMS_BUILD_TESTS "Build MLMS tests" ON)
option(MLMS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

add_library(mlms_core
    src/domain/submission.cpp
    src/services/late_penalty.cpp
    src/persistence/sqlite_connection.cpp
    src/persistence/statement.cpp
    src/persistence/transaction_scope.cpp
    src/persistence/schema_initializer.cpp
    src/persistence/user_repository.cpp
    src/persistence/department_repository.cpp
    src/services/registration_service.cpp
    src/persistence/course_repository.cpp
    src/persistence/enrollment_repository.cpp
    src/persistence/assignment_repository.cpp
    src/persistence/submission_repository.cpp
    src/persistence/mark_repository.cpp
    src/persistence/extension_repository.cpp
    src/persistence/audit_repository.cpp
    src/persistence/notification_repository.cpp
    src/services/authorization_guard.cpp
    src/services/authentication_service.cpp
    src/services/sodium_password_hasher.cpp
)

target_include_directories(mlms_core PUBLIC include)
target_link_libraries(
    mlms_core
    PUBLIC SQLite3::SQLite3 PkgConfig::SODIUM
)

if(MSVC)
    target_compile_options(mlms_core PRIVATE /W4)
    if(MLMS_WARNINGS_AS_ERRORS)
        target_compile_options(mlms_core PRIVATE /WX)
    endif()
else()
    target_compile_options(
        mlms_core PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
    if(MLMS_WARNINGS_AS_ERRORS)
        target_compile_options(mlms_core PRIVATE -Werror)
    endif()
endif()

add_executable(mlms_app src/main.cpp)
target_link_libraries(mlms_app PRIVATE mlms_core)

if(MLMS_BUILD_TESTS)
    enable_testing()
    add_executable(mlms_tests tests/test_main.cpp)
    target_link_libraries(mlms_tests PRIVATE mlms_core)
    add_test(NAME mlms_tests COMMAND mlms_tests)
endif()

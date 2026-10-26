# Moderated Learning Management System (MLMS)

A C++17 console application for managing users, courses,
assignments, submissions, marking, moderation, extensions,
notifications, and audit history.

SQLite is the persistence layer.

## Prerequisites

- CMake 3.20+
- C++17 compiler (GCC, Clang, or Visual Studio 2022)
- SQLite development library
- A vetted Argon2id library for password hashing

## Build

```sh
cmake -S . -B build -DMLMS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/mlms_app
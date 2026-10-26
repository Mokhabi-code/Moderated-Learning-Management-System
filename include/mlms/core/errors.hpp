#pragma once

#include <stdexcept>
#include <string>

namespace mlms {

class ValidationError : public std::runtime_error {
public:
    explicit ValidationError(const std::string& message)
        : std::runtime_error(message) {}
};

class PersistenceError : public std::runtime_error {
public:
    explicit PersistenceError(const std::string& message)
        : std::runtime_error(message) {}
};

class AuthorizationError : public std::runtime_error {
public:
    explicit AuthorizationError(const std::string& message)
        : std::runtime_error(message) {}
};

class InvalidStateTransition : public std::runtime_error {
public:
    explicit InvalidStateTransition(const std::string& message)
        : std::runtime_error(message) {}
};

}
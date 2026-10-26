
#pragma once

#include "mlms/domain/role.hpp"

#include <optional>
#include <string>

namespace mlms {

struct UserRecord {
    int userId{0};
    std::string username;
    std::string passwordHash;
    std::string fullName;
    std::string email;
    Role role{Role::Student};

    std::optional<int> departmentId;
    std::optional<std::string> studentNumber;
    std::optional<std::string> staffNumber;

    bool active{true};
    int failedLoginAttempts{0};
    std::optional<std::string> lockedUntil;
};

} // namespace mlms

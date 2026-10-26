
#pragma once

#include "mlms/persistence/user_repository.hpp"
#include "mlms/security/password_hasher.hpp"

#include <optional>
#include <string>

namespace mlms {

struct AuthenticatedUser {
    int userId;
    std::string username;
    std::string fullName;
    std::string email;
    Role role;
};

class AuthenticationService {
public:
    AuthenticationService(
        UserRepository& users,
        const IPasswordHasher& hasher
    )
        : users_(users),
          hasher_(hasher) {}

    std::optional<AuthenticatedUser> authenticate(
        const std::string& username,
        const std::string& password
    );

private:
    UserRepository& users_;
    const IPasswordHasher& hasher_;
};

}

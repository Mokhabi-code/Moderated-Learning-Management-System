#pragma once

#include "mlms/domain/role.hpp"
#include "mlms/persistence/sqlite_connection.hpp"
#include "mlms/security/password_hasher.hpp"

#include <string>

namespace mlms {

struct RegistrationRequest {
    std::string username;
    std::string password;
    std::string fullName;
    std::string email;
    std::string departmentId;
    std::string studentNumber;
    std::string staffNumber;
};

class RegistrationService {
public:
    RegistrationService(
        SqliteConnection& connection,
        const IPasswordHasher& hasher
    )
        : connection_(connection),
          hasher_(hasher) {}

    long long registerStudent(
        const RegistrationRequest& request
    );

private:
    SqliteConnection& connection_;
    const IPasswordHasher& hasher_;
};

}

#pragma once

#include "mlms/domain/user_record.hpp"
#include "mlms/persistence/sqlite_connection.hpp"

#include <optional>
#include <string>
#include <vector>

namespace mlms {

class UserRepository {
public:
    explicit UserRepository(SqliteConnection& connection);

    // Insert a user whose password is already hashed.
    int create(const UserRecord& user);

    // Retrieve users.
    std::optional<UserRecord> findById(int userId);
    std::optional<UserRecord> findByUsername(
        const std::string& username
    );

    std::vector<UserRecord> listAll();

    // Check username uniqueness.
    bool existsByUsername(const std::string& username);

    // Update profile fields without changing the role or password.
    bool updateProfile(
        int userId,
        const std::string& fullName,
        const std::string& email
    );

    // Activate or deactivate an account.
    bool setActive(int userId, bool active);

    // Check whether an account is currently locked.
    bool isLoginLocked(int userId);

    // Record a failed login and lock the account at the threshold.
    bool recordFailedLogin(
        int userId,
        int maxAttempts,
        int lockMinutes
    );

// Clear failed attempts and any existing lock.
bool resetLoginFailures(int userId);

private:
    SqliteConnection& connection_;

    std::optional<UserRecord> readUser(class Statement& statement);
};

} // namespace mlms

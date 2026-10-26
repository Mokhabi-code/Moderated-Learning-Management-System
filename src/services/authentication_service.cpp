
#include "mlms/services/authentication_service.hpp"

namespace mlms {

namespace {

constexpr int MAX_FAILED_ATTEMPTS = 5;
constexpr int LOCK_DURATION_MINUTES = 15;

} // namespace

std::optional<AuthenticatedUser>
AuthenticationService::authenticate(
    const std::string& username,
    const std::string& password
) {
    if (username.empty() || password.empty()) {
        return std::nullopt;
    }

    const auto user = users_.findByUsername(username);

    if (!user.has_value()) {
        return std::nullopt;
    }

    if (!user->active) {
        return std::nullopt;
    }

    // Reject login while the account is locked.
    if (users_.isLoginLocked(user->userId)) {
        return std::nullopt;
    }

    // Clear an expired lock and its previous failure count.
    if (user->lockedUntil.has_value()) {
        users_.resetLoginFailures(user->userId);
    }

    // Record incorrect passwords and apply the lockout policy.
    if (!hasher_.verifyPassword(
            password,
            user->passwordHash)) {
        users_.recordFailedLogin(
            user->userId,
            MAX_FAILED_ATTEMPTS,
            LOCK_DURATION_MINUTES
        );

        return std::nullopt;
    }

    // Successful login resets the consecutive failure count.
    users_.resetLoginFailures(user->userId);

    return AuthenticatedUser{
        user->userId,
        user->username,
        user->fullName,
        user->email,
        user->role
    };
}

} // namespace mlms

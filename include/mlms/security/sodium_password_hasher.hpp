#pragma once

#include "mlms/security/password_hasher.hpp"

namespace mlms {

class SodiumPasswordHasher final : public IPasswordHasher {
public:
    SodiumPasswordHasher();

    std::string hashPassword(
        const std::string& password
    ) const override;

    bool verifyPassword(
        const std::string& password,
        const std::string& encodedHash
    ) const override;
};

} // namespace mlms

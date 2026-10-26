#pragma once

#include <string>

namespace mlms {

// A concrete implementation must use a vetted password-hashing
// library such as Argon2id.
class IPasswordHasher {
public:
    virtual ~IPasswordHasher() = default;

    virtual std::string hashPassword(
        const std::string& password
    ) const = 0;

    virtual bool verifyPassword(
        const std::string& password,
        const std::string& encodedHash
    ) const = 0;
};

}
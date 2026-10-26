#include "mlms/security/sodium_password_hasher.hpp"
#include "mlms/core/errors.hpp"

#include <sodium.h>
#include <stdexcept>
#include <string>

namespace mlms {

SodiumPasswordHasher::SodiumPasswordHasher() {
    if (sodium_init() < 0) {
        throw std::runtime_error(
            "Unable to initialize libsodium"
        );
    }
}

std::string SodiumPasswordHasher::hashPassword(
    const std::string& password
) const {
    if (password.empty()) {
        throw ValidationError("Password cannot be empty");
    }

    char hash[crypto_pwhash_STRBYTES];

    const int result = crypto_pwhash_str(
        hash,
        password.c_str(),
        static_cast<unsigned long long>(password.size()),
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE
    );

    if (result != 0) {
        throw PersistenceError(
            "Password hashing failed; insufficient resources"
        );
    }

    return std::string(hash);
}

bool SodiumPasswordHasher::verifyPassword(
    const std::string& password,
    const std::string& encodedHash
) const {
    if (password.empty() || encodedHash.empty()) {
        return false;
    }

    return crypto_pwhash_str_verify(
        encodedHash.c_str(),
        password.c_str(),
        static_cast<unsigned long long>(password.size())
    ) == 0;
}

} // namespace mlms

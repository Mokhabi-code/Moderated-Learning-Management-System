#include "mlms/services/registration_service.hpp"
#include "mlms/core/errors.hpp"
#include "mlms/persistence/statement.hpp"
#include "mlms/persistence/transaction_scope.hpp"

#include <sqlite3.h>
#include <string>

namespace mlms {

namespace {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\n\r");

    if (first == std::string::npos) {
        return "";
    }

    const auto last = value.find_last_not_of(" \t\n\r");
    return value.substr(first, last - first + 1);
}

void validate(const RegistrationRequest& request) {
    const std::string username = trim(request.username);
    const std::string fullName = trim(request.fullName);
    const std::string email = trim(request.email);
    const std::string studentNumber = trim(request.studentNumber);

    if (username.size() < 3 || username.size() > 50) {
        throw ValidationError(
            "Username must contain between 3 and 50 characters."
        );
    }

    if (request.password.size() < 12) {
        throw ValidationError(
            "Password must contain at least 12 characters."
        );
    }

    if (fullName.empty() || fullName.size() > 120) {
        throw ValidationError("Enter a valid full name.");
    }

    const auto at = email.find('@');

    if (email.empty()
        || email.size() > 254
        || at == std::string::npos
        || at == 0
        || at == email.size() - 1) {
        throw ValidationError("Enter a valid email address.");
    }

    if (studentNumber.empty()) {
        throw ValidationError(
            "Student number is required for student registration."
        );
    }
}

}

long long RegistrationService::registerStudent(
    const RegistrationRequest& request
) {
    validate(request);

    const std::string username = trim(request.username);
    const std::string fullName = trim(request.fullName);
    const std::string email = trim(request.email);
    const std::string studentNumber = trim(request.studentNumber);
    const std::string encodedHash =
        hasher_.hashPassword(request.password);

    if (encodedHash.empty()
        || encodedHash == request.password) {

        throw PersistenceError(
            "Password hashing failed."
        );
    }

    TransactionScope transaction(connection_);

    Statement insert(
        connection_,
        "INSERT INTO user "
        "(username, password_hash, full_name, email, "
        "role, dept_id, student_no, staff_no, active) "
        "VALUES (?, ?, ?, ?, 'Student', ?, ?, NULL, 1);"
    );

    insert.bind(1, username);
    insert.bind(2, encodedHash);
    insert.bind(3, fullName);
    insert.bind(4, email);

    if (request.departmentId.empty()) {
        insert.bindNull(5);
    } else {
        int departmentId;

        try {
            std::size_t charactersUsed = 0;

            departmentId = std::stoi(
                request.departmentId,
                &charactersUsed
            );

            if (charactersUsed != request.departmentId.size()
                || departmentId <= 0) {

                throw ValidationError(
                    "Department selection is invalid."
                );
            }
        } catch (const ValidationError&) {
            throw;
        } catch (...) {
            throw ValidationError(
                "Department selection is invalid."
            );
        }

        insert.bind(5, departmentId);
    }

    insert.bind(6, studentNumber);

    insert.execute();

    const long long id =
        sqlite3_last_insert_rowid(connection_.handle());

    transaction.commit();

    return id;
}

}
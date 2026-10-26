#include "mlms/persistence/extension_repository.hpp"

#include "mlms/core/errors.hpp"
#include "mlms/persistence/statement.hpp"

#include <cctype>
#include <limits>
#include <sqlite3.h>
#include <string>

namespace mlms {

namespace {

bool isBlank(const std::string& value) {
    for (unsigned char character : value) {
        if (!std::isspace(character)) {
            return false;
        }
    }

    return true;
}

} // namespace

ExtensionRepository::ExtensionRepository(SqliteConnection& connection)
    : connection_(connection) {}

int ExtensionRepository::create(
    int assignmentId,
    int studentId,
    int grantedBy,
    const std::string& newDeadline,
    const std::string& reason,
    const std::string& grantedAt
) {
    if (assignmentId <= 0 || studentId <= 0 || grantedBy <= 0) {
        throw ValidationError(
            "Assignment, student and grantor IDs must be positive."
        );
    }

    if (isBlank(newDeadline) || isBlank(grantedAt)) {
        throw ValidationError(
            "The new deadline and grant date are required."
        );
    }

    if (isBlank(reason)) {
        throw ValidationError("The extension reason is required.");
    }

    Statement statement(
        connection_,
        "INSERT INTO extension "
        "(assignment_id, student_id, granted_by, new_deadline, reason, granted_at) "
        "VALUES (?, ?, ?, ?, ?, ?);"
    );

    statement.bind(1, assignmentId);
    statement.bind(2, studentId);
    statement.bind(3, grantedBy);
    statement.bind(4, newDeadline);
    statement.bind(5, reason);
    statement.bind(6, grantedAt);

    statement.execute();

    const sqlite3_int64 id =
        sqlite3_last_insert_rowid(connection_.handle());

    if (id <= 0 || id > std::numeric_limits<int>::max()) {
        throw PersistenceError(
            "Created extension ID is outside the supported range."
        );
    }

    return static_cast<int>(id);
}

std::optional<ExtensionRecord> ExtensionRepository::readExtension(
    Statement& statement
) {
    if (!statement.step()) {
        return std::nullopt;
    }

    ExtensionRecord extension;

    extension.extensionId = statement.columnInt(0);
    extension.assignmentId = statement.columnInt(1);
    extension.studentId = statement.columnInt(2);
    extension.grantedBy = statement.columnInt(3);
    extension.newDeadline = statement.columnText(4);
    extension.reason = statement.columnText(5);
    extension.grantedAt = statement.columnText(6);

    return extension;
}

std::optional<ExtensionRecord> ExtensionRepository::findById(
    int extensionId
) {
    if (extensionId <= 0) {
        throw ValidationError("Extension ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT extension_id, assignment_id, student_id, granted_by, "
        "new_deadline, reason, granted_at "
        "FROM extension WHERE extension_id = ?;"
    );

    statement.bind(1, extensionId);

    return readExtension(statement);
}

std::vector<ExtensionRecord> ExtensionRepository::listByStudent(
    int studentId
) {
    if (studentId <= 0) {
        throw ValidationError("Student ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT extension_id, assignment_id, student_id, granted_by, "
        "new_deadline, reason, granted_at "
        "FROM extension WHERE student_id = ? "
        "ORDER BY extension_id;"
    );

    statement.bind(1, studentId);

    std::vector<ExtensionRecord> extensions;

    while (auto extension = readExtension(statement)) {
        extensions.push_back(*extension);
    }

    return extensions;
}

std::vector<ExtensionRecord> ExtensionRepository::listByAssignment(
    int assignmentId
) {
    if (assignmentId <= 0) {
        throw ValidationError("Assignment ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT extension_id, assignment_id, student_id, granted_by, "
        "new_deadline, reason, granted_at "
        "FROM extension WHERE assignment_id = ? "
        "ORDER BY extension_id;"
    );

    statement.bind(1, assignmentId);

    std::vector<ExtensionRecord> extensions;

    while (auto extension = readExtension(statement)) {
        extensions.push_back(*extension);
    }

    return extensions;
}

} // namespace mlms
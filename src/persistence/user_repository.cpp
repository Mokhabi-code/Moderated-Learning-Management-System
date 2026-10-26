
#include "mlms/persistence/user_repository.hpp"

#include "mlms/core/errors.hpp"
#include "mlms/persistence/statement.hpp"

#include <limits>
#include <string>
#include <string_view>

#include <sqlite3.h>

namespace mlms {

namespace {

Role parseRole(const std::string& value) {
    if (value == "Student") {
        return Role::Student;
    }

    if (value == "Lecturer") {
        return Role::Lecturer;
    }

    if (value == "HoD") {
        return Role::HoD;
    }

    if (value == "Administrator") {
        return Role::Administrator;
    }

    throw PersistenceError("Database contains an invalid user role.");
}

void bindOptionalInt(
    Statement& statement,
    int index,
    const std::optional<int>& value
) {
    if (value.has_value()) {
        statement.bind(index, *value);
    } else {
        statement.bindNull(index);
    }
}

void bindOptionalText(
    Statement& statement,
    int index,
    const std::optional<std::string>& value
) {
    if (value.has_value()) {
        statement.bind(index, *value);
    } else {
        statement.bindNull(index);
    }
}

std::optional<std::string> readOptionalText(
    const Statement& statement,
    int column
) {
    if (statement.columnIsNull(column)) {
        return std::nullopt;
    }

    return statement.columnText(column);
}

constexpr std::string_view USER_COLUMNS =
    "user_id, username, password_hash, full_name, email, "
    "role, dept_id, student_no, staff_no, active, "
    "failed_login_attempts, locked_until";

} // namespace

UserRepository::UserRepository(SqliteConnection& connection)
    : connection_(connection) {
}

int UserRepository::create(const UserRecord& user) {
    if (user.username.empty()
        || user.passwordHash.empty()
        || user.fullName.empty()
        || user.email.empty()) {
        throw ValidationError(
            "Username, password hash, full name and email are required."
        );
    }

    const std::string sql =
        "INSERT INTO user "
        "(username, password_hash, full_name, email, role, "
        "dept_id, student_no, staff_no, active, "
        "failed_login_attempts, locked_until) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    Statement statement(connection_, sql);

    statement.bind(1, user.username);
    statement.bind(2, user.passwordHash);
    statement.bind(3, user.fullName);
    statement.bind(4, user.email);

    statement.bind(
        5,
        std::string(toString(user.role))
    );

    bindOptionalInt(statement, 6, user.departmentId);
    bindOptionalText(statement, 7, user.studentNumber);
    bindOptionalText(statement, 8, user.staffNumber);

    statement.bind(9, user.active ? 1 : 0);
    statement.bind(10, user.failedLoginAttempts);

    if (user.lockedUntil.has_value()) {
        statement.bind(11, *user.lockedUntil);
    } else {
        statement.bindNull(11);
    }

    statement.execute();

    const sqlite3_int64 id =
        sqlite3_last_insert_rowid(connection_.handle());

    if (id <= 0
        || id > std::numeric_limits<int>::max()) {
        throw PersistenceError(
            "Created user ID is outside the supported range."
        );
    }

    return static_cast<int>(id);
}

std::optional<UserRecord> UserRepository::readUser(
    Statement& statement
) {
    if (!statement.step()) {
        return std::nullopt;
    }

    UserRecord user;

    user.userId = statement.columnInt(0);
    user.username = statement.columnText(1);
    user.passwordHash = statement.columnText(2);
    user.fullName = statement.columnText(3);
    user.email = statement.columnText(4);
    user.role = parseRole(statement.columnText(5));

    if (!statement.columnIsNull(6)) {
        user.departmentId = statement.columnInt(6);
    }

    user.studentNumber = readOptionalText(statement, 7);
    user.staffNumber = readOptionalText(statement, 8);

    user.active = statement.columnInt(9) != 0;
    user.failedLoginAttempts = statement.columnInt(10);
    user.lockedUntil = readOptionalText(statement, 11);

    return user;
}

std::optional<UserRecord> UserRepository::findById(int userId) {
    Statement statement(
        connection_,
        "SELECT " + std::string(USER_COLUMNS) +
        " FROM user WHERE user_id = ?;"
    );

    statement.bind(1, userId);

    return readUser(statement);
}

std::optional<UserRecord> UserRepository::findByUsername(
    const std::string& username
) {
    Statement statement(
        connection_,
        "SELECT " + std::string(USER_COLUMNS) +
        " FROM user WHERE username = ?;"
    );

    statement.bind(1, username);

    return readUser(statement);
}

std::vector<UserRecord> UserRepository::listAll() {
    Statement statement(
        connection_,
        "SELECT " + std::string(USER_COLUMNS) +
        " FROM user ORDER BY user_id;"
    );

    std::vector<UserRecord> users;

    while (statement.step()) {
        UserRecord user;

        user.userId = statement.columnInt(0);
        user.username = statement.columnText(1);
        user.passwordHash = statement.columnText(2);
        user.fullName = statement.columnText(3);
        user.email = statement.columnText(4);
        user.role = parseRole(statement.columnText(5));

        if (!statement.columnIsNull(6)) {
            user.departmentId = statement.columnInt(6);
        }

        user.studentNumber = readOptionalText(statement, 7);
        user.staffNumber = readOptionalText(statement, 8);

        user.active = statement.columnInt(9) != 0;
        user.failedLoginAttempts = statement.columnInt(10);
        user.lockedUntil = readOptionalText(statement, 11);

        users.push_back(user);
    }

    return users;
}

bool UserRepository::existsByUsername(
    const std::string& username
) {
    Statement statement(
        connection_,
        "SELECT 1 FROM user WHERE username = ? LIMIT 1;"
    );

    statement.bind(1, username);

    return statement.step();
}

bool UserRepository::updateProfile(
    int userId,
    const std::string& fullName,
    const std::string& email
) {
    if (userId <= 0
        || fullName.empty()
        || email.empty()) {
        throw ValidationError(
            "A valid user ID, full name and email are required."
        );
    }

    Statement statement(
        connection_,
        "UPDATE user SET full_name = ?, email = ? "
        "WHERE user_id = ?;"
    );

    statement.bind(1, fullName);
    statement.bind(2, email);
    statement.bind(3, userId);

    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

bool UserRepository::setActive(int userId, bool active) {
    if (userId <= 0) {
        throw ValidationError("User ID must be positive.");
    }

    Statement statement(
        connection_,
        "UPDATE user SET active = ? WHERE user_id = ?;"
    );

    statement.bind(1, active ? 1 : 0);
    statement.bind(2, userId);

    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

bool UserRepository::isLoginLocked(int userId) {
    if (userId <= 0) {
        throw ValidationError("User ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT CASE "
        "WHEN locked_until IS NOT NULL "
        "AND datetime(locked_until) > datetime('now') "
        "THEN 1 ELSE 0 END "
        "FROM user WHERE user_id = ?;"
    );

    statement.bind(1, userId);

    return statement.step() && statement.columnInt(0) == 1;
}

bool UserRepository::recordFailedLogin(
    int userId,
    int maxAttempts,
    int lockMinutes
) {
    if (userId <= 0 || maxAttempts <= 0 || lockMinutes <= 0) {
        throw ValidationError(
            "User ID, maximum attempts and lock duration must be positive."
        );
    }

    Statement statement(
        connection_,
        "UPDATE user "
        "SET failed_login_attempts = failed_login_attempts + 1, "
        "locked_until = CASE "
        "WHEN failed_login_attempts + 1 >= ? "
        "THEN datetime('now', '+' || ? || ' minutes') "
        "ELSE locked_until END "
        "WHERE user_id = ? "
        "AND (locked_until IS NULL "
        "OR datetime(locked_until) <= datetime('now'));"
    );

    statement.bind(1, maxAttempts);
    statement.bind(2, lockMinutes);
    statement.bind(3, userId);

    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

bool UserRepository::resetLoginFailures(int userId) {
    if (userId <= 0) {
        throw ValidationError("User ID must be positive.");
    }

    Statement statement(
        connection_,
        "UPDATE user "
        "SET failed_login_attempts = 0, locked_until = NULL "
        "WHERE user_id = ?;"
    );

    statement.bind(1, userId);

    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

} // namespace mlms

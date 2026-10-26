#include "mlms/persistence/statement.hpp"
#include "mlms/core/errors.hpp"

namespace mlms {

namespace {

void checkBind(sqlite3* db, int result) {
    if (result != SQLITE_OK) {
        throw PersistenceError(
            "Unable to bind database parameter: "
            + std::string(sqlite3_errmsg(db))
        );
    }
}

}

Statement::Statement(
    SqliteConnection& connection,
    const std::string& sql
) {
    const int result = sqlite3_prepare_v2(
        connection.handle(),
        sql.c_str(),
        -1,
        &statement_,
        nullptr
    );

    if (result != SQLITE_OK) {
        throw PersistenceError(
            "Unable to prepare database operation: "
            + std::string(sqlite3_errmsg(connection.handle()))
        );
    }
}

Statement::~Statement() {
    if (statement_) {
        sqlite3_finalize(statement_);
    }
}

Statement::Statement(Statement&& other) noexcept
    : statement_(other.statement_) {
    other.statement_ = nullptr;
}

Statement& Statement::operator=(
    Statement&& other
) noexcept {
    if (this != &other) {
        if (statement_) {
            sqlite3_finalize(statement_);
        }

        statement_ = other.statement_;
        other.statement_ = nullptr;
    }

    return *this;
}

void Statement::bind(
    int index,
    const std::string& value
) {
    checkBind(
        sqlite3_db_handle(statement_),
        sqlite3_bind_text(
            statement_,
            index,
            value.c_str(),
            static_cast<int>(value.size()),
            SQLITE_TRANSIENT
        )
    );
}

void Statement::bind(int index, int value) {
    checkBind(
        sqlite3_db_handle(statement_),
        sqlite3_bind_int(statement_, index, value)
    );
}

void Statement::bind(int index, double value) {
    checkBind(
        sqlite3_db_handle(statement_),
        sqlite3_bind_double(statement_, index, value)
    );
}

void Statement::bindNull(int index) {
    checkBind(
        sqlite3_db_handle(statement_),
        sqlite3_bind_null(statement_, index)
    );
}

bool Statement::step() {
    const int result = sqlite3_step(statement_);

    if (result == SQLITE_ROW) {
        return true;
    }

    if (result == SQLITE_DONE) {
        return false;
    }

    throw PersistenceError(
        "Database operation failed: "
        + std::string(
            sqlite3_errmsg(sqlite3_db_handle(statement_))
        )
    );
}

void Statement::execute() {
    if (step()) {
        throw PersistenceError(
            "Database operation unexpectedly returned rows."
        );
    }
}

void Statement::reset() {
    const int result = sqlite3_reset(statement_);

    if (result != SQLITE_OK) {
        throw PersistenceError(
            "Unable to reset database statement."
        );
    }

    sqlite3_clear_bindings(statement_);
}

std::string Statement::columnText(int index) const {
    const auto* value = sqlite3_column_text(
        statement_,
        index
    );

    return value
        ? reinterpret_cast<const char*>(value)
        : std::string{};
}

int Statement::columnInt(int index) const {
    return sqlite3_column_int(statement_, index);
}

double Statement::columnDouble(int index) const {
    return sqlite3_column_double(statement_, index);
}

bool Statement::columnIsNull(int index) const {
    return sqlite3_column_type(statement_, index) == SQLITE_NULL;
}

}
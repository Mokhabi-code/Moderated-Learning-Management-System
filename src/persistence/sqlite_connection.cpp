#include "mlms/persistence/sqlite_connection.hpp"
#include "mlms/core/errors.hpp"

#include <utility>

namespace mlms {

SqliteConnection::SqliteConnection(const std::string& path) {
    const int result = sqlite3_open(path.c_str(), &db_);

    if (result != SQLITE_OK) {
        const std::string message =
            db_ ? sqlite3_errmsg(db_)
                : "unable to allocate SQLite connection";

        if (db_) {
            sqlite3_close(db_);
        }

        db_ = nullptr;

        throw PersistenceError(
            "SQLite open failed: " + message
        );
    }

    execute("PRAGMA foreign_keys = ON;");
    execute("PRAGMA busy_timeout = 5000;");
}

SqliteConnection::~SqliteConnection() {
    if (db_) {
        sqlite3_close(db_);
    }
}

SqliteConnection::SqliteConnection(
    SqliteConnection&& other
) noexcept
    : db_(other.db_) {
    other.db_ = nullptr;
}

SqliteConnection& SqliteConnection::operator=(
    SqliteConnection&& other
) noexcept {
    if (this != &other) {
        if (db_) {
            sqlite3_close(db_);
        }

        db_ = other.db_;
        other.db_ = nullptr;
    }

    return *this;
}

void SqliteConnection::execute(const std::string& sql) {
    char* error = nullptr;

    const int result = sqlite3_exec(
        db_,
        sql.c_str(),
        nullptr,
        nullptr,
        &error
    );

    if (result != SQLITE_OK) {
        const std::string message =
            error ? error : sqlite3_errmsg(db_);

        sqlite3_free(error);

        throw PersistenceError(
            "SQLite execute failed: " + message
        );
    }
}

}
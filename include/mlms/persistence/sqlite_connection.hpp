#pragma once

#include <sqlite3.h>
#include <string>

namespace mlms {

class SqliteConnection {
public:
    explicit SqliteConnection(const std::string& path);
    ~SqliteConnection();

    SqliteConnection(const SqliteConnection&) = delete;
    SqliteConnection& operator=(const SqliteConnection&) = delete;

    SqliteConnection(SqliteConnection&& other) noexcept;
    SqliteConnection& operator=(SqliteConnection&& other) noexcept;

    sqlite3* handle() const noexcept {
        return db_;
    }

    void execute(const std::string& sql);

private:
    sqlite3* db_{nullptr};
};

}
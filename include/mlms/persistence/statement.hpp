#pragma once

#include "mlms/persistence/sqlite_connection.hpp"

#include <sqlite3.h>
#include <string>

namespace mlms {

class Statement {
public:
    Statement(
        SqliteConnection& connection,
        const std::string& sql
    );

    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    void bind(int index, const std::string& value);
    void bind(int index, int value);
    void bind(int index, double value);
    void bindNull(int index);

    bool step();
    void execute();
    void reset();

    std::string columnText(int index) const;
    int columnInt(int index) const;
    double columnDouble(int index) const;
    bool columnIsNull(int index) const;

    sqlite3_stmt* handle() const noexcept {
        return statement_;
    }

private:
    sqlite3_stmt* statement_{nullptr};
};

}
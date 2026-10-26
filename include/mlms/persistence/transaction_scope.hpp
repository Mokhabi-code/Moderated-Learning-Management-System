#pragma once

#include "mlms/persistence/sqlite_connection.hpp"

#include <string>

namespace mlms {

class TransactionScope {
public:
    explicit TransactionScope(SqliteConnection& connection);
    ~TransactionScope();

    TransactionScope(const TransactionScope&) = delete;
    TransactionScope& operator=(const TransactionScope&) = delete;

    void commit();
    void rollback();

private:
    SqliteConnection* connection_;
    std::string savepoint_;
    bool outermost_{false};
    bool completed_{false};
};

}
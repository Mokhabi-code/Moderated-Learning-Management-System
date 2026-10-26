#include "mlms/persistence/transaction_scope.hpp"
#include "mlms/core/errors.hpp"

#include <atomic>
#include <utility>

namespace mlms {

namespace {
std::atomic<unsigned long long> savepointCounter{0};
}

TransactionScope::TransactionScope(SqliteConnection& connection)
    : connection_(&connection) {
    // If a transaction is already active, use a savepoint.
    // Otherwise, start a new transaction.
    if (sqlite3_get_autocommit(connection_->handle()) != 0) {
        connection_->execute("BEGIN TRANSACTION;");
        outermost_ = true;
    } else {
        savepoint_ = "mlms_sp_" +
            std::to_string(++savepointCounter);

        connection_->execute("SAVEPOINT " + savepoint_ + ";");
    }
}

TransactionScope::~TransactionScope() {
    if (!completed_ && connection_ != nullptr) {
        try {
            rollback();
        } catch (...) {
            // Destructors must not throw.
        }
    }
}

void TransactionScope::commit() {
    if (completed_) {
        return;
    }

    if (outermost_) {
        connection_->execute("COMMIT;");
    } else {
        connection_->execute("RELEASE SAVEPOINT " + savepoint_ + ";");
    }

    completed_ = true;
}

void TransactionScope::rollback() {
    if (completed_) {
        return;
    }

    if (outermost_) {
        connection_->execute("ROLLBACK;");
    } else {
        connection_->execute("ROLLBACK TO SAVEPOINT " + savepoint_ + ";");
        connection_->execute("RELEASE SAVEPOINT " + savepoint_ + ";");
    }

    completed_ = true;
}

}

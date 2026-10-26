#pragma once

#include "mlms/domain/audit_record.hpp"
#include "mlms/persistence/sqlite_connection.hpp"

#include <optional>
#include <string>
#include <vector>

namespace mlms {

class AuditRepository {
public:
    explicit AuditRepository(SqliteConnection& connection);

    int create(
        int changedBy,
        const std::string& entityType,
        int entityId,
        const std::string& field,
        const std::optional<std::string>& oldValue,
        const std::string& newValue,
        const std::string& reason,
        const std::string& changedAt
    );

    std::optional<AuditRecord> findById(int auditId);

    std::vector<AuditRecord> listByEntity(
        const std::string& entityType,
        int entityId
    );

    std::vector<AuditRecord> listByUser(int changedBy);

private:
    SqliteConnection& connection_;

    std::optional<AuditRecord> readAudit(class Statement& statement);
};

}
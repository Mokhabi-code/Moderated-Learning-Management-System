#include "mlms/persistence/audit_repository.hpp"

#include "mlms/core/errors.hpp"
#include "mlms/persistence/statement.hpp"

#include <cctype>
#include <limits>
#include <sqlite3.h>

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

AuditRepository::AuditRepository(SqliteConnection& connection)
    : connection_(connection) {}

int AuditRepository::create(
    int changedBy,
    const std::string& entityType,
    int entityId,
    const std::string& field,
    const std::optional<std::string>& oldValue,
    const std::string& newValue,
    const std::string& reason,
    const std::string& changedAt
) {
    if (changedBy <= 0 || entityId <= 0) {
        throw ValidationError(
            "Changed-by ID and entity ID must be positive."
        );
    }

    if (isBlank(entityType) || isBlank(field) ||
        isBlank(newValue) || isBlank(reason) || isBlank(changedAt)) {
        throw ValidationError(
            "Entity type, field, new value, reason and timestamp are required."
        );
    }

    Statement statement(
        connection_,
        "INSERT INTO audit_entry "
        "(changed_by, entity_type, entity_id, field, old_value, "
        "new_value, reason, changed_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);"
    );

    statement.bind(1, changedBy);
    statement.bind(2, entityType);
    statement.bind(3, entityId);
    statement.bind(4, field);

    if (oldValue.has_value()) {
        statement.bind(5, *oldValue);
    } else {
        statement.bindNull(5);
    }

    statement.bind(6, newValue);
    statement.bind(7, reason);
    statement.bind(8, changedAt);

    statement.execute();

    const sqlite3_int64 id =
        sqlite3_last_insert_rowid(connection_.handle());

    if (id <= 0 || id > std::numeric_limits<int>::max()) {
        throw PersistenceError(
            "Created audit ID is outside the supported range."
        );
    }

    return static_cast<int>(id);
}

std::optional<AuditRecord> AuditRepository::readAudit(
    Statement& statement
) {
    if (!statement.step()) {
        return std::nullopt;
    }

    AuditRecord record;

    record.auditId = statement.columnInt(0);
    record.changedBy = statement.columnInt(1);
    record.entityType = statement.columnText(2);
    record.entityId = statement.columnInt(3);
    record.field = statement.columnText(4);

    if (!statement.columnIsNull(5)) {
        record.oldValue = statement.columnText(5);
    }

    record.newValue = statement.columnText(6);
    record.reason = statement.columnText(7);
    record.changedAt = statement.columnText(8);

    return record;
}

std::optional<AuditRecord> AuditRepository::findById(int auditId) {
    if (auditId <= 0) {
        throw ValidationError("Audit ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT audit_id, changed_by, entity_type, entity_id, field, "
        "old_value, new_value, reason, changed_at "
        "FROM audit_entry WHERE audit_id = ?;"
    );

    statement.bind(1, auditId);

    return readAudit(statement);
}

std::vector<AuditRecord> AuditRepository::listByEntity(
    const std::string& entityType,
    int entityId
) {
    if (isBlank(entityType)) {
        throw ValidationError("Entity type is required.");
    }

    if (entityId <= 0) {
        throw ValidationError("Entity ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT audit_id, changed_by, entity_type, entity_id, field, "
        "old_value, new_value, reason, changed_at "
        "FROM audit_entry "
        "WHERE entity_type = ? AND entity_id = ? "
        "ORDER BY audit_id;"
    );

    statement.bind(1, entityType);
    statement.bind(2, entityId);

    std::vector<AuditRecord> records;

    while (auto record = readAudit(statement)) {
        records.push_back(*record);
    }

    return records;
}

std::vector<AuditRecord> AuditRepository::listByUser(int changedBy) {
    if (changedBy <= 0) {
        throw ValidationError("Changed-by ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT audit_id, changed_by, entity_type, entity_id, field, "
        "old_value, new_value, reason, changed_at "
        "FROM audit_entry "
        "WHERE changed_by = ? "
        "ORDER BY audit_id;"
    );

    statement.bind(1, changedBy);

    std::vector<AuditRecord> records;

    while (auto record = readAudit(statement)) {
        records.push_back(*record);
    }

    return records;
}

} // namespace mlms
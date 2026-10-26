#pragma once

#include "mlms/domain/extension_record.hpp"
#include "mlms/persistence/sqlite_connection.hpp"

#include <optional>
#include <vector>

namespace mlms {

class ExtensionRepository {
public:
    explicit ExtensionRepository(SqliteConnection& connection);

    int create(
        int assignmentId,
        int studentId,
        int grantedBy,
        const std::string& newDeadline,
        const std::string& reason,
        const std::string& grantedAt
    );

    std::optional<ExtensionRecord> findById(int extensionId);

    std::vector<ExtensionRecord> listByStudent(int studentId);

    std::vector<ExtensionRecord> listByAssignment(int assignmentId);

private:
    SqliteConnection& connection_;

    std::optional<ExtensionRecord> readExtension(
        class Statement& statement
    );
};

}
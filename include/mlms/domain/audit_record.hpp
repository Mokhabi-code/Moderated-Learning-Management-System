#pragma once

#include <optional>
#include <string>

namespace mlms {

struct AuditRecord {
    int auditId{0};
    int changedBy{0};
    std::string entityType;
    int entityId{0};
    std::string field;
    std::optional<std::string> oldValue;
    std::string newValue;
    std::string reason;
    std::string changedAt;
};

}
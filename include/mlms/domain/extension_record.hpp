#pragma once

#include <string>

namespace mlms {

struct ExtensionRecord {
    int extensionId{0};
    int assignmentId{0};
    int studentId{0};
    int grantedBy{0};
    std::string newDeadline;
    std::string reason;
    std::string grantedAt;
};

}
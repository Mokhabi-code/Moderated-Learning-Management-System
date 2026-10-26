
#pragma once

#include <optional>
#include <string>

namespace mlms {

struct MarkRecord {
    int markId{0};
    int submissionId{0};
    int lecturerId{0};
    double rawMark{0.0};
    double finalMark{0.0};
    std::string feedback;
    std::string markedAt;
    std::optional<int> moderatedBy;
    std::optional<std::string> moderatedAt;
    std::optional<std::string> releasedAt;
};

}

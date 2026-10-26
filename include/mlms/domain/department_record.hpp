
#pragma once

#include <optional>
#include <string>

namespace mlms {

struct DepartmentRecord {
    int departmentId{0};
    std::string departmentName;
    std::optional<int> hodId;
};

} // namespace mlms

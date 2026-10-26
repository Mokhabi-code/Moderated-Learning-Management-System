
#pragma once

#include <optional>
#include <string>

namespace mlms {

struct CourseRecord {
    int courseId{0};
    std::string courseCode;
    std::string courseTitle;
    int departmentId{0};
    std::optional<int> lecturerId;
};

} // namespace mlms

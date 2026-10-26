
#pragma once

#include <string>

namespace mlms {

struct EnrollmentRecord {
    int studentId{0};
    int courseId{0};
    std::string enrolledAt;
};

}

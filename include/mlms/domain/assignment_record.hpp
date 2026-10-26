
#pragma once

#include <string>

namespace mlms {

struct AssignmentRecord {
    int assignmentId{0};
    int courseId{0};
    std::string title;
    std::string description;
    std::string deadline;
    double maxMark{0.0};
};

}

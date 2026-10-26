
#pragma once

#include <string>

namespace mlms {

struct SubmissionRecord {
    int submissionId{0};
    int assignmentId{0};
    int studentId{0};
    int version{1};
    std::string submittedAt;
    std::string content;
    std::string state{"Submitted"};
    int daysLate{0};
    double penalty{0.0};
};

}

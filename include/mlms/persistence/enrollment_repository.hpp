
#pragma once

#include "mlms/domain/enrollment_record.hpp"
#include "mlms/persistence/sqlite_connection.hpp"

#include <vector>

namespace mlms {

class EnrollmentRepository {
public:
    explicit EnrollmentRepository(SqliteConnection& connection);

    void enroll(int studentId, int courseId, const std::string& enrolledAt);
    bool unenroll(int studentId, int courseId);

    bool isEnrolled(int studentId, int courseId);

    std::vector<EnrollmentRecord> listByStudent(int studentId);
    std::vector<EnrollmentRecord> listByCourse(int courseId);

private:
    SqliteConnection& connection_;
};

}


#include "mlms/persistence/enrollment_repository.hpp"

#include "mlms/core/errors.hpp"
#include "mlms/persistence/statement.hpp"

#include <sqlite3.h>

namespace mlms {

EnrollmentRepository::EnrollmentRepository(SqliteConnection& connection)
    : connection_(connection) {}

void EnrollmentRepository::enroll(
    int studentId,
    int courseId,
    const std::string& enrolledAt) {

    if (studentId <= 0 || courseId <= 0 || enrolledAt.empty()) {
        throw ValidationError(
            "Valid student ID, course ID and enrollment date are required."
        );
    }

    Statement statement(
        connection_,
        "INSERT INTO enrollment (student_id, course_id, enrolled_at) "
        "VALUES (?, ?, ?);"
    );

    statement.bind(1, studentId);
    statement.bind(2, courseId);
    statement.bind(3, enrolledAt);
    statement.execute();
}

bool EnrollmentRepository::unenroll(int studentId, int courseId) {
    if (studentId <= 0 || courseId <= 0) {
        throw ValidationError("Student ID and course ID must be positive.");
    }

    Statement statement(
        connection_,
        "DELETE FROM enrollment WHERE student_id = ? AND course_id = ?;"
    );

    statement.bind(1, studentId);
    statement.bind(2, courseId);
    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

bool EnrollmentRepository::isEnrolled(int studentId, int courseId) {
    if (studentId <= 0 || courseId <= 0) {
        throw ValidationError("Student ID and course ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT 1 FROM enrollment WHERE student_id = ? AND course_id = ?;"
    );

    statement.bind(1, studentId);
    statement.bind(2, courseId);

    return statement.step();
}

std::vector<EnrollmentRecord> EnrollmentRepository::listByStudent(
    int studentId) {

    if (studentId <= 0) {
        throw ValidationError("Student ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT student_id, course_id, enrolled_at "
        "FROM enrollment WHERE student_id = ? "
        "ORDER BY course_id;"
    );

    statement.bind(1, studentId);

    std::vector<EnrollmentRecord> enrollments;

    while (statement.step()) {
        EnrollmentRecord record;
        record.studentId = statement.columnInt(0);
        record.courseId = statement.columnInt(1);
        record.enrolledAt = statement.columnText(2);

        enrollments.push_back(record);
    }

    return enrollments;
}

std::vector<EnrollmentRecord> EnrollmentRepository::listByCourse(
    int courseId) {

    if (courseId <= 0) {
        throw ValidationError("Course ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT student_id, course_id, enrolled_at "
        "FROM enrollment WHERE course_id = ? "
        "ORDER BY student_id;"
    );

    statement.bind(1, courseId);

    std::vector<EnrollmentRecord> enrollments;

    while (statement.step()) {
        EnrollmentRecord record;
        record.studentId = statement.columnInt(0);
        record.courseId = statement.columnInt(1);
        record.enrolledAt = statement.columnText(2);

        enrollments.push_back(record);
    }

    return enrollments;
}

}


#include "mlms/persistence/course_repository.hpp"

#include "mlms/core/errors.hpp"
#include "mlms/persistence/statement.hpp"

#include <limits>
#include <sqlite3.h>
#include <string>

namespace mlms {

CourseRepository::CourseRepository(
    SqliteConnection& connection
) : connection_(connection) {
}

int CourseRepository::create(
    const std::string& courseCode,
    const std::string& courseTitle,
    int departmentId,
    std::optional<int> lecturerId
) {
    if (courseCode.empty() || courseTitle.empty()) {
        throw ValidationError(
            "Course code and title are required."
        );
    }

    if (departmentId <= 0) {
        throw ValidationError(
            "Department ID must be positive."
        );
    }

    if (lecturerId.has_value() && *lecturerId <= 0) {
        throw ValidationError(
            "Lecturer ID must be positive."
        );
    }

    Statement statement(
        connection_,
        "INSERT INTO course "
        "(course_code, course_title, dept_id, lecturer_id) "
        "VALUES (?, ?, ?, ?);"
    );

    statement.bind(1, courseCode);
    statement.bind(2, courseTitle);
    statement.bind(3, departmentId);

    if (lecturerId.has_value()) {
        statement.bind(4, *lecturerId);
    } else {
        statement.bindNull(4);
    }

    statement.execute();

    const sqlite3_int64 id =
        sqlite3_last_insert_rowid(connection_.handle());

    if (id <= 0 || id > std::numeric_limits<int>::max()) {
        throw PersistenceError(
            "Created course ID is outside the supported range."
        );
    }

    return static_cast<int>(id);
}

std::optional<CourseRecord>
CourseRepository::readCourse(Statement& statement) {
    if (!statement.step()) {
        return std::nullopt;
    }

    CourseRecord course;

    course.courseId = statement.columnInt(0);
    course.courseCode = statement.columnText(1);
    course.courseTitle = statement.columnText(2);
    course.departmentId = statement.columnInt(3);

    if (!statement.columnIsNull(4)) {
        course.lecturerId = statement.columnInt(4);
    }

    return course;
}

std::optional<CourseRecord>
CourseRepository::findById(int courseId) {
    Statement statement(
        connection_,
        "SELECT course_id, course_code, course_title, "
        "dept_id, lecturer_id "
        "FROM course WHERE course_id = ?;"
    );

    statement.bind(1, courseId);

    return readCourse(statement);
}

std::optional<CourseRecord>
CourseRepository::findByCode(const std::string& courseCode) {
    Statement statement(
        connection_,
        "SELECT course_id, course_code, course_title, "
        "dept_id, lecturer_id "
        "FROM course WHERE course_code = ?;"
    );

    statement.bind(1, courseCode);

    return readCourse(statement);
}

std::vector<CourseRecord> CourseRepository::listAll() {
    Statement statement(
        connection_,
        "SELECT course_id, course_code, course_title, "
        "dept_id, lecturer_id "
        "FROM course ORDER BY course_id;"
    );

    std::vector<CourseRecord> courses;

    while (statement.step()) {
        CourseRecord course;

        course.courseId = statement.columnInt(0);
        course.courseCode = statement.columnText(1);
        course.courseTitle = statement.columnText(2);
        course.departmentId = statement.columnInt(3);

        if (!statement.columnIsNull(4)) {
            course.lecturerId = statement.columnInt(4);
        }

        courses.push_back(course);
    }

    return courses;
}

std::vector<CourseRecord>
CourseRepository::listByDepartment(int departmentId) {
    if (departmentId <= 0) {
        throw ValidationError(
            "Department ID must be positive."
        );
    }

    Statement statement(
        connection_,
        "SELECT course_id, course_code, course_title, "
        "dept_id, lecturer_id "
        "FROM course WHERE dept_id = ? ORDER BY course_id;"
    );

    statement.bind(1, departmentId);

    std::vector<CourseRecord> courses;

    while (statement.step()) {
        CourseRecord course;

        course.courseId = statement.columnInt(0);
        course.courseCode = statement.columnText(1);
        course.courseTitle = statement.columnText(2);
        course.departmentId = statement.columnInt(3);

        if (!statement.columnIsNull(4)) {
            course.lecturerId = statement.columnInt(4);
        }

        courses.push_back(course);
    }

    return courses;
}

bool CourseRepository::updateDetails(
    int courseId,
    const std::string& courseCode,
    const std::string& courseTitle
) {
    if (courseId <= 0 ||
        courseCode.empty() ||
        courseTitle.empty()) {
        throw ValidationError(
            "A valid course ID, code and title are required."
        );
    }

    Statement statement(
        connection_,
        "UPDATE course "
        "SET course_code = ?, course_title = ? "
        "WHERE course_id = ?;"
    );

    statement.bind(1, courseCode);
    statement.bind(2, courseTitle);
    statement.bind(3, courseId);
    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

bool CourseRepository::assignLecturer(
    int courseId,
    std::optional<int> lecturerId
) {
    if (courseId <= 0) {
        throw ValidationError(
            "Course ID must be positive."
        );
    }

    if (lecturerId.has_value() && *lecturerId <= 0) {
        throw ValidationError(
            "Lecturer ID must be positive."
        );
    }

    Statement statement(
        connection_,
        "UPDATE course SET lecturer_id = ? "
        "WHERE course_id = ?;"
    );

    if (lecturerId.has_value()) {
        statement.bind(1, *lecturerId);
    } else {
        statement.bindNull(1);
    }

    statement.bind(2, courseId);
    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

} // namespace mlms


#include "mlms/persistence/assignment_repository.hpp"

#include "mlms/core/errors.hpp"
#include "mlms/persistence/statement.hpp"

#include <cmath>
#include <limits>
#include <sqlite3.h>

namespace mlms {

AssignmentRepository::AssignmentRepository(SqliteConnection& connection)
    : connection_(connection) {}

int AssignmentRepository::create(
    int courseId,
    const std::string& title,
    const std::string& description,
    const std::string& deadline,
    double maxMark) {

    if (courseId <= 0) {
        throw ValidationError("Course ID must be positive.");
    }

    if (title.empty() || description.empty() || deadline.empty()) {
        throw ValidationError(
            "Assignment title, description and deadline are required."
        );
    }

    if (!std::isfinite(maxMark) || maxMark <= 0.0) {
        throw ValidationError(
            "Maximum mark must be a finite positive number."
        );
    }

    Statement statement(
        connection_,
        "INSERT INTO assignment "
        "(course_id, title, description, deadline, max_mark) "
        "VALUES (?, ?, ?, ?, ?);"
    );

    statement.bind(1, courseId);
    statement.bind(2, title);
    statement.bind(3, description);
    statement.bind(4, deadline);
    statement.bind(5, maxMark);
    statement.execute();

    const sqlite3_int64 id =
        sqlite3_last_insert_rowid(connection_.handle());

    if (id <= 0 || id > std::numeric_limits<int>::max()) {
        throw PersistenceError(
            "Created assignment ID is outside the supported range."
        );
    }

    return static_cast<int>(id);
}

std::optional<AssignmentRecord> AssignmentRepository::readAssignment(
    Statement& statement) {

    if (!statement.step()) {
        return std::nullopt;
    }

    AssignmentRecord assignment;
    assignment.assignmentId = statement.columnInt(0);
    assignment.courseId = statement.columnInt(1);
    assignment.title = statement.columnText(2);
    assignment.description = statement.columnText(3);
    assignment.deadline = statement.columnText(4);
    assignment.maxMark = statement.columnDouble(5);

    return assignment;
}

std::optional<AssignmentRecord> AssignmentRepository::findById(
    int assignmentId) {

    if (assignmentId <= 0) {
        throw ValidationError("Assignment ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT assignment_id, course_id, title, description, "
        "deadline, max_mark FROM assignment WHERE assignment_id = ?;"
    );

    statement.bind(1, assignmentId);

    return readAssignment(statement);
}

std::vector<AssignmentRecord> AssignmentRepository::listAll() {
    Statement statement(
        connection_,
        "SELECT assignment_id, course_id, title, description, "
        "deadline, max_mark FROM assignment ORDER BY assignment_id;"
    );

    std::vector<AssignmentRecord> assignments;

    while (statement.step()) {
        AssignmentRecord assignment;
        assignment.assignmentId = statement.columnInt(0);
        assignment.courseId = statement.columnInt(1);
        assignment.title = statement.columnText(2);
        assignment.description = statement.columnText(3);
        assignment.deadline = statement.columnText(4);
        assignment.maxMark = statement.columnDouble(5);

        assignments.push_back(assignment);
    }

    return assignments;
}

std::vector<AssignmentRecord> AssignmentRepository::listByCourse(
    int courseId) {

    if (courseId <= 0) {
        throw ValidationError("Course ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT assignment_id, course_id, title, description, "
        "deadline, max_mark FROM assignment "
        "WHERE course_id = ? ORDER BY assignment_id;"
    );

    statement.bind(1, courseId);

    std::vector<AssignmentRecord> assignments;

    while (statement.step()) {
        AssignmentRecord assignment;
        assignment.assignmentId = statement.columnInt(0);
        assignment.courseId = statement.columnInt(1);
        assignment.title = statement.columnText(2);
        assignment.description = statement.columnText(3);
        assignment.deadline = statement.columnText(4);
        assignment.maxMark = statement.columnDouble(5);

        assignments.push_back(assignment);
    }

    return assignments;
}

bool AssignmentRepository::updateDetails(
    int assignmentId,
    const std::string& title,
    const std::string& description,
    const std::string& deadline,
    double maxMark) {

    if (assignmentId <= 0) {
        throw ValidationError("Assignment ID must be positive.");
    }

    if (title.empty() || description.empty() || deadline.empty()) {
        throw ValidationError(
            "Assignment title, description and deadline are required."
        );
    }

    if (!std::isfinite(maxMark) || maxMark <= 0.0) {
        throw ValidationError(
            "Maximum mark must be a finite positive number."
        );
    }

    Statement statement(
        connection_,
        "UPDATE assignment SET title = ?, description = ?, "
        "deadline = ?, max_mark = ? WHERE assignment_id = ?;"
    );

    statement.bind(1, title);
    statement.bind(2, description);
    statement.bind(3, deadline);
    statement.bind(4, maxMark);
    statement.bind(5, assignmentId);
    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

bool AssignmentRepository::deleteAssignment(int assignmentId) {
    if (assignmentId <= 0) {
        throw ValidationError("Assignment ID must be positive.");
    }

    Statement statement(
        connection_,
        "DELETE FROM assignment WHERE assignment_id = ?;"
    );

    statement.bind(1, assignmentId);
    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

}

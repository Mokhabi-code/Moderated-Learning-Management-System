
#include "mlms/persistence/submission_repository.hpp"

#include "mlms/core/errors.hpp"
#include "mlms/persistence/statement.hpp"

#include <cmath>
#include <limits>
#include <sqlite3.h>
#include <string>

namespace mlms {

namespace {

bool isValidState(const std::string& state) {
    return state == "Submitted" ||
           state == "Late" ||
           state == "Marked" ||
           state == "Moderated" ||
           state == "Released";
}

} // namespace

SubmissionRepository::SubmissionRepository(SqliteConnection& connection)
    : connection_(connection) {}

int SubmissionRepository::create(
    int assignmentId,
    int studentId,
    int version,
    const std::string& submittedAt,
    const std::string& content,
    const std::string& state,
    int daysLate,
    double penalty) {

    if (assignmentId <= 0 || studentId <= 0 || version <= 0) {
        throw ValidationError(
            "Assignment ID, student ID and version must be positive."
        );
    }

    if (submittedAt.empty() || content.empty()) {
        throw ValidationError(
            "Submission date and content are required."
        );
    }

    if (!isValidState(state)) {
        throw ValidationError("Invalid submission state.");
    }

    if (daysLate < 0) {
        throw ValidationError("Days late cannot be negative.");
    }

    if (!std::isfinite(penalty) || penalty < 0.0) {
        throw ValidationError(
            "Penalty must be a finite non-negative number."
        );
    }

    if (daysLate > 0 && state != "Late") {
        throw ValidationError(
            "A submission with late days must have the Late state."
        );
    }

    if (daysLate == 0 && state == "Late") {
        throw ValidationError(
            "A submission with zero late days cannot have the Late state."
        );
    }

    Statement statement(
        connection_,
        "INSERT INTO submission "
        "(assignment_id, student_id, version, submitted_at, content, "
        "state, days_late, penalty) VALUES (?, ?, ?, ?, ?, ?, ?, ?);"
    );

    statement.bind(1, assignmentId);
    statement.bind(2, studentId);
    statement.bind(3, version);
    statement.bind(4, submittedAt);
    statement.bind(5, content);
    statement.bind(6, state);
    statement.bind(7, daysLate);
    statement.bind(8, penalty);
    statement.execute();

    const sqlite3_int64 id =
        sqlite3_last_insert_rowid(connection_.handle());

    if (id <= 0 || id > std::numeric_limits<int>::max()) {
        throw PersistenceError(
            "Created submission ID is outside the supported range."
        );
    }

    return static_cast<int>(id);
}

std::optional<SubmissionRecord> SubmissionRepository::readSubmission(
    Statement& statement) {

    if (!statement.step()) {
        return std::nullopt;
    }

    SubmissionRecord record;
    record.submissionId = statement.columnInt(0);
    record.assignmentId = statement.columnInt(1);
    record.studentId = statement.columnInt(2);
    record.version = statement.columnInt(3);
    record.submittedAt = statement.columnText(4);
    record.content = statement.columnText(5);
    record.state = statement.columnText(6);
    record.daysLate = statement.columnInt(7);
    record.penalty = statement.columnDouble(8);

    return record;
}

std::optional<SubmissionRecord> SubmissionRepository::findById(
    int submissionId) {

    if (submissionId <= 0) {
        throw ValidationError("Submission ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT submission_id, assignment_id, student_id, version, "
        "submitted_at, content, state, days_late, penalty "
        "FROM submission WHERE submission_id = ?;"
    );

    statement.bind(1, submissionId);
    return readSubmission(statement);
}

std::vector<SubmissionRecord> SubmissionRepository::listByAssignment(
    int assignmentId) {

    if (assignmentId <= 0) {
        throw ValidationError("Assignment ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT submission_id, assignment_id, student_id, version, "
        "submitted_at, content, state, days_late, penalty "
        "FROM submission WHERE assignment_id = ? "
        "ORDER BY student_id, version;"
    );

    statement.bind(1, assignmentId);

    std::vector<SubmissionRecord> records;

    while (statement.step()) {
        SubmissionRecord record;
        record.submissionId = statement.columnInt(0);
        record.assignmentId = statement.columnInt(1);
        record.studentId = statement.columnInt(2);
        record.version = statement.columnInt(3);
        record.submittedAt = statement.columnText(4);
        record.content = statement.columnText(5);
        record.state = statement.columnText(6);
        record.daysLate = statement.columnInt(7);
        record.penalty = statement.columnDouble(8);

        records.push_back(record);
    }

    return records;
}

std::vector<SubmissionRecord> SubmissionRepository::listByStudent(
    int studentId) {

    if (studentId <= 0) {
        throw ValidationError("Student ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT submission_id, assignment_id, student_id, version, "
        "submitted_at, content, state, days_late, penalty "
        "FROM submission WHERE student_id = ? "
        "ORDER BY assignment_id, version;"
    );

    statement.bind(1, studentId);

    std::vector<SubmissionRecord> records;

    while (statement.step()) {
        SubmissionRecord record;
        record.submissionId = statement.columnInt(0);
        record.assignmentId = statement.columnInt(1);
        record.studentId = statement.columnInt(2);
        record.version = statement.columnInt(3);
        record.submittedAt = statement.columnText(4);
        record.content = statement.columnText(5);
        record.state = statement.columnText(6);
        record.daysLate = statement.columnInt(7);
        record.penalty = statement.columnDouble(8);

        records.push_back(record);
    }

    return records;
}

std::optional<SubmissionRecord> SubmissionRepository::findLatest(
    int assignmentId,
    int studentId) {

    if (assignmentId <= 0 || studentId <= 0) {
        throw ValidationError(
            "Assignment ID and student ID must be positive."
        );
    }

    Statement statement(
        connection_,
        "SELECT submission_id, assignment_id, student_id, version, "
        "submitted_at, content, state, days_late, penalty "
        "FROM submission WHERE assignment_id = ? AND student_id = ? "
        "ORDER BY version DESC LIMIT 1;"
    );

    statement.bind(1, assignmentId);
    statement.bind(2, studentId);

    return readSubmission(statement);
}

bool SubmissionRepository::updateState(
    int submissionId,
    const std::string& state) {

    if (submissionId <= 0) {
        throw ValidationError("Submission ID must be positive.");
    }

    if (!isValidState(state)) {
        throw ValidationError("Invalid submission state.");
    }

    Statement statement(
        connection_,
        "UPDATE submission SET state = ? WHERE submission_id = ?;"
    );

    statement.bind(1, state);
    statement.bind(2, submissionId);
    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

}

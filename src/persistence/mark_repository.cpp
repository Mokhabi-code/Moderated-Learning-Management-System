
#include "mlms/persistence/mark_repository.hpp"

#include "mlms/core/errors.hpp"
#include "mlms/persistence/statement.hpp"

#include <cmath>
#include <limits>
#include <sqlite3.h>

namespace mlms {

MarkRepository::MarkRepository(SqliteConnection& connection)
    : connection_(connection) {}

int MarkRepository::create(
    int submissionId,
    int lecturerId,
    double rawMark,
    double finalMark,
    const std::string& feedback,
    const std::string& markedAt) {

    if (submissionId <= 0 || lecturerId <= 0) {
        throw ValidationError(
            "Submission ID and lecturer ID must be positive."
        );
    }

    if (!std::isfinite(rawMark) || rawMark < 0.0 ||
        !std::isfinite(finalMark) || finalMark < 0.0) {
        throw ValidationError(
            "Raw and final marks must be finite and non-negative."
        );
    }

    if (feedback.empty() || markedAt.empty()) {
        throw ValidationError("Feedback and marked date are required.");
    }

    Statement statement(
        connection_,
        "INSERT INTO mark "
        "(submission_id, lecturer_id, raw_mark, final_mark, feedback, "
        "marked_at) VALUES (?, ?, ?, ?, ?, ?);"
    );

    statement.bind(1, submissionId);
    statement.bind(2, lecturerId);
    statement.bind(3, rawMark);
    statement.bind(4, finalMark);
    statement.bind(5, feedback);
    statement.bind(6, markedAt);
    statement.execute();

    const sqlite3_int64 id =
        sqlite3_last_insert_rowid(connection_.handle());

    if (id <= 0 || id > std::numeric_limits<int>::max()) {
        throw PersistenceError(
            "Created mark ID is outside the supported range."
        );
    }

    return static_cast<int>(id);
}

std::optional<MarkRecord> MarkRepository::readMark(
    Statement& statement) {

    if (!statement.step()) {
        return std::nullopt;
    }

    MarkRecord mark;
    mark.markId = statement.columnInt(0);
    mark.submissionId = statement.columnInt(1);
    mark.lecturerId = statement.columnInt(2);
    mark.rawMark = statement.columnDouble(3);
    mark.finalMark = statement.columnDouble(4);
    mark.feedback = statement.columnText(5);
    mark.markedAt = statement.columnText(6);

    if (!statement.columnIsNull(7)) {
        mark.moderatedBy = statement.columnInt(7);
    }

    if (!statement.columnIsNull(8)) {
        mark.moderatedAt = statement.columnText(8);
    }

    if (!statement.columnIsNull(9)) {
        mark.releasedAt = statement.columnText(9);
    }

    return mark;
}

std::optional<MarkRecord> MarkRepository::findById(int markId) {
    if (markId <= 0) {
        throw ValidationError("Mark ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT mark_id, submission_id, lecturer_id, raw_mark, "
        "final_mark, feedback, marked_at, moderated_by, moderated_at, "
        "released_at FROM mark WHERE mark_id = ?;"
    );

    statement.bind(1, markId);
    return readMark(statement);
}

std::optional<MarkRecord> MarkRepository::findBySubmission(
    int submissionId) {

    if (submissionId <= 0) {
        throw ValidationError("Submission ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT mark_id, submission_id, lecturer_id, raw_mark, "
        "final_mark, feedback, marked_at, moderated_by, moderated_at, "
        "released_at FROM mark WHERE submission_id = ?;"
    );

    statement.bind(1, submissionId);
    return readMark(statement);
}

bool MarkRepository::updateMark(
    int markId,
    double rawMark,
    double finalMark,
    const std::string& feedback) {

    if (markId <= 0) {
        throw ValidationError("Mark ID must be positive.");
    }

    if (!std::isfinite(rawMark) || rawMark < 0.0 ||
        !std::isfinite(finalMark) || finalMark < 0.0) {
        throw ValidationError(
            "Raw and final marks must be finite and non-negative."
        );
    }

    if (feedback.empty()) {
        throw ValidationError("Feedback is required.");
    }

    Statement statement(
        connection_,
        "UPDATE mark SET raw_mark = ?, final_mark = ?, feedback = ? "
        "WHERE mark_id = ? AND released_at IS NULL;"
    );

    statement.bind(1, rawMark);
    statement.bind(2, finalMark);
    statement.bind(3, feedback);
    statement.bind(4, markId);
    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

bool MarkRepository::recordModeration(
    int markId,
    int moderatorId,
    const std::string& moderatedAt) {

    if (markId <= 0 || moderatorId <= 0) {
        throw ValidationError(
            "Mark ID and moderator ID must be positive."
        );
    }

    if (moderatedAt.empty()) {
        throw ValidationError("Moderation date is required.");
    }

    Statement statement(
        connection_,
        "UPDATE mark SET moderated_by = ?, moderated_at = ? "
        "WHERE mark_id = ? AND released_at IS NULL;"
    );

    statement.bind(1, moderatorId);
    statement.bind(2, moderatedAt);
    statement.bind(3, markId);
    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

bool MarkRepository::releaseMark(
    int markId,
    const std::string& releasedAt) {

    if (markId <= 0) {
        throw ValidationError("Mark ID must be positive.");
    }

    if (releasedAt.empty()) {
        throw ValidationError("Release date is required.");
    }

    Statement statement(
        connection_,
        "UPDATE mark SET released_at = ? "
        "WHERE mark_id = ? AND released_at IS NULL "
        "AND moderated_by IS NOT NULL AND moderated_at IS NOT NULL;"
    );

    statement.bind(1, releasedAt);
    statement.bind(2, markId);
    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

}

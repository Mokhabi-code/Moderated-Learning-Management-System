
#pragma once

#include "mlms/domain/submission_record.hpp"
#include "mlms/persistence/sqlite_connection.hpp"

#include <optional>
#include <vector>

namespace mlms {

class SubmissionRepository {
public:
    explicit SubmissionRepository(SqliteConnection& connection);

    int create(
        int assignmentId,
        int studentId,
        int version,
        const std::string& submittedAt,
        const std::string& content,
        const std::string& state,
        int daysLate,
        double penalty
    );

    std::optional<SubmissionRecord> findById(int submissionId);

    std::vector<SubmissionRecord> listByAssignment(int assignmentId);

    std::vector<SubmissionRecord> listByStudent(int studentId);

    std::optional<SubmissionRecord> findLatest(
        int assignmentId,
        int studentId
    );

    bool updateState(int submissionId, const std::string& state);

private:
    SqliteConnection& connection_;

    std::optional<SubmissionRecord> readSubmission(
        class Statement& statement
    );
};

}

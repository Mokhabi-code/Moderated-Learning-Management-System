
#pragma once

#include "mlms/domain/mark_record.hpp"
#include "mlms/persistence/sqlite_connection.hpp"

#include <optional>

namespace mlms {

class MarkRepository {
public:
    explicit MarkRepository(SqliteConnection& connection);

    int create(
        int submissionId,
        int lecturerId,
        double rawMark,
        double finalMark,
        const std::string& feedback,
        const std::string& markedAt
    );

    std::optional<MarkRecord> findById(int markId);

    std::optional<MarkRecord> findBySubmission(int submissionId);

    bool updateMark(
        int markId,
        double rawMark,
        double finalMark,
        const std::string& feedback
    );

    bool recordModeration(
        int markId,
        int moderatorId,
        const std::string& moderatedAt
    );

    bool releaseMark(int markId, const std::string& releasedAt);

private:
    SqliteConnection& connection_;

    std::optional<MarkRecord> readMark(class Statement& statement);
};

}

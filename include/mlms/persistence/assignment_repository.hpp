
#pragma once

#include "mlms/domain/assignment_record.hpp"
#include "mlms/persistence/sqlite_connection.hpp"

#include <optional>
#include <string>
#include <vector>

namespace mlms {

class AssignmentRepository {
public:
    explicit AssignmentRepository(SqliteConnection& connection);

    int create(
        int courseId,
        const std::string& title,
        const std::string& description,
        const std::string& deadline,
        double maxMark
    );

    std::optional<AssignmentRecord> findById(int assignmentId);

    std::vector<AssignmentRecord> listAll();

    std::vector<AssignmentRecord> listByCourse(int courseId);

    bool updateDetails(
        int assignmentId,
        const std::string& title,
        const std::string& description,
        const std::string& deadline,
        double maxMark
    );

    bool deleteAssignment(int assignmentId);

private:
    SqliteConnection& connection_;

    std::optional<AssignmentRecord> readAssignment(
        class Statement& statement
    );
};

}

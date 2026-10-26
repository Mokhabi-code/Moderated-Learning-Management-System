
#pragma once

#include "mlms/domain/course_record.hpp"
#include "mlms/persistence/sqlite_connection.hpp"

#include <optional>
#include <string>
#include <vector>

namespace mlms {

class CourseRepository {
public:
    explicit CourseRepository(SqliteConnection& connection);

    int create(
        const std::string& courseCode,
        const std::string& courseTitle,
        int departmentId,
        std::optional<int> lecturerId = std::nullopt
    );

    std::optional<CourseRecord> findById(int courseId);
    std::optional<CourseRecord> findByCode(
        const std::string& courseCode
    );

    std::vector<CourseRecord> listAll();
    std::vector<CourseRecord> listByDepartment(int departmentId);

    bool updateDetails(
        int courseId,
        const std::string& courseCode,
        const std::string& courseTitle
    );

    bool assignLecturer(
        int courseId,
        std::optional<int> lecturerId
    );

private:
    SqliteConnection& connection_;

    std::optional<CourseRecord> readCourse(
        class Statement& statement
    );
};

} // namespace mlms


#pragma once

#include "mlms/domain/department_record.hpp"
#include "mlms/persistence/sqlite_connection.hpp"

#include <optional>
#include <string>
#include <vector>

namespace mlms {

class DepartmentRepository {
public:
    explicit DepartmentRepository(SqliteConnection& connection);

    int create(const std::string& departmentName);

    std::optional<DepartmentRecord> findById(int departmentId);
    std::optional<DepartmentRecord> findByName(
        const std::string& departmentName
    );

    std::vector<DepartmentRecord> listAll();

    bool updateName(
        int departmentId,
        const std::string& departmentName
    );

    bool assignHead(int departmentId, int hodId);

private:
    SqliteConnection& connection_;

    std::optional<DepartmentRecord> readDepartment(
        class Statement& statement
    );
};

} // namespace mlms

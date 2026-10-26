
#include "mlms/persistence/department_repository.hpp"

#include "mlms/core/errors.hpp"
#include "mlms/persistence/statement.hpp"

#include <limits>
#include <sqlite3.h>
#include <string>

namespace mlms {

DepartmentRepository::DepartmentRepository(
    SqliteConnection& connection
) : connection_(connection) {
}

int DepartmentRepository::create(
    const std::string& departmentName
) {
    if (departmentName.empty()) {
        throw ValidationError("Department name is required.");
    }

    Statement statement(
        connection_,
        "INSERT INTO department (dept_name) VALUES (?);"
    );

    statement.bind(1, departmentName);
    statement.execute();

    const sqlite3_int64 id =
        sqlite3_last_insert_rowid(connection_.handle());

    if (id <= 0 || id > std::numeric_limits<int>::max()) {
        throw PersistenceError(
            "Created department ID is outside the supported range."
        );
    }

    return static_cast<int>(id);
}

std::optional<DepartmentRecord>
DepartmentRepository::readDepartment(Statement& statement) {
    if (!statement.step()) {
        return std::nullopt;
    }

    DepartmentRecord department;

    department.departmentId = statement.columnInt(0);
    department.departmentName = statement.columnText(1);

    if (!statement.columnIsNull(2)) {
        department.hodId = statement.columnInt(2);
    }

    return department;
}

std::optional<DepartmentRecord>
DepartmentRepository::findById(int departmentId) {
    Statement statement(
        connection_,
        "SELECT dept_id, dept_name, hod_id "
        "FROM department WHERE dept_id = ?;"
    );

    statement.bind(1, departmentId);

    return readDepartment(statement);
}

std::optional<DepartmentRecord>
DepartmentRepository::findByName(
    const std::string& departmentName
) {
    Statement statement(
        connection_,
        "SELECT dept_id, dept_name, hod_id "
        "FROM department WHERE dept_name = ?;"
    );

    statement.bind(1, departmentName);

    return readDepartment(statement);
}

std::vector<DepartmentRecord>
DepartmentRepository::listAll() {
    Statement statement(
        connection_,
        "SELECT dept_id, dept_name, hod_id "
        "FROM department ORDER BY dept_id;"
    );

    std::vector<DepartmentRecord> departments;

    while (statement.step()) {
        DepartmentRecord department;

        department.departmentId = statement.columnInt(0);
        department.departmentName = statement.columnText(1);

        if (!statement.columnIsNull(2)) {
            department.hodId = statement.columnInt(2);
        }

        departments.push_back(department);
    }

    return departments;
}

bool DepartmentRepository::updateName(
    int departmentId,
    const std::string& departmentName
) {
    if (departmentId <= 0 || departmentName.empty()) {
        throw ValidationError(
            "A valid department ID and name are required."
        );
    }

    Statement statement(
        connection_,
        "UPDATE department SET dept_name = ? WHERE dept_id = ?;"
    );

    statement.bind(1, departmentName);
    statement.bind(2, departmentId);
    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

bool DepartmentRepository::assignHead(
    int departmentId,
    int hodId
) {
    if (departmentId <= 0 || hodId <= 0) {
        throw ValidationError(
            "Department ID and HoD user ID must be positive."
        );
    }

    Statement statement(
        connection_,
        "UPDATE department SET hod_id = ? WHERE dept_id = ?;"
    );

    statement.bind(1, hodId);
    statement.bind(2, departmentId);
    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

} // namespace mlms

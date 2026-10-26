#include "mlms/persistence/schema_initializer.hpp"
#include "mlms/core/errors.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace mlms {

void SchemaInitializer::initialize(
    SqliteConnection& connection,
    const std::string& schemaPath
) {
    std::ifstream file(schemaPath);

    if (!file.is_open()) {
        throw PersistenceError(
            "Unable to open database schema: " + schemaPath
        );
    }

    std::ostringstream contents;
    contents << file.rdbuf();

    if (file.bad()) {
        throw PersistenceError(
            "Failed to read database schema: " + schemaPath
        );
    }

    const std::string sql = contents.str();

    if (sql.empty()) {
        throw PersistenceError(
            "Database schema file is empty: " + schemaPath
        );
    }

    connection.execute(sql);
}

} // namespace mlms

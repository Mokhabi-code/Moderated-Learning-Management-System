#pragma once

#include "mlms/persistence/sqlite_connection.hpp"

#include <string>

namespace mlms {

class SchemaInitializer {
public:
    static void initialize(
        SqliteConnection& connection,
        const std::string& schemaPath
    );
};

}
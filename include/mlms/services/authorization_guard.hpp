
#pragma once

#include "mlms/domain/role.hpp"

namespace mlms {

class AuthorizationGuard {
public:
    static void requireStudent(Role role);
    static void requireLecturer(Role role);
    static void requireHoD(Role role);
    static void requireAdministrator(Role role);
};

}

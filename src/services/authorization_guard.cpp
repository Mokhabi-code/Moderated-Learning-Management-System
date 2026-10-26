
#include "mlms/services/authorization_guard.hpp"
#include "mlms/core/errors.hpp"

namespace mlms {

void AuthorizationGuard::requireStudent(Role role) {
    if (role != Role::Student) {
        throw AuthorizationError(
            "Access denied: Student role required."
        );
    }
}

void AuthorizationGuard::requireLecturer(Role role) {
    if (role != Role::Lecturer) {
        throw AuthorizationError(
            "Access denied: Lecturer role required."
        );
    }
}

void AuthorizationGuard::requireHoD(Role role) {
    if (role != Role::HoD) {
        throw AuthorizationError(
            "Access denied: HoD role required."
        );
    }
}

void AuthorizationGuard::requireAdministrator(Role role) {
    if (role != Role::Administrator) {
        throw AuthorizationError(
            "Access denied: Administrator role required."
        );
    }
}

}

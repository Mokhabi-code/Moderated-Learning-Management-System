#pragma once

#include <string_view>

namespace mlms {

enum class Role {
    Student,
    Lecturer,
    HoD,
    Administrator
};

inline std::string_view toString(Role role) {
    switch (role) {
        case Role::Student:
            return "Student";
        case Role::Lecturer:
            return "Lecturer";
        case Role::HoD:
            return "HoD";
        case Role::Administrator:
            return "Administrator";
    }

    return "Student";
}

}
#pragma once
#include <string>

namespace mlms::domain {

enum class Role { Student, Lecturer, HoD, Administrator };

class User {
protected:
    int id;
    std::string username;
    std::string passwordHash;
    std::string fullName;
    std::string email;
    
public:
    virtual ~User() = default;
    virtual Role getRole() const = 0;
};

}
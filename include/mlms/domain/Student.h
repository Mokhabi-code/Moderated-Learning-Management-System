#pragma once
#include "User.h"

namespace mlms::domain {

class Student : public User {
private:
    std::string studentNo;
    int deptId;
    
public:
    Role getRole() const override { return Role::Student; }
};

}
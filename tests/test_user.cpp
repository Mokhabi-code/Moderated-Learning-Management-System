#include <gtest/gtest.h>
#include "mlms/domain/Student.h"

using namespace mlms::domain;

TEST(UserTest, StudentHasCorrectRole) {
    Student s;
    EXPECT_EQ(s.getRole(), Role::Student);
}
#include <gtest/gtest.h>
#include "mlms/domain/StandardLatePenalty.h"

using namespace mlms::domain;

TEST(LatePenaltyTest, ComputesCorrectly) {
    StandardLatePenalty penalty;
    
    // On time
    EXPECT_DOUBLE_EQ(penalty.computePenalty(0, 100.0), 0.0);
    // 1 day late (10%)
    EXPECT_DOUBLE_EQ(penalty.computePenalty(1, 100.0), 10.0);
    // 3 days late (30%)
    EXPECT_DOUBLE_EQ(penalty.computePenalty(3, 100.0), 30.0);
    // 4 days late (Zero mark)
    EXPECT_DOUBLE_EQ(penalty.computePenalty(4, 100.0), 100.0); 
}
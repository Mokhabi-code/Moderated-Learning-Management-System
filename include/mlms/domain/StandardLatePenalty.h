#pragma once
#include "ILatePenalty.h"

namespace mlms::domain {

class StandardLatePenalty : public ILatePenalty {
public:
    double computePenalty(int daysLate, double maxMark) const override {
        if (daysLate <= 0) return 0.0;
        if (daysLate > 3) return maxMark; // Zero mark after 3 days
        return maxMark * 0.10 * daysLate;
    }
};

}
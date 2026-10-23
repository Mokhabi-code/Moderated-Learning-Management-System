#pragma once

namespace mlms::domain {

class ILatePenalty {
public:
    virtual ~ILatePenalty() = default;
    virtual double computePenalty(int daysLate, double maxMark) const = 0;
};

}
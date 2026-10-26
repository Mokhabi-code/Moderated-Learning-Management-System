#pragma once

namespace mlms {

class ILatePenalty {
public:
    virtual ~ILatePenalty() = default;

    // Returns the penalty amount in marks, not a percentage.
    virtual double calculate(
        double rawMark,
        double maxMark,
        int daysLate
    ) const = 0;
};

class StandardLatePenalty final : public ILatePenalty {
public:
    explicit StandardLatePenalty(
        double percentPerDay,
        double maximumPercent
    );

    double calculate(
        double rawMark,
        double maxMark,
        int daysLate
    ) const override;

private:
    double percentPerDay_;
    double maximumPercent_;
};

class NoLatePenalty final : public ILatePenalty {
public:
    double calculate(
        double rawMark,
        double maxMark,
        int daysLate
    ) const override;
};

}
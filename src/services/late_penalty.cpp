#include "mlms/services/late_penalty.hpp"
#include "mlms/core/errors.hpp"

#include <algorithm>
#include <cmath>

namespace mlms {

StandardLatePenalty::StandardLatePenalty(
    double percentPerDay,
    double maximumPercent
)
    : percentPerDay_(percentPerDay),
      maximumPercent_(maximumPercent) {

    if (!std::isfinite(percentPerDay_)
        || !std::isfinite(maximumPercent_)
        || percentPerDay_ < 0.0
        || maximumPercent_ < 0.0
        || maximumPercent_ > 100.0) {

        throw ValidationError(
            "Late-penalty percentages must be finite and within valid bounds."
        );
    }
}

double StandardLatePenalty::calculate(
    double rawMark,
    double maxMark,
    int daysLate
) const {

    if (!std::isfinite(rawMark)
        || !std::isfinite(maxMark)
        || maxMark <= 0.0
        || rawMark < 0.0
        || rawMark > maxMark
        || daysLate < 0) {

        throw ValidationError(
            "Invalid mark, maximum mark, or days-late value."
        );
    }

    if (daysLate == 0 || daysLate >= 4) {
        return 0.0;
    }

    const double rate = std::min(
        percentPerDay_ * static_cast<double>(daysLate),
        maximumPercent_
    );

    return maxMark * rate / 100.0;
}

double NoLatePenalty::calculate(
    double rawMark,
    double maxMark,
    int daysLate
) const {

    if (!std::isfinite(rawMark)
        || !std::isfinite(maxMark)
        || maxMark <= 0.0
        || rawMark < 0.0
        || rawMark > maxMark
        || daysLate < 0) {

        throw ValidationError(
            "Invalid mark, maximum mark, or days-late value."
        );
    }

    return 0.0;
}

}
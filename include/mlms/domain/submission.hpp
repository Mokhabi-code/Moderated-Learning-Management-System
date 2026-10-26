#pragma once

#include <string_view>

namespace mlms {

enum class SubmissionState {
    Submitted,
    Late,
    Marked,
    Moderated,
    Released
};

std::string_view toString(SubmissionState state);

bool canTransition(
    SubmissionState from,
    SubmissionState to
);

}
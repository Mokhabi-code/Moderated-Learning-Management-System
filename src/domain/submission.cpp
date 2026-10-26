#include "mlms/domain/submission.hpp"

namespace mlms {

std::string_view toString(SubmissionState state) {
    switch (state) {
        case SubmissionState::Submitted:
            return "Submitted";
        case SubmissionState::Late:
            return "Late";
        case SubmissionState::Marked:
            return "Marked";
        case SubmissionState::Moderated:
            return "Moderated";
        case SubmissionState::Released:
            return "Released";
    }

    return "Submitted";
}

bool canTransition(
    SubmissionState from,
    SubmissionState to
) {
    switch (from) {
        case SubmissionState::Submitted:
        case SubmissionState::Late:
            return to == SubmissionState::Marked;

        case SubmissionState::Marked:
            return to == SubmissionState::Moderated;

        case SubmissionState::Moderated:
            return to == SubmissionState::Marked
                || to == SubmissionState::Released;

        case SubmissionState::Released:
            return false;
    }

    return false;
}

}
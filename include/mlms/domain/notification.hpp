
#pragma once

#include <optional>
#include <string>

namespace mlms {

struct Notification {
    int notificationId{0};
    int recipientId{0};
    std::string type;
    std::string message;
    std::optional<int> relatedEntityId;
    std::string createdAt;
    std::optional<std::string> readAt;
};

}

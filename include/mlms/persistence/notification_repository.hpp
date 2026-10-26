
#pragma once

#include "mlms/domain/notification.hpp"
#include "mlms/persistence/sqlite_connection.hpp"

#include <optional>
#include <string>
#include <vector>

namespace mlms {

class NotificationRepository {
public:
    explicit NotificationRepository(SqliteConnection& connection);

    int create(
        int recipientId,
        const std::string& type,
        const std::string& message,
        const std::optional<int>& relatedEntityId,
        const std::string& createdAt
    );

    std::optional<Notification> findById(int notificationId);

    std::vector<Notification> listByRecipient(int recipientId);

    std::vector<Notification> listUnreadByRecipient(int recipientId);

    bool markAsRead(
        int notificationId,
        const std::string& readAt
    );

private:
    SqliteConnection& connection_;

    std::optional<Notification> readNotification(
        class Statement& statement
    );
};

}

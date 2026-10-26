
#include "mlms/persistence/notification_repository.hpp"

#include "mlms/core/errors.hpp"
#include "mlms/persistence/statement.hpp"

#include <cctype>
#include <limits>
#include <sqlite3.h>

namespace mlms {

namespace {

bool isBlank(const std::string& value) {
    for (unsigned char character : value) {
        if (!std::isspace(character)) {
            return false;
        }
    }

    return true;
}

bool isValidType(const std::string& type) {
    return type == "MarkReleased" ||
           type == "ModerationReturned" ||
           type == "ExtensionGranted";
}

} // namespace

NotificationRepository::NotificationRepository(
    SqliteConnection& connection
)
    : connection_(connection) {}

int NotificationRepository::create(
    int recipientId,
    const std::string& type,
    const std::string& message,
    const std::optional<int>& relatedEntityId,
    const std::string& createdAt
) {
    if (recipientId <= 0) {
        throw ValidationError("Recipient ID must be positive.");
    }

    if (!isValidType(type)) {
        throw ValidationError("Invalid notification type.");
    }

    if (isBlank(message) || isBlank(createdAt)) {
        throw ValidationError(
            "Notification message and creation timestamp are required."
        );
    }

    if (relatedEntityId.has_value() && *relatedEntityId <= 0) {
        throw ValidationError(
            "Related entity ID must be positive when provided."
        );
    }

    Statement statement(
        connection_,
        "INSERT INTO notification "
        "(recipient_id, type, message, related_entity_id, created_at) "
        "VALUES (?, ?, ?, ?, ?);"
    );

    statement.bind(1, recipientId);
    statement.bind(2, type);
    statement.bind(3, message);

    if (relatedEntityId.has_value()) {
        statement.bind(4, *relatedEntityId);
    } else {
        statement.bindNull(4);
    }

    statement.bind(5, createdAt);
    statement.execute();

    const sqlite3_int64 id =
        sqlite3_last_insert_rowid(connection_.handle());

    if (id <= 0 || id > std::numeric_limits<int>::max()) {
        throw PersistenceError(
            "Created notification ID is outside the supported range."
        );
    }

    return static_cast<int>(id);
}

std::optional<Notification>
NotificationRepository::readNotification(Statement& statement) {
    if (!statement.step()) {
        return std::nullopt;
    }

    Notification notification;

    notification.notificationId = statement.columnInt(0);
    notification.recipientId = statement.columnInt(1);
    notification.type = statement.columnText(2);
    notification.message = statement.columnText(3);

    if (!statement.columnIsNull(4)) {
        notification.relatedEntityId = statement.columnInt(4);
    }

    notification.createdAt = statement.columnText(5);

    if (!statement.columnIsNull(6)) {
        notification.readAt = statement.columnText(6);
    }

    return notification;
}

std::optional<Notification>
NotificationRepository::findById(int notificationId) {
    if (notificationId <= 0) {
        throw ValidationError(
            "Notification ID must be positive."
        );
    }

    Statement statement(
        connection_,
        "SELECT notification_id, recipient_id, type, message, "
        "related_entity_id, created_at, read_at "
        "FROM notification WHERE notification_id = ?;"
    );

    statement.bind(1, notificationId);

    return readNotification(statement);
}

std::vector<Notification>
NotificationRepository::listByRecipient(int recipientId) {
    if (recipientId <= 0) {
        throw ValidationError("Recipient ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT notification_id, recipient_id, type, message, "
        "related_entity_id, created_at, read_at "
        "FROM notification WHERE recipient_id = ? "
        "ORDER BY notification_id;"
    );

    statement.bind(1, recipientId);

    std::vector<Notification> notifications;

    while (auto notification = readNotification(statement)) {
        notifications.push_back(*notification);
    }

    return notifications;
}

std::vector<Notification>
NotificationRepository::listUnreadByRecipient(int recipientId) {
    if (recipientId <= 0) {
        throw ValidationError("Recipient ID must be positive.");
    }

    Statement statement(
        connection_,
        "SELECT notification_id, recipient_id, type, message, "
        "related_entity_id, created_at, read_at "
        "FROM notification "
        "WHERE recipient_id = ? AND read_at IS NULL "
        "ORDER BY notification_id;"
    );

    statement.bind(1, recipientId);

    std::vector<Notification> notifications;

    while (auto notification = readNotification(statement)) {
        notifications.push_back(*notification);
    }

    return notifications;
}

bool NotificationRepository::markAsRead(
    int notificationId,
    const std::string& readAt
) {
    if (notificationId <= 0) {
        throw ValidationError(
            "Notification ID must be positive."
        );
    }

    if (isBlank(readAt)) {
        throw ValidationError("Read timestamp is required.");
    }

    Statement statement(
        connection_,
        "UPDATE notification SET read_at = ? "
        "WHERE notification_id = ? AND read_at IS NULL;"
    );

    statement.bind(1, readAt);
    statement.bind(2, notificationId);
    statement.execute();

    return sqlite3_changes(connection_.handle()) > 0;
}

} // namespace mlms

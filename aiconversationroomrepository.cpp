#include "aiconversationroomrepository.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#include <utility>

namespace {
QJsonObject participantToJson(const AIConversationParticipant &participant)
{
    QJsonObject value;
    value.insert(QStringLiteral("participantId"), participant.participantId);
    value.insert(QStringLiteral("petProjectPath"), participant.petProjectPath);
    value.insert(QStringLiteral("projectId"), participant.projectId);
    value.insert(QStringLiteral("enabled"), participant.enabled);
    value.insert(QStringLiteral("validationError"), participant.validationError);
    value.insert(QStringLiteral("speakingWeight"), participant.speakingWeight);
    value.insert(QStringLiteral("lastSpokeTurn"), static_cast<qint64>(participant.lastSpokeTurn));
    return value;
}

QJsonObject relationshipToJson(const ParticipantRelationship &relationship)
{
    QJsonObject value;
    value.insert(QStringLiteral("fromParticipantId"), relationship.fromParticipantId);
    value.insert(QStringLiteral("toParticipantId"), relationship.toParticipantId);
    value.insert(QStringLiteral("preferredAddress"), relationship.preferredAddress);
    value.insert(QStringLiteral("description"), relationship.description);
    return value;
}

ParticipantRelationship relationshipFromJson(const QJsonObject &value)
{
    ParticipantRelationship relationship;
    relationship.fromParticipantId = value.value(QStringLiteral("fromParticipantId")).toString();
    relationship.toParticipantId = value.value(QStringLiteral("toParticipantId")).toString();
    relationship.preferredAddress = value.value(QStringLiteral("preferredAddress")).toString();
    relationship.description = value.value(QStringLiteral("description")).toString();
    return relationship;
}

AIConversationParticipant participantFromJson(const QJsonObject &value)
{
    AIConversationParticipant participant;
    participant.participantId = value.value(QStringLiteral("participantId")).toString();
    participant.petProjectPath = value.value(QStringLiteral("petProjectPath")).toString();
    participant.projectId = value.value(QStringLiteral("projectId")).toString();
    participant.characterName = value.value(QStringLiteral("characterName")).toString();
    participant.systemPrompt = value.value(QStringLiteral("systemPrompt")).toString();
    participant.providerProfileId = value.value(QStringLiteral("providerProfileId")).toString();
    participant.enabled = value.value(QStringLiteral("enabled")).toBool(true);
    participant.validationError = value.value(QStringLiteral("validationError")).toString();
    participant.speakingWeight = value.value(QStringLiteral("speakingWeight")).toDouble(1.0);
    participant.lastSpokeTurn = value.value(QStringLiteral("lastSpokeTurn")).toVariant().toULongLong();
    return participant;
}

QJsonObject messageToJson(const ConversationMessage &message)
{
    QJsonObject value;
    value.insert(QStringLiteral("messageId"), message.messageId);
    value.insert(QStringLiteral("sequence"), message.sequence);
    value.insert(QStringLiteral("senderId"), message.senderId);
    value.insert(QStringLiteral("senderName"), message.senderName);
    value.insert(QStringLiteral("roomId"), message.roomId);
    value.insert(QStringLiteral("turnId"), message.turnId);
    value.insert(QStringLiteral("responderIndex"), message.responderIndex);
    value.insert(QStringLiteral("senderType"), static_cast<int>(message.senderType));
    value.insert(QStringLiteral("status"), static_cast<int>(message.status));
    value.insert(QStringLiteral("content"), message.content);
    value.insert(QStringLiteral("errorCode"), message.errorCode);
    value.insert(QStringLiteral("actionId"), message.actionId);
    value.insert(QStringLiteral("actionParameters"), message.actionParameters);
    value.insert(QStringLiteral("desktopDeliveryStatus"), static_cast<int>(message.desktopDeliveryStatus));
    value.insert(QStringLiteral("actionExecutionStatus"), static_cast<int>(message.actionExecutionStatus));
    value.insert(QStringLiteral("actionStatus"), message.actionStatus);
    value.insert(QStringLiteral("timestamp"), message.timestamp.toUTC().toString(Qt::ISODateWithMs));
    return value;
}

ConversationMessage messageFromJson(const QJsonObject &value)
{
    ConversationMessage message;
    message.messageId = value.value(QStringLiteral("messageId")).toString();
    message.sequence = value.value(QStringLiteral("sequence")).toInt();
    message.senderId = value.value(QStringLiteral("senderId")).toString();
    message.senderName = value.value(QStringLiteral("senderName")).toString();
    message.roomId = value.value(QStringLiteral("roomId")).toString();
    message.turnId = value.value(QStringLiteral("turnId")).toString();
    message.responderIndex = value.value(QStringLiteral("responderIndex")).toInt(-1);
    message.senderType = static_cast<ConversationSenderType>(value.value(QStringLiteral("senderType")).toInt());
    message.status = static_cast<ConversationMessageStatus>(value.value(QStringLiteral("status")).toInt());
    message.content = value.value(QStringLiteral("content")).toString();
    message.errorCode = value.value(QStringLiteral("errorCode")).toString();
    message.actionId = value.value(QStringLiteral("actionId")).toString();
    message.actionParameters = value.value(QStringLiteral("actionParameters")).toObject();
    message.desktopDeliveryStatus = static_cast<DesktopDeliveryStatus>(
        value.value(QStringLiteral("desktopDeliveryStatus")).toInt(static_cast<int>(DesktopDeliveryStatus::NotRequested)));
    message.actionExecutionStatus = static_cast<ActionExecutionStatus>(
        value.value(QStringLiteral("actionExecutionStatus")).toInt(static_cast<int>(ActionExecutionStatus::NotRequested)));
    message.actionStatus = value.value(QStringLiteral("actionStatus")).toString();
    if (!value.contains(QStringLiteral("desktopDeliveryStatus"))) {
        if (message.actionStatus == QStringLiteral("DeliveredToRuntime")) {
            message.desktopDeliveryStatus = DesktopDeliveryStatus::Delivered;
        } else if (message.actionStatus == QStringLiteral("RuntimeDispatchFailed")) {
            message.desktopDeliveryStatus = DesktopDeliveryStatus::PetNotRunning;
        } else if (message.status == ConversationMessageStatus::Completed) {
            message.desktopDeliveryStatus = DesktopDeliveryStatus::Pending;
        }
    }
    if (!value.contains(QStringLiteral("actionExecutionStatus"))) {
        message.actionExecutionStatus = message.actionId.isEmpty()
            ? ActionExecutionStatus::NotRequested
            : (message.actionStatus == QStringLiteral("DeliveredToRuntime")
                   ? ActionExecutionStatus::Executed
                   : ActionExecutionStatus::Pending);
    }
    message.timestamp = QDateTime::fromString(value.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
    return message;
}
}

AIConversationRoomRepository::AIConversationRoomRepository(QString storagePath)
    : m_storagePath(std::move(storagePath))
{
}

QString AIConversationRoomRepository::storagePath() const
{
    if (!m_storagePath.isEmpty()) {
        return m_storagePath;
    }
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("ai_conversation_rooms.json"));
}

bool AIConversationRoomRepository::load(QVector<AIConversationRoom> *rooms,
                                        QString *errorMessage,
                                        RoomRepositoryLoadStatus *status) const
{
    if (status) *status = RoomRepositoryLoadStatus::Ok;
    if (!rooms) {
        if (status) *status = RoomRepositoryLoadStatus::InvalidData;
        return false;
    }
    rooms->clear();
    QFile file(storagePath());
    if (!file.exists()) {
        if (status) *status = RoomRepositoryLoadStatus::MissingFile;
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = file.errorString();
        if (status) *status = RoomRepositoryLoadStatus::PermissionDenied;
        return false;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid room data: %1").arg(error.errorString());
        if (status) *status = RoomRepositoryLoadStatus::CorruptJson;
        return false;
    }
    const QJsonObject root = document.object();
    const int schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt();
    if (schemaVersion > 3) {
        if (errorMessage) *errorMessage = QStringLiteral("Room data was created by a newer application version and is protected from overwrite.");
        if (status) *status = RoomRepositoryLoadStatus::UnsupportedFutureVersion;
        return false;
    }
    if (schemaVersion < 1) {
        if (errorMessage) *errorMessage = QStringLiteral("Room data requires an unsupported migration.");
        if (status) *status = RoomRepositoryLoadStatus::MigrationRequired;
        return false;
    }
    QSet<QString> roomIds;
    for (const QJsonValue &roomValue : root.value(QStringLiteral("rooms")).toArray()) {
        const QJsonObject value = roomValue.toObject();
        AIConversationRoom room;
        room.roomId = value.value(QStringLiteral("roomId")).toString();
        room.roomName = value.value(QStringLiteral("roomName")).toString();
        const int mode = value.value(QStringLiteral("mode")).toInt();
        if (room.roomId.isEmpty() || roomIds.contains(room.roomId)
            || mode < static_cast<int>(AIConversationMode::Directed)
            || mode > static_cast<int>(AIConversationMode::RoundTable)) {
            if (errorMessage) *errorMessage = QStringLiteral("Invalid or duplicate room data.");
            return false;
        }
        roomIds.insert(room.roomId);
        room.mode = static_cast<AIConversationMode>(mode);
        room.minRespondersPerTurn = qMax(1, value.value(QStringLiteral("minRespondersPerTurn")).toInt(1));
        room.maxRespondersPerTurn = value.value(QStringLiteral("maxRespondersPerTurn")).toInt(2);
        room.maxRespondersPerTurn = qMax(room.minRespondersPerTurn, room.maxRespondersPerTurn);
        room.directedTargetParticipantId = value.value(QStringLiteral("directedTargetParticipantId")).toString();
        room.persistHistory = value.value(QStringLiteral("persistHistory")).toBool(true);
        const int historyLimit = value.value(QStringLiteral("historyMaxMessages")).toInt(200);
        if (historyLimit < 1 || historyLimit > 2000) {
            if (errorMessage) *errorMessage = QStringLiteral("Invalid room history limit.");
            return false;
        }
        room.historyMaxMessages = historyLimit;
        room.roundTableNextParticipantId = value.value(QStringLiteral("roundTableNextParticipantId")).toString();
        room.roundTableCursor = value.value(QStringLiteral("roundTableCursor")).toInt();
        room.turnGeneration = value.value(QStringLiteral("turnGeneration")).toVariant().toULongLong();
        room.nextMessageSequence = qMax<qint64>(1, value.value(QStringLiteral("nextMessageSequence")).toVariant().toLongLong());
        room.activeTurnId = value.value(QStringLiteral("activeTurnId")).toString();
        QSet<QString> participantIds;
        for (const QJsonValue &participant : value.value(QStringLiteral("participants")).toArray()) {
            const AIConversationParticipant parsed = participantFromJson(participant.toObject());
            if (parsed.participantId.isEmpty() || participantIds.contains(parsed.participantId)
                || parsed.speakingWeight < 0.0 || parsed.speakingWeight > 100.0) {
                if (errorMessage) *errorMessage = QStringLiteral("Invalid or duplicate participant data.");
                if (status) *status = RoomRepositoryLoadStatus::InvalidData;
                return false;
            }
            AIConversationParticipant validated = parsed;
            const QFileInfo projectFile(validated.petProjectPath);
            if (validated.petProjectPath.trimmed().isEmpty() || !projectFile.exists() || !projectFile.isFile()) {
                validated.enabled = false;
                validated.validationError = QStringLiteral("Participant pet.json is missing or unavailable.");
            }
            participantIds.insert(validated.participantId);
            room.participants.append(validated);
        }
        for (const QJsonValue &relationshipValue : value.value(QStringLiteral("relationships")).toArray()) {
            room.relationships.append(relationshipFromJson(relationshipValue.toObject()));
        }
        normalizeRoomReferences(room);
        QString validationError;
        if (!validateRoomForPersistence(room, &validationError)) {
            if (errorMessage) *errorMessage = validationError;
            if (status) *status = RoomRepositoryLoadStatus::InvalidData;
            return false;
        }
        QSet<QString> messageIds;
        int previousSequence = 0;
        int maxSequence = 0;
        for (const QJsonValue &messageValue : value.value(QStringLiteral("history")).toArray()) {
            const ConversationMessage message = messageFromJson(messageValue.toObject());
            const int messageStatus = static_cast<int>(message.status);
            const int senderType = static_cast<int>(message.senderType);
            if (message.messageId.isEmpty() || messageIds.contains(message.messageId)
                || messageStatus < static_cast<int>(ConversationMessageStatus::Pending)
                || messageStatus > static_cast<int>(ConversationMessageStatus::Interrupted)
                || senderType < static_cast<int>(ConversationSenderType::User)
                || senderType > static_cast<int>(ConversationSenderType::InternalControl)
                || message.sequence <= 0 || message.sequence <= previousSequence
                || message.roomId != room.roomId
                || ((message.senderType == ConversationSenderType::User
                     || message.senderType == ConversationSenderType::Pet) && message.turnId.isEmpty())) {
                if (errorMessage) *errorMessage = QStringLiteral("Invalid or duplicate message data.");
                if (status) *status = RoomRepositoryLoadStatus::InvalidData;
                return false;
            }
            messageIds.insert(message.messageId);
            previousSequence = message.sequence;
            maxSequence = qMax(maxSequence, message.sequence);
            room.history.append(message);
        }
        if (room.nextMessageSequence <= maxSequence) {
            if (errorMessage) *errorMessage = QStringLiteral("Room next message sequence is invalid.");
            if (status) *status = RoomRepositoryLoadStatus::InvalidData;
            return false;
        }
        rooms->append(room);
    }
    return true;
}

bool AIConversationRoomRepository::save(const QVector<AIConversationRoom> &rooms, QString *errorMessage) const
{
    const QString path = storagePath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (errorMessage) *errorMessage = QStringLiteral("Unable to create the room data directory.");
        return false;
    }
    QJsonArray array;
    for (AIConversationRoom room : rooms) {
        normalizeRoomReferences(room);
        QString validationError;
        if (!validateRoomForPersistence(room, &validationError)) {
            if (errorMessage) *errorMessage = validationError;
            return false;
        }
        QJsonObject value;
        value.insert(QStringLiteral("roomId"), room.roomId);
        value.insert(QStringLiteral("roomName"), room.roomName);
        value.insert(QStringLiteral("mode"), static_cast<int>(room.mode));
        value.insert(QStringLiteral("minRespondersPerTurn"), room.minRespondersPerTurn);
        value.insert(QStringLiteral("maxRespondersPerTurn"), room.maxRespondersPerTurn);
        value.insert(QStringLiteral("directedTargetParticipantId"), room.directedTargetParticipantId);
        value.insert(QStringLiteral("persistHistory"), room.persistHistory);
        value.insert(QStringLiteral("historyMaxMessages"), room.historyMaxMessages);
        value.insert(QStringLiteral("roundTableNextParticipantId"), room.roundTableNextParticipantId);
        value.insert(QStringLiteral("roundTableCursor"), room.roundTableCursor);
        value.insert(QStringLiteral("turnGeneration"), static_cast<qint64>(room.turnGeneration));
        value.insert(QStringLiteral("nextMessageSequence"), room.nextMessageSequence);
        value.insert(QStringLiteral("activeTurnId"), room.activeTurnId);
        QJsonArray relationships;
        for (const ParticipantRelationship &relationship : room.relationships) {
            relationships.append(relationshipToJson(relationship));
        }
        value.insert(QStringLiteral("relationships"), relationships);
        QJsonArray participants;
        for (const AIConversationParticipant &participant : room.participants) participants.append(participantToJson(participant));
        value.insert(QStringLiteral("participants"), participants);
        QJsonArray history;
        if (room.persistHistory) {
            const int start = qMax(0, room.history.size() - room.historyMaxMessages);
            for (int i = start; i < room.history.size(); ++i) {
                history.append(messageToJson(room.history.at(i)));
            }
        }
        value.insert(QStringLiteral("history"), history);
        array.append(value);
    }
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 3);
    root.insert(QStringLiteral("rooms"), array);
    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact);
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit()) {
        if (errorMessage) *errorMessage = output.errorString();
        return false;
    }
    return true;
}

bool AIConversationRoomRepository::backupCorruptStore(QString *backupPath, QString *errorMessage) const
{
    const QString sourcePath = storagePath();
    QFile source(sourcePath);
    if (!source.exists()) {
        return true;
    }

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddTHHmmsszzzZ"));
    const QString destinationPath = QStringLiteral("%1.corrupt.%2.json").arg(sourcePath, timestamp);
    if (!source.copy(destinationPath)) {
        if (errorMessage) {
            *errorMessage = source.errorString();
        }
        return false;
    }
    if (backupPath) {
        *backupPath = destinationPath;
    }
    return true;
}

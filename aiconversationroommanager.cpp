#include "aiconversationroommanager.h"

#include "aiprovider.h"
#include "aiproviderprofileregistry.h"
#include "petproject.h"

#include <QFileInfo>
#include <QPointer>
#include <QRandomGenerator>
#include <QSharedPointer>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
QString systemName()
{
    return QStringLiteral("\u7cfb\u7edf");
}

void markParticipantSpoke(AIConversationRoom *room, const QString &participantId)
{
    if (!room) {
        return;
    }
    for (AIConversationParticipant &participant : room->participants) {
        if (participant.participantId == participantId) {
            participant.lastSpokeTurn = room->turnGeneration;
            return;
        }
    }
}

bool containsParticipant(const AIConversationRoom &room, const QString &participantId)
{
    return std::any_of(room.participants.cbegin(), room.participants.cend(), [&participantId](const AIConversationParticipant &participant) {
        return participant.participantId == participantId;
    });
}

QString modeName(AIConversationMode mode)
{
    switch (mode) {
    case AIConversationMode::Directed: return QStringLiteral("Directed");
    case AIConversationMode::FreeGroup: return QStringLiteral("FreeGroup");
    case AIConversationMode::RoundTable: return QStringLiteral("RoundTable");
    }
    return QStringLiteral("Unknown");
}

struct GroupConversationContext
{
    QString roomId;
    QString roomName;
    AIConversationMode mode {AIConversationMode::Directed};
    QString turnId;
    AIConversationParticipant currentSpeaker;
    QVector<AIConversationParticipant> allParticipants;
    QString currentUserMessage;
    QVector<ChatHistoryEntry> recentHistory;
    QVector<ConversationMessage> currentTurnMessages;
    QStringList alreadyRespondedThisTurn;
    int tokenBudget {2200};
    QStringList relationshipHints;
    QStringList responseOrder;
    int estimatedRequestCharacters {0};
    int estimatedRequestTokens {0};
};

GroupConversationContext buildGroupContext(const AIConversationRoom &room,
                                           const AIConversationParticipant &currentSpeaker,
                                           const QString &currentUserMessage,
                                           const QVector<AIConversationParticipant> &responseOrder)
{
    GroupConversationContext context;
    context.roomId = room.roomId;
    context.roomName = room.roomName;
    context.mode = room.mode;
    context.turnId = room.activeTurnId;
    context.currentSpeaker = currentSpeaker;
    context.allParticipants = room.participants;
    context.currentUserMessage = currentUserMessage;
    for (const AIConversationParticipant &participant : responseOrder) {
        context.responseOrder.append(participant.participantId);
    }
    for (const ConversationMessage &message : room.history) {
        if (message.turnId != room.activeTurnId) {
            continue;
        }
        if (message.senderType == ConversationSenderType::Pet
            && message.status == ConversationMessageStatus::Completed) {
            context.alreadyRespondedThisTurn.append(message.senderId);
            context.currentTurnMessages.append(message);
        } else if (message.senderType == ConversationSenderType::System
                   && message.status != ConversationMessageStatus::Pending) {
            context.currentTurnMessages.append(message);
        }
    }
    for (const ParticipantRelationship &relationship : room.relationships) {
        if (relationship.fromParticipantId != currentSpeaker.participantId) continue;
        QString targetName = relationship.toParticipantId;
        for (const AIConversationParticipant &participant : room.participants) {
            if (participant.participantId == relationship.toParticipantId) {
                targetName = participant.characterName;
                break;
            }
        }
        QString hint = QStringLiteral("%1 -> %2").arg(currentSpeaker.characterName, targetName);
        if (!relationship.preferredAddress.trimmed().isEmpty()) {
            hint += QStringLiteral("; preferred address: %1").arg(relationship.preferredAddress.trimmed());
        }
        if (!relationship.description.trimmed().isEmpty()) {
            hint += QStringLiteral("; relationship: %1").arg(relationship.description.trimmed());
        }
        context.relationshipHints.append(hint);
    }

    int fixedCharacters = 600 + currentSpeaker.systemPrompt.size() + currentUserMessage.size()
        + room.roomId.size() + room.roomName.size() + currentSpeaker.characterName.size() + currentSpeaker.participantId.size();
    for (const AIConversationParticipant &participant : room.participants) {
        fixedCharacters += participant.characterName.size() + participant.participantId.size() + 8;
    }
    for (const ConversationMessage &message : context.currentTurnMessages) {
        fixedCharacters += message.senderName.size() + message.content.size() + 4;
    }
    for (const QString &hint : context.relationshipHints) fixedCharacters += hint.size();
    for (const QString &id : context.responseOrder) fixedCharacters += id.size() + 2;

    const int characterBudget = context.tokenBudget * 4;
    int remainingHistoryCharacters = qMax(0, characterBudget - fixedCharacters);
    int selectedHistoryCharacters = 0;
    QVector<ChatHistoryEntry> newestFirstHistory;
    for (int i = room.history.size() - 1; i >= 0 && remainingHistoryCharacters > 0; --i) {
        const ConversationMessage &message = room.history.at(i);
        if (message.senderType == ConversationSenderType::InternalControl
            || message.senderType == ConversationSenderType::System
            || message.turnId == room.activeTurnId
            || message.status != ConversationMessageStatus::Completed) {
            continue;
        }
        const QString clipped = message.content.left(remainingHistoryCharacters);
        newestFirstHistory.append({message.senderType == ConversationSenderType::User
                                       ? ChatHistoryRole::User
                                       : ChatHistoryRole::Assistant,
                                   clipped});
        remainingHistoryCharacters -= clipped.size();
        selectedHistoryCharacters += clipped.size();
    }
    context.recentHistory.reserve(newestFirstHistory.size());
    for (int i = newestFirstHistory.size() - 1; i >= 0; --i) {
        context.recentHistory.append(newestFirstHistory.at(i));
    }
    context.estimatedRequestCharacters = fixedCharacters + selectedHistoryCharacters;
    context.estimatedRequestTokens = (context.estimatedRequestCharacters + 3) / 4;
    return context;
}

QString groupContextText(const GroupConversationContext &context)
{
    QStringList identities;
    for (const AIConversationParticipant &participant : context.allParticipants) {
        identities.append(QStringLiteral("%1 (%2)%3")
                              .arg(participant.characterName,
                                   participant.participantId,
                                   participant.enabled ? QString() : QStringLiteral(" [disabled]")));
    }
    QStringList currentTurnLines;
    int remainingCurrentCharacters = context.tokenBudget / 2;
    for (const ConversationMessage &message : context.currentTurnMessages) {
        if (remainingCurrentCharacters <= 0) break;
        const QString line = QStringLiteral("%1: %2").arg(message.senderName, message.content.left(remainingCurrentCharacters));
        currentTurnLines.append(line);
        remainingCurrentCharacters -= line.size();
    }
    return QStringLiteral("Group room id: %1\nRoom name: %2\nMode: %3\nTurn id: %4\nCurrent speaker: %5 (%6)\nParticipants: %7\n"
                          "The current user message is supplied as the only user message outside this system prompt.\n"
                          "Replies already completed in this turn: %8\nAlready replied this turn: %9\nResponse order: %10\nRelationships for current speaker: %11\nToken budget: %12; estimated request characters: %13; estimated tokens: %14\n"
                          "Reply only as the current speaker. Do not write dialogue for another participant.")
        .arg(context.roomId,
             context.roomName,
             modeName(context.mode),
             context.turnId,
             context.currentSpeaker.characterName,
             context.currentSpeaker.participantId,
             identities.join(QStringLiteral(", ")),
             currentTurnLines.isEmpty() ? QStringLiteral("none") : currentTurnLines.join(QStringLiteral("\n")),
             context.alreadyRespondedThisTurn.isEmpty() ? QStringLiteral("none")
                                                       : context.alreadyRespondedThisTurn.join(QStringLiteral(", ")),
             context.responseOrder.isEmpty() ? QStringLiteral("none") : context.responseOrder.join(QStringLiteral(", ")),
             context.relationshipHints.isEmpty() ? QStringLiteral("none") : context.relationshipHints.join(QStringLiteral("\n")),
             QString::number(context.tokenBudget),
             QString::number(context.estimatedRequestCharacters),
             QString::number(context.estimatedRequestTokens));
}

ProviderLookupResult findProfileStrict(const QString &id)
{
    AIProviderProfileRegistry profiles;
    return profiles.resolveStrict(id);
}

ConversationMessage makeMessage(AIConversationRoom *room,
                                const QString &senderId,
                                const QString &senderName,
                                ConversationSenderType senderType,
                                const QString &content,
                                ConversationMessageStatus status = ConversationMessageStatus::Completed)
{
    ConversationMessage message;
    message.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    message.sequence = room ? room->nextMessageSequence++ : 0;
    message.roomId = room ? room->roomId : QString();
    message.turnId = room ? room->activeTurnId : QString();
    message.senderId = senderId;
    message.senderName = senderName;
    message.senderType = senderType;
    message.status = status;
    message.content = content;
    return message;
}

ConversationMessage makeDraftMessage(const QString &senderId,
                                     const QString &senderName,
                                     ConversationSenderType senderType,
                                     const QString &content,
                                     ConversationMessageStatus status,
                                     const QString &errorCode = {},
                                     const QString &actionId = {},
                                     const QString &actionStatus = {},
                                     int responderIndex = -1)
{
    ConversationMessage message;
    message.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    message.senderId = senderId;
    message.senderName = senderName;
    message.senderType = senderType;
    message.status = status;
    message.content = content;
    message.errorCode = errorCode;
    message.actionId = actionId;
    message.actionStatus = actionStatus;
    message.responderIndex = responderIndex;
    if (senderType == ConversationSenderType::Pet && status == ConversationMessageStatus::Completed) {
        message.desktopDeliveryStatus = DesktopDeliveryStatus::Pending;
        message.actionExecutionStatus = actionId.isEmpty()
            ? ActionExecutionStatus::NotRequested
            : ActionExecutionStatus::Pending;
    }
    return message;
}
}

AIRequestHandle *AIProviderConversationChatClient::sendMessage(const AIProviderProfile &profile,
                                                                const AIChatRequest &request,
                                                                AIProvider::ResultCallback callback)
{
    return AIProvider::instance().sendMessage(profile, request, std::move(callback));
}

AIConversationRoomManager::AIConversationRoomManager(QObject *parent)
    : AIConversationRoomManager(nullptr, 2, QString(), parent)
{
    QVector<AIConversationRoom> restored;
    QString error;
    if (m_repository.load(&restored, &error, &m_loadStatus) && restoreInterruptedTurns(&restored, &error)) {
        refreshLoadedParticipants(&restored);
        for (const AIConversationRoom &room : restored) {
            m_rooms.insert(room.roomId, room);
            m_roomOrder.append(room.roomId);
        }
    } else {
        m_loadFailure = true;
        m_loadFailureMessage = error;
        QString backupError;
        if (m_loadStatus != RoomRepositoryLoadStatus::UnsupportedFutureVersion) {
            m_repository.backupCorruptStore(&m_corruptBackupPath, &backupError);
        }
        if (!backupError.isEmpty()) {
            m_loadFailureMessage += QStringLiteral(" Backup failed: %1").arg(backupError);
        }
    }
}

AIConversationRoomManager::AIConversationRoomManager(IAIConversationChatClient *chatClient,
                                                     int defaultMaxResponders,
                                                     QObject *parent)
    : AIConversationRoomManager(chatClient, defaultMaxResponders, QString(), parent)
{
}

AIConversationRoomManager::AIConversationRoomManager(IAIConversationChatClient *chatClient,
                                                     int defaultMaxResponders,
                                                     const QString &repositoryPath,
                                                     QObject *parent)
    : QObject(parent)
    , m_chatClient(chatClient ? chatClient : &m_defaultChatClient)
    , m_defaultMaxResponders(qBound(1, defaultMaxResponders, 32))
    , m_repository(repositoryPath)
{
    if (repositoryPath.isEmpty()) {
        return;
    }
    QVector<AIConversationRoom> restored;
    QString error;
    if (m_repository.load(&restored, &error, &m_loadStatus) && restoreInterruptedTurns(&restored, &error)) {
        refreshLoadedParticipants(&restored);
        for (const AIConversationRoom &room : restored) {
            m_rooms.insert(room.roomId, room);
            m_roomOrder.append(room.roomId);
        }
        return;
    }
    m_loadFailure = true;
    m_loadFailureMessage = error;
    QString backupError;
    if (m_loadStatus != RoomRepositoryLoadStatus::UnsupportedFutureVersion) {
        m_repository.backupCorruptStore(&m_corruptBackupPath, &backupError);
    }
    if (!backupError.isEmpty()) {
        m_loadFailureMessage += QStringLiteral(" Backup failed: %1").arg(backupError);
    }
}

bool AIConversationRoomManager::restoreInterruptedTurns(QVector<AIConversationRoom> *rooms, QString *errorMessage)
{
    if (!rooms) {
        return false;
    }
    bool changed = false;
    for (AIConversationRoom &room : *rooms) {
        if (room.activeTurnId.isEmpty()) {
            continue;
        }
        const QString interruptedTurnId = room.activeTurnId;
        ConversationMessage interrupted = makeMessage(&room,
                                                       QStringLiteral("system"),
                                                       systemName(),
                                                       ConversationSenderType::System,
                                                       QStringLiteral("The previous AI turn was interrupted by application restart."),
                                                       ConversationMessageStatus::Interrupted);
        interrupted.turnId = interruptedTurnId;
        interrupted.errorCode = QStringLiteral("InterruptedOnStartup");
        room.history.append(interrupted);
        while (room.history.size() > room.historyMaxMessages) {
            room.history.removeFirst();
        }
        room.activeTurnId.clear();
        changed = true;
    }
    return !changed || m_repository.save(*rooms, errorMessage);
}

void AIConversationRoomManager::refreshLoadedParticipants(QVector<AIConversationRoom> *rooms) const
{
    if (!rooms) return;
    for (AIConversationRoom &room : *rooms) {
        for (AIConversationParticipant &participant : room.participants) {
            const bool requestedEnabled = participant.enabled;
            QString error;
            if (!refreshParticipant(&participant, &error)) {
                participant.enabled = false;
                participant.validationError = error;
            } else {
                participant.enabled = requestedEnabled;
                participant.validationError.clear();
            }
        }
        normalizeRoomReferences(room);
    }
}

AIConversationRoomManager::~AIConversationRoomManager()
{
    m_shuttingDown = true;
    for (auto it = m_rooms.begin(); it != m_rooms.end(); ++it) {
        ++it->turnGeneration;
        it->activeTurnId.clear();
    }
    m_busyRooms.clear();
    const auto activeRequests = std::exchange(m_activeTurnRequests, {});
    for (const QVector<ActiveTurnRequest> &requests : activeRequests) {
        for (const ActiveTurnRequest &request : requests) {
            if (request.handle && !request.handle->isFinished()) {
                request.handle->cancel(QStringLiteral("ManagerDestroyed"));
            }
            if (request.lease) {
                request.lease->release();
            }
        }
    }
}

QString AIConversationRoomManager::createRoom(const QString &name)
{
    if (m_loadFailure) {
        setMutationFailure(RoomMutationError::LoadFailed, {}, m_loadFailureMessage);
        return {};
    }
    AIConversationRoom room;
    room.roomId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    room.roomName = name.trimmed().isEmpty() ? QStringLiteral("\u65b0\u623f\u95f4") : name.trimmed();
    room.maxRespondersPerTurn = m_defaultMaxResponders;
    auto nextRooms = m_rooms;
    auto nextOrder = m_roomOrder;
    nextRooms.insert(room.roomId, room);
    nextOrder.append(room.roomId);
    return commitRoomState(nextRooms, nextOrder, room.roomId) ? room.roomId : QString();
}

bool AIConversationRoomManager::deleteRoom(const QString &roomId)
{
    if (!m_rooms.contains(roomId)) {
        setMutationFailure(RoomMutationError::NotFound, roomId, QStringLiteral("Room not found."));
        return false;
    }
    auto nextRooms = m_rooms;
    auto nextOrder = m_roomOrder;
    nextRooms.remove(roomId);
    nextOrder.removeAll(roomId);
    const bool wasBusy = isRoomBusy(roomId);
    if (!commitRoomState(nextRooms, nextOrder, roomId)) {
        return false;
    }
    // Removing the persisted room first makes every late callback fail its room lookup.
    m_deletingRooms.insert(roomId);
    cancelRequestsForRoom(roomId, QStringLiteral("RoomDeleted"));
    m_deletingRooms.remove(roomId);
    if (m_busyRooms.remove(roomId) || wasBusy) {
        emit turnFinished(roomId);
    }
    return true;
}

bool AIConversationRoomManager::renameRoom(const QString &roomId, const QString &name)
{
    if (!m_rooms.contains(roomId) || name.trimmed().isEmpty()) {
        setMutationFailure(name.trimmed().isEmpty() ? RoomMutationError::InvalidInput : RoomMutationError::NotFound,
                           roomId,
                           QStringLiteral("Room name is invalid or the room no longer exists."));
        return false;
    }
    auto nextRooms = m_rooms;
    nextRooms[roomId].roomName = name.trimmed();
    return commitRoomState(nextRooms, m_roomOrder, roomId);
}

bool AIConversationRoomManager::setRoomMode(const QString &roomId, AIConversationMode mode)
{
    const AIConversationRoom *current = m_rooms.contains(roomId) ? &m_rooms[roomId] : nullptr;
    return current && setRoomSettings(roomId, mode, current->minRespondersPerTurn,
                                      current->maxRespondersPerTurn, current->directedTargetParticipantId,
                                      current->persistHistory, current->historyMaxMessages);
}

bool AIConversationRoomManager::setRoomConfiguration(const QString &roomId,
                                                     int minResponders,
                                                     int maxResponders,
                                                     const QString &directedTargetParticipantId,
                                                      bool persistHistory,
                                                      int historyMaxMessages)
{
    const AIConversationRoom *current = m_rooms.contains(roomId) ? &m_rooms[roomId] : nullptr;
    return current && setRoomSettings(roomId, current->mode, minResponders, maxResponders,
                                      directedTargetParticipantId, persistHistory, historyMaxMessages);
}

bool AIConversationRoomManager::setRoomSettings(const QString &roomId,
                                                 AIConversationMode mode,
                                                 int minResponders,
                                                 int maxResponders,
                                                 const QString &directedTargetParticipantId,
                                                 bool persistHistory,
                                                 int historyMaxMessages)
{
    if (!m_rooms.contains(roomId) || isRoomBusy(roomId)) {
        setMutationFailure(isRoomBusy(roomId) ? RoomMutationError::Busy : RoomMutationError::NotFound,
                           roomId,
                           QStringLiteral("The room is unavailable for configuration changes."));
        return false;
    }
    const int boundedMin = qBound(1, minResponders, 32);
    const int boundedMax = qBound(boundedMin, maxResponders, 32);
    auto nextRooms = m_rooms;
    AIConversationRoom &nextRoom = nextRooms[roomId];
    if (mode == AIConversationMode::Directed
        && !directedTargetParticipantId.isEmpty()
        && std::none_of(nextRoom.participants.cbegin(), nextRoom.participants.cend(), [&directedTargetParticipantId](const AIConversationParticipant &participant) {
               return participant.enabled && participant.participantId == directedTargetParticipantId;
           })) {
        setMutationFailure(RoomMutationError::InvalidInput, roomId, QStringLiteral("The directed target is not an enabled participant."));
        return false;
    }
    nextRoom.mode = mode;
    nextRoom.minRespondersPerTurn = boundedMin;
    nextRoom.maxRespondersPerTurn = boundedMax;
    nextRoom.directedTargetParticipantId = directedTargetParticipantId;
    nextRoom.persistHistory = persistHistory;
    nextRoom.historyMaxMessages = qBound(1, historyMaxMessages, 2000);
    return commitRoomState(nextRooms, m_roomOrder, roomId);
}

QVector<AIConversationRoom> AIConversationRoomManager::rooms() const
{
    QVector<AIConversationRoom> result;
    result.reserve(m_roomOrder.size());
    for (const QString &roomId : m_roomOrder) {
        const auto it = m_rooms.constFind(roomId);
        if (it != m_rooms.constEnd()) result.append(it.value());
    }
    return result;
}

AIConversationRoom *AIConversationRoomManager::room(const QString &roomId)
{
    auto it = m_rooms.find(roomId);
    return it == m_rooms.end() ? nullptr : &it.value();
}

QString AIConversationRoomManager::canonicalProjectPath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

bool AIConversationRoomManager::refreshParticipant(AIConversationParticipant *participant, QString *errorMessage) const
{
    if (!participant) {
        return false;
    }
    if (m_participantResolver) {
        return m_participantResolver->resolve(participant, errorMessage);
    }
    if (participant->petProjectPath.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("A room participant must reference a pet project.");
        return false;
    }
    const QFileInfo projectFile(participant->petProjectPath);
    if (!projectFile.exists() || !projectFile.isFile()) {
        if (errorMessage) *errorMessage = QStringLiteral("The participant pet.json does not exist.");
        return false;
    }
    PetProject project;
    if (!project.load(participant->petProjectPath, errorMessage)) {
        return false;
    }
    participant->projectId = project.effectiveProjectId();
    participant->characterName = project.aiCharacterName.trimmed().isEmpty() ? project.name : project.aiCharacterName.trimmed();
    participant->systemPrompt = project.aiSystemPrompt;
    participant->providerProfileId = project.aiProviderProfileId;
    participant->allowedActionIds = project.aiTriggerActionIds();
    participant->allowedActionDescriptors = project.aiTriggerActionDescriptors();
    if (participant->projectId.trimmed().isEmpty() || participant->characterName.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("The participant project has no valid identity.");
        return false;
    }
    return true;
}

ConversationMessageStatus AIConversationRoomManager::statusFromRequestResult(const AIRequestResult &result)
{
    if (result.errorCode == QStringLiteral("TimedOut")) {
        return ConversationMessageStatus::TimedOut;
    }
    if (result.cancelled) {
        return ConversationMessageStatus::Cancelled;
    }
    return result.success ? ConversationMessageStatus::Completed : ConversationMessageStatus::Failed;
}

bool AIConversationRoomManager::mutateRoomAtomically(
    const QString &roomId,
    const std::function<bool(AIConversationRoom &)> &mutation)
{
    if (!m_rooms.contains(roomId)) {
        setMutationFailure(RoomMutationError::NotFound, roomId, QStringLiteral("Room not found."));
        return false;
    }

    auto nextRooms = m_rooms;
    AIConversationRoom &nextRoom = nextRooms[roomId];
    if (!mutation(nextRoom)) {
        return false;
    }
    while (nextRoom.history.size() > nextRoom.historyMaxMessages) {
        nextRoom.history.removeFirst();
    }
    return commitRoomState(nextRooms, m_roomOrder, roomId);
}

bool AIConversationRoomManager::appendMessageAtomically(const QString &roomId,
                                                         const QString &senderId,
                                                         const QString &senderName,
                                                         ConversationSenderType senderType,
                                                         const QString &content,
                                                         ConversationMessageStatus status,
                                                         const QString &errorCode,
                                                         const QString &actionId,
                                                         const QString &actionStatus,
                                                         int responderIndex,
                                                         ConversationMessage *committedMessage)
{
    ConversationMessage message;
    const bool saved = mutateRoomAtomically(roomId, [&](AIConversationRoom &nextRoom) {
        message = makeMessage(&nextRoom, senderId, senderName, senderType, content, status);
        message.errorCode = errorCode;
        message.actionId = actionId;
        message.actionStatus = actionStatus;
        message.responderIndex = responderIndex;
        nextRoom.history.append(message);
        return true;
    });
    if (saved && committedMessage) {
        *committedMessage = message;
    }
    return saved;
}

bool AIConversationRoomManager::completeResponderSlotAtomically(const QString &roomId,
                                                                int responderIndex,
                                                                const ConversationMessage &completed,
                                                                ConversationMessage *committedMessage)
{
    ConversationMessage committed;
    const bool saved = mutateRoomAtomically(roomId, [&](AIConversationRoom &nextRoom) {
        for (ConversationMessage &pending : nextRoom.history) {
            if (pending.turnId != nextRoom.activeTurnId
                || pending.responderIndex != responderIndex
                || pending.status != ConversationMessageStatus::Pending) {
                continue;
            }
            committed = completed;
            committed.messageId = pending.messageId;
            committed.sequence = pending.sequence;
            committed.roomId = pending.roomId;
            committed.turnId = pending.turnId;
            committed.responderIndex = responderIndex;
            pending = committed;
            if (pending.senderType == ConversationSenderType::Pet) {
                markParticipantSpoke(&nextRoom, pending.senderId);
            }
            return true;
        }

        committed = makeMessage(&nextRoom,
                                completed.senderId,
                                completed.senderName,
                                completed.senderType,
                                completed.content,
                                completed.status);
        committed.errorCode = completed.errorCode;
        committed.actionId = completed.actionId;
        committed.actionParameters = completed.actionParameters;
        committed.desktopDeliveryStatus = completed.desktopDeliveryStatus;
        committed.actionExecutionStatus = completed.actionExecutionStatus;
        committed.actionStatus = completed.actionStatus;
        committed.responderIndex = responderIndex;
        nextRoom.history.append(committed);
        if (committed.senderType == ConversationSenderType::Pet) {
            markParticipantSpoke(&nextRoom, committed.senderId);
        }
        return true;
    });
    if (saved && committedMessage) {
        *committedMessage = committed;
    }
    return saved;
}

bool AIConversationRoomManager::addParticipantFromProject(const QString &roomId,
                                                          const QString &petJsonPath,
                                                          QString *errorMessage)
{
    const auto roomIt = m_rooms.constFind(roomId);
    if (roomIt == m_rooms.constEnd()) {
        if (errorMessage) *errorMessage = QStringLiteral("\u623f\u95f4\u4e0d\u5b58\u5728\u3002");
        setMutationFailure(RoomMutationError::NotFound, roomId, QStringLiteral("Room not found."));
        return false;
    }

    PetProject project;
    if (!project.load(petJsonPath, errorMessage)) {
        return false;
    }

    const QString projectId = project.effectiveProjectId();
    for (const AIConversationParticipant &participant : roomIt->participants) {
        if (participant.projectId == projectId || canonicalProjectPath(participant.petProjectPath) == canonicalProjectPath(petJsonPath)) {
            if (errorMessage) *errorMessage = QStringLiteral("\u8fd9\u4e2a\u684c\u5ba0\u5df2\u7ecf\u5728\u623f\u95f4\u91cc\u3002");
            return false;
        }
    }

    AIConversationParticipant participant;
    participant.participantId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    participant.petProjectPath = canonicalProjectPath(petJsonPath);
    participant.projectId = projectId;
    participant.characterName = project.aiCharacterName.trimmed().isEmpty() ? project.name : project.aiCharacterName.trimmed();
    participant.systemPrompt = project.aiSystemPrompt;
    participant.providerProfileId = project.aiProviderProfileId;
    participant.allowedActionIds = project.aiTriggerActionIds();
    participant.allowedActionDescriptors = project.aiTriggerActionDescriptors();
    auto nextRooms = m_rooms;
    nextRooms[roomId].participants.append(participant);
    if (!commitRoomState(nextRooms, m_roomOrder, roomId)) {
        if (errorMessage) *errorMessage = m_lastMutationResult.message;
        return false;
    }
    return true;
}

bool AIConversationRoomManager::removeParticipant(const QString &roomId, const QString &participantId)
{
    const auto roomIt = m_rooms.constFind(roomId);
    if (roomIt == m_rooms.constEnd()) {
        setMutationFailure(RoomMutationError::NotFound, roomId, QStringLiteral("Room not found."));
        return false;
    }
    auto nextRooms = m_rooms;
    QVector<AIConversationParticipant> &participants = nextRooms[roomId].participants;
    const qsizetype oldSize = participants.size();
    participants.erase(std::remove_if(participants.begin(),
                                                  participants.end(),
                                                  [&participantId](const AIConversationParticipant &participant) {
                                                      return participant.participantId == participantId;
                                                  }),
                                   participants.end());
    if (participants.size() == oldSize) {
        setMutationFailure(RoomMutationError::NotFound, roomId, QStringLiteral("Participant not found."));
        return false;
    }
    for (ConversationMessage &message : nextRooms[roomId].history) {
        if (message.senderId == participantId && message.status == ConversationMessageStatus::Pending) {
            message.status = ConversationMessageStatus::Cancelled;
            message.content = QStringLiteral("AI request cancelled because the participant was removed.");
            message.errorCode = QStringLiteral("ParticipantRemoved");
            message.actionStatus = QStringLiteral("NotRequested");
        }
    }
    if (!commitRoomState(nextRooms, m_roomOrder, roomId)) {
        return false;
    }
    // The committed participant set is checked by every callback before it writes a reply.
    cancelRequestsForParticipant(roomId, participantId, QStringLiteral("ParticipantRemoved"));
    return true;
}

bool AIConversationRoomManager::setParticipantEnabled(const QString &roomId,
                                                      const QString &participantId,
                                                      bool enabled)
{
    if (!m_rooms.contains(roomId) || isRoomBusy(roomId)) {
        setMutationFailure(isRoomBusy(roomId) ? RoomMutationError::Busy : RoomMutationError::NotFound,
                           roomId,
                           QStringLiteral("The participant cannot be changed while the room is busy."));
        return false;
    }
    auto nextRooms = m_rooms;
    for (AIConversationParticipant &participant : nextRooms[roomId].participants) {
        if (participant.participantId == participantId) {
            participant.enabled = enabled;
            if (!enabled && nextRooms[roomId].directedTargetParticipantId == participantId) {
                nextRooms[roomId].directedTargetParticipantId.clear();
            }
            return commitRoomState(nextRooms, m_roomOrder, roomId);
        }
    }
    setMutationFailure(RoomMutationError::NotFound, roomId, QStringLiteral("Participant not found."));
    return false;
}

bool AIConversationRoomManager::setParticipantWeight(const QString &roomId,
                                                      const QString &participantId,
                                                      double weight)
{
    if (!m_rooms.contains(roomId) || isRoomBusy(roomId) || weight < 0.0 || weight > 100.0) {
        setMutationFailure(RoomMutationError::InvalidInput, roomId, QStringLiteral("The participant weight is invalid."));
        return false;
    }
    auto nextRooms = m_rooms;
    for (AIConversationParticipant &participant : nextRooms[roomId].participants) {
        if (participant.participantId == participantId) {
            participant.speakingWeight = weight;
            return commitRoomState(nextRooms, m_roomOrder, roomId);
        }
    }
    setMutationFailure(RoomMutationError::NotFound, roomId, QStringLiteral("Participant not found."));
    return false;
}

bool AIConversationRoomManager::setParticipantRelationship(
    const QString &roomId,
    const ParticipantRelationship &relationship)
{
    if (!m_rooms.contains(roomId) || isRoomBusy(roomId)
        || relationship.fromParticipantId.isEmpty()
        || relationship.toParticipantId.isEmpty()
        || relationship.fromParticipantId == relationship.toParticipantId) {
        setMutationFailure(isRoomBusy(roomId) ? RoomMutationError::Busy : RoomMutationError::InvalidInput,
                           roomId,
                           QStringLiteral("The participant relationship is invalid or the room is busy."));
        return false;
    }
    auto nextRooms = m_rooms;
    AIConversationRoom &nextRoom = nextRooms[roomId];
    if (!containsParticipant(nextRoom, relationship.fromParticipantId)
        || !containsParticipant(nextRoom, relationship.toParticipantId)) {
        setMutationFailure(RoomMutationError::InvalidInput,
                           roomId,
                           QStringLiteral("Both relationship participants must belong to the room."));
        return false;
    }
    for (ParticipantRelationship &existing : nextRoom.relationships) {
        if (existing.fromParticipantId == relationship.fromParticipantId
            && existing.toParticipantId == relationship.toParticipantId) {
            existing = relationship;
            return commitRoomState(nextRooms, m_roomOrder, roomId);
        }
    }
    nextRoom.relationships.append(relationship);
    return commitRoomState(nextRooms, m_roomOrder, roomId);
}

bool AIConversationRoomManager::removeParticipantRelationship(const QString &roomId,
                                                              const QString &fromParticipantId,
                                                              const QString &toParticipantId)
{
    if (!m_rooms.contains(roomId) || isRoomBusy(roomId)
        || fromParticipantId.isEmpty() || toParticipantId.isEmpty()) {
        setMutationFailure(isRoomBusy(roomId) ? RoomMutationError::Busy : RoomMutationError::InvalidInput,
                           roomId,
                           QStringLiteral("The participant relationship cannot be removed."));
        return false;
    }
    auto nextRooms = m_rooms;
    AIConversationRoom &nextRoom = nextRooms[roomId];
    const auto oldEnd = nextRoom.relationships.end();
    const auto newEnd = std::remove_if(nextRoom.relationships.begin(), oldEnd,
                                       [&fromParticipantId, &toParticipantId](const ParticipantRelationship &relationship) {
        return relationship.fromParticipantId == fromParticipantId
            && relationship.toParticipantId == toParticipantId;
    });
    if (newEnd == oldEnd) {
        setMutationFailure(RoomMutationError::InvalidInput,
                           roomId,
                           QStringLiteral("The participant relationship does not exist."));
        return false;
    }
    nextRoom.relationships.erase(newEnd, oldEnd);
    return commitRoomState(nextRooms, m_roomOrder, roomId);
}

bool AIConversationRoomManager::clearHistory(const QString &roomId)
{
    if (!m_rooms.contains(roomId) || isRoomBusy(roomId)) {
        setMutationFailure(isRoomBusy(roomId) ? RoomMutationError::Busy : RoomMutationError::NotFound,
                           roomId,
                           QStringLiteral("The room history cannot be cleared right now."));
        return false;
    }
    auto nextRooms = m_rooms;
    nextRooms[roomId].history.clear();
    return commitRoomState(nextRooms, m_roomOrder, roomId);
}

bool AIConversationRoomManager::submitUserMessage(const QString &roomId,
                                                  const QString &message,
                                                  const QStringList &targetParticipantIds)
{
    AIConversationRoom *targetRoom = room(roomId);
    if (!targetRoom || message.trimmed().isEmpty() || isRoomBusy(roomId)) {
        setMutationFailure(!targetRoom ? RoomMutationError::NotFound
                                       : (message.trimmed().isEmpty() ? RoomMutationError::InvalidInput
                                                                      : RoomMutationError::Busy),
                           roomId,
                           !targetRoom ? QStringLiteral("Room not found.")
                                       : (message.trimmed().isEmpty() ? QStringLiteral("Message is empty.")
                                                                      : QStringLiteral("The room is already generating a reply.")));
        return false;
    }
    QStringList effectiveTargetIds = targetParticipantIds;
    if (targetRoom->mode == AIConversationMode::Directed) {
        effectiveTargetIds = targetRoom->directedTargetParticipantId.isEmpty()
            ? QStringList()
            : QStringList {targetRoom->directedTargetParticipantId};
    }
    if (targetRoom->mode == AIConversationMode::Directed && effectiveTargetIds.size() != 1) {
        setMutationFailure(RoomMutationError::InvalidInput,
                           roomId,
                           QStringLiteral("Directed mode requires exactly one enabled target participant."));
        return false;
    }

    QVector<AIConversationParticipant> responders = selectRespondersForRoom(*targetRoom, effectiveTargetIds);
    if (responders.isEmpty()) {
        setMutationFailure(RoomMutationError::InvalidInput,
                           roomId,
                           QStringLiteral("No enabled participant is available for this turn."));
        return false;
    }
    for (AIConversationParticipant &participant : responders) {
        QString participantError;
        if (!refreshParticipant(&participant, &participantError)) {
            setMutationFailure(RoomMutationError::InvalidInput,
                               roomId,
                               QStringLiteral("Participant is invalid: %1").arg(participantError));
            return false;
        }
        if (!m_participantResolver) {
            const ProviderLookupResult profile = findProfileStrict(participant.providerProfileId);
            if (!profile.ok) {
                setMutationFailure(RoomMutationError::InvalidInput,
                                   roomId,
                                   QStringLiteral("Participant AI provider is invalid: %1").arg(profile.message));
                return false;
            }
        }
    }

    const AIConversationMode dispatchMode = targetRoom->mode;
    auto nextRooms = m_rooms;
    AIConversationRoom &nextRoom = nextRooms[roomId];
    ++nextRoom.turnGeneration;
    nextRoom.activeTurnId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    nextRoom.history.append(makeMessage(&nextRoom,
                                        QStringLiteral("operator"),
                                        QStringLiteral("\u4f60"),
                                        ConversationSenderType::User,
                                        message.trimmed()));
    for (int index = 0; index < responders.size(); ++index) {
        const AIConversationParticipant &participant = responders.at(index);
        ConversationMessage pending = makeMessage(&nextRoom,
                                                   participant.participantId,
                                                   participant.characterName,
                                                   ConversationSenderType::Pet,
                                                   QStringLiteral("Thinking..."),
                                                   ConversationMessageStatus::Pending);
        pending.responderIndex = index;
        pending.actionStatus = QStringLiteral("Pending");
        nextRoom.history.append(pending);
    }
    while (nextRoom.history.size() > nextRoom.historyMaxMessages) {
        nextRoom.history.removeFirst();
    }
    if (!commitRoomState(nextRooms, m_roomOrder, roomId)) {
        return false;
    }

    m_busyRooms.insert(roomId);
    const quint64 turnGeneration = m_rooms.value(roomId).turnGeneration;
    emit roomUpdated(roomId);
    emit turnStarted(roomId);
    if (dispatchMode == AIConversationMode::FreeGroup) {
        runResponderParallel(roomId, responders, message.trimmed(), turnGeneration);
    } else {
        runResponderSequence(roomId, responders, 0, message.trimmed(), turnGeneration);
    }
    return true;
}

bool AIConversationRoomManager::cancelTurn(const QString &roomId)
{
    if (!m_rooms.contains(roomId) || !isRoomBusy(roomId)) {
        return false;
    }
    if (!mutateRoomAtomically(roomId, [](AIConversationRoom &nextRoom) {
            const QString cancelledTurnId = nextRoom.activeTurnId;
            ++nextRoom.turnGeneration;
            ConversationMessage cancelled = makeMessage(&nextRoom,
                                                         QStringLiteral("system"),
                                                         systemName(),
                                                         ConversationSenderType::System,
                                                         QStringLiteral("\u5df2\u53d6\u6d88\u672c\u8f6e AI \u8bf7\u6c42\u3002"),
                                                         ConversationMessageStatus::Cancelled);
            cancelled.turnId = cancelledTurnId;
            cancelled.errorCode = QStringLiteral("CancelledByUser");
            for (ConversationMessage &pending : nextRoom.history) {
                if (pending.turnId == cancelledTurnId && pending.status == ConversationMessageStatus::Pending) {
                    pending.status = ConversationMessageStatus::Cancelled;
                    pending.content = QStringLiteral("AI request cancelled.");
                    pending.errorCode = QStringLiteral("CancelledByUser");
                    pending.actionStatus = QStringLiteral("NotRequested");
                }
            }
            nextRoom.history.append(cancelled);
            nextRoom.activeTurnId.clear();
            return true;
        })) {
        return false;
    }
    cancelRequestsForRoom(roomId, QStringLiteral("CancelledByUser"));
    finishTurn(roomId);
    return true;
}

bool AIConversationRoomManager::recordRuntimeDispatchFailure(const QString &roomId, const QString &messageId)
{
    RuntimeReplyDeliveryResult result;
    result.textDelivered = false;
    result.errorCode = QStringLiteral("PetNotRunning");
    result.message = QStringLiteral("The target desktop pet is not running.");
    result.action = {RuntimeActionDispatchStatus::Failed, result.errorCode, result.message};
    const bool changed = recordRuntimeDeliveryResult(roomId, messageId, result);
    if (changed) {
        emit roomError(roomId, QStringLiteral("The target desktop pet is not running, so its reply was not delivered."));
    }
    return changed;
}

bool AIConversationRoomManager::recordRuntimeDispatchDelivered(const QString &roomId, const QString &messageId)
{
    if (messageId.isEmpty()) return false;
    return mutateRoomAtomically(roomId, [&messageId](AIConversationRoom &nextRoom) {
        for (ConversationMessage &message : nextRoom.history) {
            if (message.messageId == messageId) {
                message.desktopDeliveryStatus = DesktopDeliveryStatus::Delivered;
                return true;
            }
        }
        return false;
    });
}

bool AIConversationRoomManager::recordRuntimeDeliveryResult(const QString &roomId,
                                                             const QString &messageId,
                                                             const RuntimeReplyDeliveryResult &result)
{
    if (messageId.isEmpty()) return false;
    return mutateRoomAtomically(roomId, [&messageId, &result](AIConversationRoom &nextRoom) {
        for (ConversationMessage &message : nextRoom.history) {
            if (message.messageId == messageId) {
                if (result.textDelivered) {
                    message.desktopDeliveryStatus = DesktopDeliveryStatus::Delivered;
                } else if (result.errorCode == QStringLiteral("PetNotRunning")) {
                    message.desktopDeliveryStatus = DesktopDeliveryStatus::PetNotRunning;
                } else {
                    message.desktopDeliveryStatus = DesktopDeliveryStatus::Failed;
                }
                const RuntimeActionDispatchStatus actionStatus = message.actionId.isEmpty()
                    ? RuntimeActionDispatchStatus::NotRequested
                    : result.action.status;
                switch (actionStatus) {
                case RuntimeActionDispatchStatus::NotRequested:
                    message.actionExecutionStatus = ActionExecutionStatus::NotRequested;
                    message.actionStatus = QStringLiteral("NotRequested");
                    break;
                case RuntimeActionDispatchStatus::Executed:
                    message.actionExecutionStatus = ActionExecutionStatus::Executed;
                    message.actionStatus = QStringLiteral("Executed");
                    break;
                case RuntimeActionDispatchStatus::Queued:
                    message.actionExecutionStatus = ActionExecutionStatus::Queued;
                    message.actionStatus = QStringLiteral("Queued");
                    break;
                case RuntimeActionDispatchStatus::Rejected:
                    message.actionExecutionStatus = ActionExecutionStatus::Rejected;
                    message.actionStatus = QStringLiteral("Rejected");
                    break;
                case RuntimeActionDispatchStatus::Failed:
                    message.actionExecutionStatus = ActionExecutionStatus::Failed;
                    message.actionStatus = QStringLiteral("Failed");
                    break;
                }
                message.errorCode = !result.action.errorCode.isEmpty() ? result.action.errorCode : result.errorCode;
                return true;
            }
        }
        return false;
    });
}

bool AIConversationRoomManager::recordRuntimeActionResult(const QString &roomId,
                                                           const QString &messageId,
                                                           const RuntimeActionDispatchResult &result)
{
    if (messageId.isEmpty()) return false;
    return mutateRoomAtomically(roomId, [&messageId, &result](AIConversationRoom &nextRoom) {
        for (ConversationMessage &message : nextRoom.history) {
            if (message.messageId != messageId) continue;
            switch (result.status) {
            case RuntimeActionDispatchStatus::NotRequested: message.actionExecutionStatus = ActionExecutionStatus::NotRequested; break;
            case RuntimeActionDispatchStatus::Executed: message.actionExecutionStatus = ActionExecutionStatus::Executed; break;
            case RuntimeActionDispatchStatus::Queued: message.actionExecutionStatus = ActionExecutionStatus::Queued; break;
            case RuntimeActionDispatchStatus::Rejected: message.actionExecutionStatus = ActionExecutionStatus::Rejected; break;
            case RuntimeActionDispatchStatus::Failed: message.actionExecutionStatus = ActionExecutionStatus::Failed; break;
            }
            message.actionStatus = result.status == RuntimeActionDispatchStatus::Executed ? QStringLiteral("Executed")
                : result.status == RuntimeActionDispatchStatus::Queued ? QStringLiteral("Queued")
                : result.status == RuntimeActionDispatchStatus::Rejected ? QStringLiteral("Rejected")
                : result.status == RuntimeActionDispatchStatus::Failed ? QStringLiteral("Failed")
                : QStringLiteral("NotRequested");
            message.errorCode = result.errorCode;
            return true;
        }
        return false;
    });
}

bool AIConversationRoomManager::recoverFromLoadFailure()
{
    if (!m_loadFailure) {
        return true;
    }
    if (m_loadStatus == RoomRepositoryLoadStatus::UnsupportedFutureVersion) {
        setMutationFailure(RoomMutationError::LoadFailed,
                           {},
                           QStringLiteral("Newer room data is read-only and cannot be replaced by this application version."));
        return false;
    }
    QHash<QString, AIConversationRoom> emptyRooms;
    QVector<QString> emptyOrder;
    if (!commitRoomState(emptyRooms, emptyOrder)) {
        return false;
    }
    m_loadFailure = false;
    m_loadFailureMessage.clear();
    return true;
}

void AIConversationRoomManager::trackRequest(const QString &roomId,
                                             const QString &participantId,
                                             AIRequestHandle *handle,
                                             const QSharedPointer<PetAiRequestLease> &lease)
{
    if (m_shuttingDown) {
        if (lease) lease->release();
        return;
    }
    if (!lease || (handle && handle->isFinished())) {
        return;
    }
    if (!isRoomBusy(roomId)) {
        lease->release();
        return;
    }
    lease->bindHandle(handle);
    m_activeTurnRequests[roomId].append({participantId, handle, lease});
}

void AIConversationRoomManager::cancelRequestsForRoom(const QString &roomId, const QString &reason)
{
    const QVector<ActiveTurnRequest> active = m_activeTurnRequests.take(roomId);
    for (const ActiveTurnRequest &request : active) {
        if (request.handle && !request.handle->isFinished()) {
            request.handle->cancel(reason);
        }
        if (request.lease) {
            request.lease->release();
        }
    }
}

void AIConversationRoomManager::cancelRequestsForParticipant(const QString &roomId,
                                                              const QString &participantId,
                                                              const QString &reason)
{
    const QVector<ActiveTurnRequest> active = m_activeTurnRequests.take(roomId);
    QVector<ActiveTurnRequest> remaining;
    QVector<ActiveTurnRequest> cancelled;
    for (const ActiveTurnRequest &request : active) {
        (request.participantId == participantId ? cancelled : remaining).append(request);
    }
    if (!remaining.isEmpty()) {
        m_activeTurnRequests.insert(roomId, remaining);
    }
    for (const ActiveTurnRequest &request : cancelled) {
        if (request.handle && !request.handle->isFinished()) {
            request.handle->cancel(reason);
        }
        if (request.lease) {
            request.lease->release();
        }
    }
}

bool AIConversationRoomManager::isRoomBusy(const QString &roomId) const
{
    return m_busyRooms.contains(roomId);
}

QVector<AIConversationParticipant> AIConversationRoomManager::selectRespondersForRoom(const AIConversationRoom &room,
                                                                                      const QStringList &targetParticipantIds)
{
    return selectRespondersForRoom(room, targetParticipantIds, [](double upper) {
        return QRandomGenerator::global()->generateDouble() * upper;
    });
}

QVector<AIConversationParticipant> AIConversationRoomManager::selectRespondersForRoom(
    const AIConversationRoom &room,
    const QStringList &targetParticipantIds,
    std::function<double(double)> randomBelow)
{
    QVector<AIConversationParticipant> enabled;
    for (const AIConversationParticipant &participant : room.participants) {
        if (participant.enabled) {
            enabled.append(participant);
        }
    }

    if (room.mode == AIConversationMode::Directed) {
        for (const QString &targetId : targetParticipantIds) {
            for (const AIConversationParticipant &participant : enabled) {
                if (participant.participantId == targetId) {
                    return {participant};
                }
            }
        }
        return {};
    }

    if (enabled.size() < qMax(1, room.minRespondersPerTurn)) {
        return {};
    }
    const int lowerCount = qMin(room.minRespondersPerTurn, enabled.size());
    const int upperCount = qMin(qMax(room.minRespondersPerTurn, room.maxRespondersPerTurn), enabled.size());
    const int count = room.mode == AIConversationMode::FreeGroup && lowerCount != upperCount
        ? lowerCount + qBound(0, static_cast<int>(randomBelow(upperCount - lowerCount + 1)), upperCount - lowerCount)
        : upperCount;
    if (room.mode == AIConversationMode::RoundTable) {
        QVector<AIConversationParticipant> result;
        if (enabled.isEmpty()) {
            return result;
        }
        int cursor = 0;
        for (int i = 0; i < enabled.size(); ++i) {
            if (enabled.at(i).participantId == room.roundTableNextParticipantId) {
                cursor = i;
                break;
            }
        }
        for (int i = 0; i < count; ++i) {
            result.append(enabled.at((cursor + i) % enabled.size()));
        }
        return result;
    }

    QVector<AIConversationParticipant> pool;
    for (const AIConversationParticipant &participant : enabled) {
        if (participant.speakingWeight > 0.0) {
            pool.append(participant);
        }
    }
    if (pool.size() < room.minRespondersPerTurn) {
        return {};
    }

    QVector<AIConversationParticipant> result;
    while (!pool.isEmpty() && result.size() < count) {
        double total = 0.0;
        for (const AIConversationParticipant &participant : pool) {
            const quint64 turnsSinceSpoke = room.turnGeneration > participant.lastSpokeTurn
                ? room.turnGeneration - participant.lastSpokeTurn
                : 0;
            const double recentPenalty = turnsSinceSpoke <= 1 ? 0.35 : 1.0;
            const double waitingBonus = qMin(2.0, static_cast<double>(turnsSinceSpoke) * 0.15);
            total += qMax(0.0, participant.speakingWeight) * recentPenalty * (1.0 + waitingBonus);
        }
        if (total <= 0.0) {
            break;
        }
        double pick = qBound(0.0, randomBelow(total), std::nextafter(total, 0.0));
        int chosen = 0;
        for (int i = 0; i < pool.size(); ++i) {
            const AIConversationParticipant &candidate = pool.at(i);
            const quint64 turnsSinceSpoke = room.turnGeneration > candidate.lastSpokeTurn
                ? room.turnGeneration - candidate.lastSpokeTurn
                : 0;
            const double recentPenalty = turnsSinceSpoke <= 1 ? 0.35 : 1.0;
            const double waitingBonus = qMin(2.0, static_cast<double>(turnsSinceSpoke) * 0.15);
            pick -= qMax(0.0, candidate.speakingWeight) * recentPenalty * (1.0 + waitingBonus);
            if (pick <= 0.0) {
                chosen = i;
                break;
            }
        }
        result.append(pool.takeAt(chosen));
    }
    return result;
}

void AIConversationRoomManager::advanceRoundTableAfterSuccessfulTurn(
    AIConversationRoom *room,
    const QVector<AIConversationParticipant> &responders)
{
    if (!room || room->mode != AIConversationMode::RoundTable || responders.isEmpty()) {
        return;
    }
    QVector<AIConversationParticipant> enabled;
    for (const AIConversationParticipant &participant : room->participants) {
        if (participant.enabled) {
            enabled.append(participant);
        }
    }
    if (enabled.isEmpty()) {
        room->roundTableNextParticipantId.clear();
        room->roundTableCursor = 0;
        return;
    }
    const QString lastId = responders.last().participantId;
    int lastIndex = -1;
    for (int i = 0; i < enabled.size(); ++i) {
        if (enabled.at(i).participantId == lastId) {
            lastIndex = i;
            break;
        }
    }
    const int nextIndex = lastIndex < 0 ? 0 : (lastIndex + 1) % enabled.size();
    room->roundTableNextParticipantId = enabled.at(nextIndex).participantId;
    room->roundTableCursor = nextIndex;
}

void AIConversationRoomManager::advanceRoundTableAfterPlannedTurn(
    const QString &roomId,
    const QVector<AIConversationParticipant> &responders,
    int completedSlot)
{
    if (completedSlot != responders.size() - 1) {
        return;
    }
    mutateRoomAtomically(roomId, [this, &responders](AIConversationRoom &nextRoom) {
        if (nextRoom.mode == AIConversationMode::RoundTable) {
            advanceRoundTableAfterSuccessfulTurn(&nextRoom, responders);
        }
        return true;
    });
}

QString AIConversationRoomManager::transcriptForRoom(const AIConversationRoom &room)
{
    QStringList lines;
    const int start = qMax(0, room.history.size() - 30);
    for (int i = start; i < room.history.size(); ++i) {
        const ConversationMessage &message = room.history.at(i);
        if (message.senderType == ConversationSenderType::InternalControl) {
            continue;
        }
        lines.append(QStringLiteral("%1: %2").arg(message.senderName, message.content));
    }
    return lines.join('\n');
}

void AIConversationRoomManager::runResponderSequence(const QString &roomId,
                                                     const QVector<AIConversationParticipant> &responders,
                                                     int index,
                                                     const QString &latestUserMessage,
                                                     quint64 turnGeneration)
{
    if (m_shuttingDown) {
        return;
    }
    AIConversationRoom *targetRoom = room(roomId);
    if (!targetRoom || targetRoom->turnGeneration != turnGeneration || index >= responders.size()) {
        if (!m_deletingRooms.contains(roomId)) {
            finishTurn(roomId);
        }
        return;
    }
    const QString roomName = targetRoom->roomName;

    AIConversationParticipant participant = responders.at(index);
    if (!containsParticipant(*targetRoom, participant.participantId)) {
        advanceRoundTableAfterPlannedTurn(roomId, responders, index);
        runResponderSequence(roomId, responders, index + 1, latestUserMessage, turnGeneration);
        return;
    }
    QString refreshError;
    if (!refreshParticipant(&participant, &refreshError)) {
        const ConversationMessage failed = makeDraftMessage(participant.participantId,
                                                            participant.characterName,
                                                            ConversationSenderType::System,
                                                            QStringLiteral("%1 \u914d\u7f6e\u91cd\u8f7d\u5931\u8d25: %2").arg(participant.characterName, refreshError),
                                                            ConversationMessageStatus::Failed,
                                                            QStringLiteral("ParticipantRefreshFailed"), {}, {}, index);
        if (!completeResponderSlotAtomically(roomId, index, failed)) {
            finishTurn(roomId);
            return;
        }
        advanceRoundTableAfterPlannedTurn(roomId, responders, index);
        runResponderSequence(roomId, responders, index + 1, latestUserMessage, turnGeneration);
        return;
    }

    const ProviderLookupResult profileResult = findProfileStrict(participant.providerProfileId);
    if (!profileResult.ok && !m_participantResolver) {
        const ConversationMessage failed = makeDraftMessage(participant.participantId,
                                                            participant.characterName,
                                                            ConversationSenderType::System,
                                                            QStringLiteral("%1 AI \u670d\u52a1\u672a\u914d\u7f6e\uff1a%2").arg(participant.characterName, profileResult.message),
                                                            ConversationMessageStatus::Failed,
                                                            QStringLiteral("ProviderNotConfigured"), {}, {}, index);
        if (!completeResponderSlotAtomically(roomId, index, failed)) {
            finishTurn(roomId);
            return;
        }
        advanceRoundTableAfterPlannedTurn(roomId, responders, index);
        runResponderSequence(roomId, responders, index + 1, latestUserMessage, turnGeneration);
        return;
    }

    QString leaseError;
    const QString leaseProjectId = participant.projectId.trimmed().isEmpty()
        ? QStringLiteral("room:%1:%2").arg(roomId, participant.participantId)
        : participant.projectId;
    const auto lease = PetAiRequestCoordinator::instance().acquire(leaseProjectId,
                                                                     participant.petProjectPath,
                                                                     PetAiRequestSource::MultiAi,
                                                                     roomId,
                                                                     &leaseError);
    if (!lease) {
        const ConversationMessage failed = makeDraftMessage(participant.participantId,
                                                            participant.characterName,
                                                            ConversationSenderType::System,
                                                            QStringLiteral("%1 AI \u8bf7\u6c42\u672a\u53d1\u9001\uff1a%2").arg(participant.characterName, leaseError),
                                                            ConversationMessageStatus::Failed,
                                                            QStringLiteral("LeaseUnavailable"), {}, {}, index);
        if (!completeResponderSlotAtomically(roomId, index, failed)) {
            finishTurn(roomId);
            return;
        }
        advanceRoundTableAfterPlannedTurn(roomId, responders, index);
        runResponderSequence(roomId, responders, index + 1, latestUserMessage, turnGeneration);
        return;
    }

    AIChatRequest request;
    request.characterName = participant.characterName;
    const GroupConversationContext context = buildGroupContext(*targetRoom, participant, latestUserMessage, responders);
    request.systemPrompt = participant.systemPrompt + QStringLiteral("\n\n")
        + groupContextText(context);
    request.history = context.recentHistory;
    request.userMessage = context.currentUserMessage;
    request.allowedActionIds = participant.allowedActionIds;
    request.allowedActionDescriptors = participant.allowedActionDescriptors;

    QPointer<AIConversationRoomManager> guard(this);
    AIRequestHandle *handle = m_chatClient->sendMessage(profileResult.profile, request, [guard, roomId, roomName, responders, index, participant, latestUserMessage, turnGeneration, lease](const AIRequestResult &result) {
        if (!guard || guard->m_shuttingDown) {
            return;
        }
        AIConversationRoom *callbackRoom = guard->room(roomId);
        if (!callbackRoom) {
            if (!guard->m_deletingRooms.contains(roomId)) {
                guard->finishTurn(roomId);
            }
            return;
        }
        if (callbackRoom->turnGeneration != turnGeneration) {
            return;
        }
        if (!containsParticipant(*callbackRoom, participant.participantId)) {
            guard->advanceRoundTableAfterPlannedTurn(roomId, responders, index);
            guard->runResponderSequence(roomId, responders, index + 1, latestUserMessage, turnGeneration);
            return;
        }

        if (result.success) {
            const AIChatReply parsed = AIProvider::parseStructuredReply(result.message, participant.allowedActionIds);
            ConversationMessage petMessage;
            const QString content = parsed.reply.trimmed().isEmpty() ? result.message.trimmed() : parsed.reply.trimmed();
            ConversationMessage draft = makeDraftMessage(participant.participantId,
                                                               participant.characterName,
                                                               ConversationSenderType::Pet,
                                                               content,
                                                               ConversationMessageStatus::Completed,
                                                               {},
                                                               parsed.actionId,
                                                               parsed.actionId.isEmpty()
                                                                   ? QStringLiteral("NotRequested")
                                                                   : QStringLiteral("PendingRuntimeDispatch"),
                                                               index);
            draft.actionParameters = parsed.actionParameters;
            const bool saved = guard->completeResponderSlotAtomically(roomId, index, draft, &petMessage);
            if (!saved) {
                guard->finishTurn(roomId);
                return;
            }
            emit guard->roomPetReply(roomId, participant.participantId, participant.petProjectPath, petMessage.content,
                                     parsed.actionId, parsed.actionParameters, petMessage.messageId, roomName, latestUserMessage);
        } else {
            const QString failure = QStringLiteral("%1 request failed [%2]: %3")
                .arg(participant.characterName, result.errorCode, result.message);
            const ConversationMessage failed = makeDraftMessage(participant.participantId,
                                                                participant.characterName,
                                                                ConversationSenderType::System,
                                                                failure,
                                                                statusFromRequestResult(result),
                                                                result.errorCode, {}, {}, index);
            if (!guard->completeResponderSlotAtomically(roomId, index, failed)) {
                guard->finishTurn(roomId);
                return;
            }
            emit guard->roomError(roomId, failure);
        }

        guard->advanceRoundTableAfterPlannedTurn(roomId, responders, index);
        guard->runResponderSequence(roomId, responders, index + 1, latestUserMessage, turnGeneration);
    });
    trackRequest(roomId, participant.participantId, handle, lease);
}

void AIConversationRoomManager::runResponderParallel(const QString &roomId,
                                                      const QVector<AIConversationParticipant> &responders,
                                                      const QString &latestUserMessage,
                                                      quint64 turnGeneration)
{
    if (m_shuttingDown) {
        return;
    }
    AIConversationRoom *targetRoom = room(roomId);
    if (!targetRoom || targetRoom->turnGeneration != turnGeneration || responders.isEmpty()) {
        finishTurn(roomId);
        return;
    }
    const QString roomName = targetRoom->roomName;

    struct PreparedRequest {
        int slot {-1};
        AIConversationParticipant participant;
        AIProviderProfile profile;
        AIChatRequest request;
        QSharedPointer<PetAiRequestLease> lease;
    };
    struct ParallelTurnState {
        int expected {0};
        int completed {0};
        bool committed {false};
        QVector<ConversationMessage> messages;
        QVector<QString> replies;
        QVector<QString> actionIds;
        QVector<AIConversationParticipant> participants;
        QVector<QString> pendingMessageIds;
    };

    auto state = QSharedPointer<ParallelTurnState>::create();
    state->expected = responders.size();
    state->messages.resize(responders.size());
    state->replies.resize(responders.size());
    state->actionIds.resize(responders.size());
    state->participants = responders;
    state->pendingMessageIds.resize(responders.size());
    for (const ConversationMessage &message : targetRoom->history) {
        if (message.turnId == targetRoom->activeTurnId
            && message.status == ConversationMessageStatus::Pending
            && message.responderIndex >= 0
            && message.responderIndex < state->pendingMessageIds.size()) {
            state->pendingMessageIds[message.responderIndex] = message.messageId;
        }
    }
    QVector<PreparedRequest> prepared;

    // Prepare every slot before dispatch so synchronous provider callbacks cannot finish a partial turn.
    for (int i = 0; i < responders.size(); ++i) {
        AIConversationParticipant participant = responders.at(i);
        QString refreshError;
        if (!refreshParticipant(&participant, &refreshError)) {
            state->messages[i] = makeDraftMessage(participant.participantId, participant.characterName,
                                                  ConversationSenderType::System,
                                                  QStringLiteral("%1 \u914d\u7f6e\u91cd\u8f7d\u5931\u8d25: %2").arg(participant.characterName, refreshError),
                                                  ConversationMessageStatus::Failed,
                                                  QStringLiteral("ParticipantRefreshFailed"), {}, {}, i);
            ++state->completed;
            continue;
        }
        state->participants[i] = participant;
        const ProviderLookupResult profileResult = findProfileStrict(participant.providerProfileId);
        if (!profileResult.ok && !m_participantResolver) {
            state->messages[i] = makeDraftMessage(participant.participantId, participant.characterName,
                                                  ConversationSenderType::System,
                                                  QStringLiteral("%1 AI \u670d\u52a1\u672a\u914d\u7f6e\uff1a%2").arg(participant.characterName, profileResult.message),
                                                  ConversationMessageStatus::Failed,
                                                  QStringLiteral("ProviderNotConfigured"), {}, {}, i);
            ++state->completed;
            continue;
        }
        QString leaseError;
        const QString leaseProjectId = participant.projectId.trimmed().isEmpty()
            ? QStringLiteral("room:%1:%2").arg(roomId, participant.participantId)
            : participant.projectId;
        const auto lease = PetAiRequestCoordinator::instance().acquire(leaseProjectId,
                                                                         participant.petProjectPath,
                                                                         PetAiRequestSource::MultiAi,
                                                                         roomId,
                                                                         &leaseError);
        if (!lease) {
            state->messages[i] = makeDraftMessage(participant.participantId, participant.characterName,
                                                  ConversationSenderType::System,
                                                  QStringLiteral("%1 AI \u8bf7\u6c42\u672a\u53d1\u9001\uff1a%2").arg(participant.characterName, leaseError),
                                                  ConversationMessageStatus::Failed,
                                                  QStringLiteral("LeaseUnavailable"), {}, {}, i);
            ++state->completed;
            continue;
        }
        AIChatRequest request;
        request.characterName = participant.characterName;
        const GroupConversationContext context = buildGroupContext(*targetRoom, participant, latestUserMessage, responders);
        request.systemPrompt = participant.systemPrompt + QStringLiteral("\n\n")
            + groupContextText(context);
        request.history = context.recentHistory;
        request.userMessage = context.currentUserMessage;
        request.allowedActionIds = participant.allowedActionIds;
        request.allowedActionDescriptors = participant.allowedActionDescriptors;
        prepared.append({i, participant, profileResult.profile, request, lease});
    }

    QPointer<AIConversationRoomManager> guard(this);
    auto commit = QSharedPointer<std::function<void()>>::create();
    *commit = [guard, roomId, roomName, latestUserMessage, turnGeneration, state]() {
        if (!guard || guard->m_shuttingDown || state->committed || state->completed != state->expected) {
            return;
        }
        AIConversationRoom *callbackRoom = guard->room(roomId);
        if (!callbackRoom || callbackRoom->turnGeneration != turnGeneration) {
            return;
        }
        if (!guard->mutateRoomAtomically(roomId, [state](AIConversationRoom &nextRoom) {
                for (int slot = 0; slot < state->messages.size(); ++slot) {
                    ConversationMessage &completed = state->messages[slot];
                    if (completed.messageId.isEmpty()) {
                        continue;
                    }
                    for (ConversationMessage &pending : nextRoom.history) {
                        if (pending.messageId != state->pendingMessageIds.value(slot)
                            || pending.status != ConversationMessageStatus::Pending) {
                            continue;
                        }
                        completed.messageId = pending.messageId;
                        completed.sequence = pending.sequence;
                        completed.roomId = pending.roomId;
                        completed.turnId = pending.turnId;
                        pending = completed;
                        if (pending.senderType == ConversationSenderType::Pet) {
                            markParticipantSpoke(&nextRoom, pending.senderId);
                        }
                        break;
                    }
                }
                return true;
            })) {
            guard->finishTurn(roomId);
            return;
        }
        state->committed = true;
        for (int i = 0; i < state->messages.size(); ++i) {
            const ConversationMessage &message = state->messages.at(i);
            if (message.senderType == ConversationSenderType::Pet) {
                const AIConversationParticipant participant = state->participants.value(i);
                emit guard->roomPetReply(roomId, participant.participantId, participant.petProjectPath,
                                         state->replies.value(i), message.actionId, message.actionParameters, message.messageId,
                                         roomName, latestUserMessage);
            }
        }
        guard->finishTurn(roomId);
    };

    for (const PreparedRequest &item : prepared) {
        AIRequestHandle *handle = m_chatClient->sendMessage(item.profile, item.request,
            [guard, roomId, turnGeneration, item, state, commit](const AIRequestResult &result) {
                if (!guard || guard->m_shuttingDown) {
                    return;
                }
                AIConversationRoom *callbackRoom = guard->room(roomId);
                if (!callbackRoom || callbackRoom->turnGeneration != turnGeneration || state->committed) {
                    if (!callbackRoom && guard->m_deletingRooms.contains(roomId)) {
                        return;
                    }
                    return;
                }
                if (!containsParticipant(*callbackRoom, item.participant.participantId)) {
                    ++state->completed;
                    (*commit)();
                    return;
                }
                if (result.success) {
                    const AIChatReply parsed = AIProvider::parseStructuredReply(result.message, item.participant.allowedActionIds);
                    const QString content = parsed.reply.trimmed().isEmpty() ? result.message.trimmed() : parsed.reply.trimmed();
                    state->messages[item.slot] = makeDraftMessage(item.participant.participantId,
                                                                   item.participant.characterName,
                                                                   ConversationSenderType::Pet,
                                                                   content,
                                                                   ConversationMessageStatus::Completed,
                                                                   {},
                                                                   parsed.actionId,
                                                                   parsed.actionId.isEmpty()
                                                                       ? QStringLiteral("NotRequested")
                                                                       : QStringLiteral("PendingRuntimeDispatch"),
                                                                   item.slot);
                    state->messages[item.slot].actionParameters = parsed.actionParameters;
                    state->replies[item.slot] = content;
                    state->actionIds[item.slot] = parsed.actionId;
                } else {
                    state->messages[item.slot] = makeDraftMessage(item.participant.participantId,
                                                                   item.participant.characterName,
                                                                   ConversationSenderType::System,
                                                                   QStringLiteral("%1 request failed [%2]: %3")
                                                                       .arg(item.participant.characterName, result.errorCode, result.message),
                                                                   statusFromRequestResult(result),
                                                                   result.errorCode,
                                                                   {}, {}, item.slot);
                }
                state->messages[item.slot].messageId = state->pendingMessageIds.value(item.slot);
                if (!guard->mutateRoomAtomically(roomId, [state, item](AIConversationRoom &nextRoom) {
                        ConversationMessage completed = state->messages.value(item.slot);
                        for (ConversationMessage &pending : nextRoom.history) {
                            if (pending.messageId != completed.messageId
                                || pending.status != ConversationMessageStatus::Pending) {
                                continue;
                            }
                            completed.sequence = pending.sequence;
                            completed.roomId = pending.roomId;
                            completed.turnId = pending.turnId;
                            pending = completed;
                            state->messages[item.slot] = completed;
                            if (completed.senderType == ConversationSenderType::Pet) {
                                markParticipantSpoke(&nextRoom, completed.senderId);
                            }
                            return true;
                        }
                        return false;
                    })) {
                    guard->finishTurn(roomId);
                    return;
                }
                ++state->completed;
                (*commit)();
            });
        trackRequest(roomId, item.participant.participantId, handle, item.lease);
    }
    (*commit)();
}

void AIConversationRoomManager::finishTurn(const QString &roomId)
{
    if (m_shuttingDown) {
        return;
    }
    if (m_deletingRooms.contains(roomId)) {
        m_busyRooms.remove(roomId);
        m_activeTurnRequests.remove(roomId);
        return;
    }
    if (!m_busyRooms.contains(roomId)) {
        return;
    }
    if (!mutateRoomAtomically(roomId, [](AIConversationRoom &nextRoom) {
            nextRoom.activeTurnId.clear();
            return true;
        })) {
        return;
    }
    m_busyRooms.remove(roomId);
    m_activeTurnRequests.remove(roomId);
    emit roomUpdated(roomId);
    emit turnFinished(roomId);
}

QVector<AIConversationRoom> AIConversationRoomManager::roomsForState(
    const QHash<QString, AIConversationRoom> &rooms,
    const QVector<QString> &order) const
{
    QVector<AIConversationRoom> result;
    result.reserve(order.size());
    for (const QString &roomId : order) {
        const auto it = rooms.constFind(roomId);
        if (it != rooms.constEnd()) {
            result.append(it.value());
        }
    }
    return result;
}

bool AIConversationRoomManager::commitRoomState(const QHash<QString, AIConversationRoom> &rooms,
                                                const QVector<QString> &order,
                                                const QString &roomId)
{
    if (m_shuttingDown) {
        return false;
    }
    auto normalizedRooms = rooms;
    for (auto it = normalizedRooms.begin(); it != normalizedRooms.end(); ++it) {
        normalizeRoomReferences(it.value());
    }
    QString error;
    if (!m_repository.save(roomsForState(normalizedRooms, order), &error)) {
        setMutationFailure(RoomMutationError::PersistenceFailed,
                           roomId,
                           QStringLiteral("Unable to save AI conversation rooms: %1").arg(error));
        return false;
    }
    m_rooms = normalizedRooms;
    m_roomOrder = order;
    setMutationSuccess();
    if (!roomId.isEmpty()) {
        emit roomUpdated(roomId);
    }
    return true;
}

void AIConversationRoomManager::setMutationFailure(RoomMutationError error,
                                                    const QString &roomId,
                                                    const QString &message)
{
    m_lastMutationResult = {false, error, message};
    emit roomError(roomId, message);
}

void AIConversationRoomManager::setMutationSuccess()
{
    m_lastMutationResult = {true, RoomMutationError::None, {}};
}

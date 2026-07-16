#ifndef AICONVERSATIONROOMMANAGER_H
#define AICONVERSATIONROOMMANAGER_H

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <functional>

#include "aiconversationroom.h"
#include "aiconversationroomrepository.h"
#include "aiprovider.h"
#include "aiproviderprofileregistry.h"
#include "petairequestcoordinator.h"
#include "runtimeactionresult.h"

class IAIConversationChatClient
{
public:
    virtual ~IAIConversationChatClient() = default;
    virtual AIRequestHandle *sendMessage(const AIProviderProfile &profile,
                                         const AIChatRequest &request,
                                         AIProvider::ResultCallback callback) = 0;
};

// Production rooms resolve a participant from its pet.json. Tests can inject a
// resolver without weakening the persisted project-path invariant.
class IAIConversationParticipantResolver
{
public:
    virtual ~IAIConversationParticipantResolver() = default;
    virtual bool resolve(AIConversationParticipant *participant, QString *errorMessage) const = 0;
};

class AIProviderConversationChatClient : public IAIConversationChatClient
{
public:
    AIRequestHandle *sendMessage(const AIProviderProfile &profile,
                                 const AIChatRequest &request,
                                 AIProvider::ResultCallback callback) override;
};

enum class RoomMutationError
{
    None,
    InvalidInput,
    NotFound,
    Busy,
    PersistenceFailed,
    LoadFailed
};

struct RoomMutationResult
{
    bool ok {false};
    RoomMutationError error {RoomMutationError::None};
    QString message;
};

class AIConversationRoomManager : public QObject
{
    Q_OBJECT

public:
    explicit AIConversationRoomManager(QObject *parent = nullptr);
    explicit AIConversationRoomManager(IAIConversationChatClient *chatClient,
                                       int defaultMaxResponders,
                                       QObject *parent = nullptr);
    AIConversationRoomManager(IAIConversationChatClient *chatClient,
                              int defaultMaxResponders,
                              const QString &repositoryPath,
                              QObject *parent = nullptr);
    ~AIConversationRoomManager() override;
    QString createRoom(const QString &name);
    bool deleteRoom(const QString &roomId);
    bool renameRoom(const QString &roomId, const QString &name);
    bool setRoomMode(const QString &roomId, AIConversationMode mode);
    bool setRoomConfiguration(const QString &roomId,
                              int minResponders,
                              int maxResponders,
                              const QString &directedTargetParticipantId,
                              bool persistHistory,
                              int historyMaxMessages);
    bool setRoomSettings(const QString &roomId,
                         AIConversationMode mode,
                         int minResponders,
                         int maxResponders,
                         const QString &directedTargetParticipantId,
                         bool persistHistory,
                         int historyMaxMessages);
    QVector<AIConversationRoom> rooms() const;
    AIConversationRoom *room(const QString &roomId);
    bool addParticipantFromProject(const QString &roomId, const QString &petJsonPath, QString *errorMessage = nullptr);
    bool removeParticipant(const QString &roomId, const QString &participantId);
    bool setParticipantEnabled(const QString &roomId, const QString &participantId, bool enabled);
    bool setParticipantWeight(const QString &roomId, const QString &participantId, double weight);
    bool setParticipantRelationship(const QString &roomId,
                                    const ParticipantRelationship &relationship);
    bool removeParticipantRelationship(const QString &roomId,
                                       const QString &fromParticipantId,
                                       const QString &toParticipantId);
    bool clearHistory(const QString &roomId);
    bool submitUserMessage(const QString &roomId, const QString &message, const QStringList &targetParticipantIds = {});
    bool cancelTurn(const QString &roomId);
    bool recordRuntimeDispatchFailure(const QString &roomId, const QString &messageId);
    bool recordRuntimeDispatchDelivered(const QString &roomId, const QString &messageId);
    bool recordRuntimeDeliveryResult(const QString &roomId,
                                     const QString &messageId,
                                     const RuntimeReplyDeliveryResult &result);
    bool recordRuntimeActionResult(const QString &roomId,
                                   const QString &messageId,
                                   const RuntimeActionDispatchResult &result);
    bool isRoomBusy(const QString &roomId) const;
    bool hasLoadFailure() const { return m_loadFailure; }
    bool isFutureSchemaProtected() const
    {
        return m_loadStatus == RoomRepositoryLoadStatus::UnsupportedFutureVersion;
    }
    QString loadFailureMessage() const { return m_loadFailureMessage; }
    QString corruptBackupPath() const { return m_corruptBackupPath; }
    RoomMutationResult lastMutationResult() const { return m_lastMutationResult; }
    bool recoverFromLoadFailure();
    void setParticipantResolverForTesting(const IAIConversationParticipantResolver *resolver)
    {
        m_participantResolver = resolver;
    }
    static QVector<AIConversationParticipant> selectRespondersForRoom(const AIConversationRoom &room,
                                                                      const QStringList &targetParticipantIds = {});
    static QVector<AIConversationParticipant> selectRespondersForRoom(const AIConversationRoom &room,
                                                                      const QStringList &targetParticipantIds,
                                                                      std::function<double(double)> randomBelow);
    static QString transcriptForRoom(const AIConversationRoom &room);

signals:
    void roomUpdated(const QString &roomId);
    void turnStarted(const QString &roomId);
    void turnFinished(const QString &roomId);
    void roomError(const QString &roomId, const QString &message);
    void roomPetReply(const QString &roomId,
                      const QString &participantId,
                      const QString &petProjectPath,
                      const QString &reply,
                      const QString &actionId,
                      const QJsonObject &actionParameters,
                      const QString &messageId,
                      const QString &roomName,
                      const QString &userMessage);

private:
    static QString canonicalProjectPath(const QString &path);
    bool restoreInterruptedTurns(QVector<AIConversationRoom> *rooms, QString *errorMessage);
    void refreshLoadedParticipants(QVector<AIConversationRoom> *rooms) const;
    bool refreshParticipant(AIConversationParticipant *participant, QString *errorMessage = nullptr) const;
    bool mutateRoomAtomically(const QString &roomId,
                              const std::function<bool(AIConversationRoom &)> &mutation);
    bool appendMessageAtomically(const QString &roomId,
                                 const QString &senderId,
                                 const QString &senderName,
                                 ConversationSenderType senderType,
                                 const QString &content,
                                 ConversationMessageStatus status,
                                 const QString &errorCode = {},
                                 const QString &actionId = {},
                                 const QString &actionStatus = {},
                                 int responderIndex = -1,
                                 ConversationMessage *committedMessage = nullptr);
    bool completeResponderSlotAtomically(const QString &roomId,
                                         int responderIndex,
                                         const ConversationMessage &completed,
                                         ConversationMessage *committedMessage = nullptr);
    static ConversationMessageStatus statusFromRequestResult(const AIRequestResult &result);
    void runResponderSequence(const QString &roomId,
                              const QVector<AIConversationParticipant> &responders,
                              int index,
                              const QString &latestUserMessage,
                              quint64 turnGeneration);
    void runResponderParallel(const QString &roomId,
                              const QVector<AIConversationParticipant> &responders,
                              const QString &latestUserMessage,
                              quint64 turnGeneration);
    void advanceRoundTableAfterSuccessfulTurn(AIConversationRoom *room,
                                              const QVector<AIConversationParticipant> &responders);
    void advanceRoundTableAfterPlannedTurn(const QString &roomId,
                                           const QVector<AIConversationParticipant> &responders,
                                           int completedSlot);
    void finishTurn(const QString &roomId);
    void trackRequest(const QString &roomId,
                      const QString &participantId,
                      AIRequestHandle *handle,
                      const QSharedPointer<PetAiRequestLease> &lease);
    void cancelRequestsForRoom(const QString &roomId, const QString &reason);
    void cancelRequestsForParticipant(const QString &roomId,
                                      const QString &participantId,
                                      const QString &reason);
    QVector<AIConversationRoom> roomsForState(const QHash<QString, AIConversationRoom> &rooms,
                                              const QVector<QString> &order) const;
    bool commitRoomState(const QHash<QString, AIConversationRoom> &rooms,
                         const QVector<QString> &order,
                         const QString &roomId = {});
    void setMutationFailure(RoomMutationError error, const QString &roomId, const QString &message);
    void setMutationSuccess();

    struct ActiveTurnRequest {
        QString participantId;
        QPointer<AIRequestHandle> handle;
        QSharedPointer<PetAiRequestLease> lease;
    };

    QHash<QString, AIConversationRoom> m_rooms;
    QVector<QString> m_roomOrder;
    QSet<QString> m_busyRooms;
    QHash<QString, QVector<ActiveTurnRequest>> m_activeTurnRequests;
    QSet<QString> m_deletingRooms;
    IAIConversationChatClient *m_chatClient {nullptr};
    AIProviderConversationChatClient m_defaultChatClient;
    int m_defaultMaxResponders {2};
    AIConversationRoomRepository m_repository;
    bool m_loadFailure {false};
    QString m_loadFailureMessage;
    QString m_corruptBackupPath;
    RoomMutationResult m_lastMutationResult;
    RoomRepositoryLoadStatus m_loadStatus {RoomRepositoryLoadStatus::Ok};
    bool m_shuttingDown {false};
    const IAIConversationParticipantResolver *m_participantResolver {nullptr};
};

#endif // AICONVERSATIONROOMMANAGER_H

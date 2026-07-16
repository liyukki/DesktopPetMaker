#ifndef AICONVERSATIONROOM_H
#define AICONVERSATIONROOM_H

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include "aiactiondescriptor.h"

enum class ConversationSenderType
{
    User,
    Pet,
    System,
    InternalControl
};

enum class AIConversationMode
{
    Directed,
    FreeGroup,
    RoundTable
};

enum class ConversationMessageStatus
{
    Pending,
    Completed,
    Failed,
    Cancelled,
    TimedOut,
    InvalidAction,
    RuntimeDispatchFailed,
    Interrupted
};

enum class DesktopDeliveryStatus
{
    NotRequested,
    Pending,
    Delivered,
    PetNotRunning,
    Failed
};

enum class ActionExecutionStatus
{
    NotRequested,
    Pending,
    Executed,
    Queued,
    Rejected,
    Failed
};

struct ConversationMessage
{
    QString messageId;
    int sequence {0};
    QString senderId;
    QString senderName;
    QString roomId;
    QString turnId;
    int responderIndex {-1};
    ConversationSenderType senderType {ConversationSenderType::User};
    ConversationMessageStatus status {ConversationMessageStatus::Completed};
    QString content;
    QString errorCode;
    QString actionId;
    QJsonObject actionParameters;
    DesktopDeliveryStatus desktopDeliveryStatus {DesktopDeliveryStatus::NotRequested};
    ActionExecutionStatus actionExecutionStatus {ActionExecutionStatus::NotRequested};
    // Schema 1/2 compatibility only. New code uses the typed delivery/action fields.
    QString actionStatus;
    QDateTime timestamp {QDateTime::currentDateTime()};
};

struct ParticipantRelationship
{
    QString fromParticipantId;
    QString toParticipantId;
    QString preferredAddress;
    QString description;
};

struct AIConversationParticipant
{
    QString participantId;
    QString petProjectPath;
    QString projectId;
    QString characterName;
    QString systemPrompt;
    QString providerProfileId;
    QStringList allowedActionIds;
    QVector<AIActionDescriptor> allowedActionDescriptors;
    bool enabled {true};
    QString validationError;
    double speakingWeight {1.0};
    quint64 lastSpokeTurn {0};
};

struct AIConversationRoom
{
    QString roomId;
    QString roomName;
    QVector<AIConversationParticipant> participants;
    QVector<ConversationMessage> history;
    AIConversationMode mode {AIConversationMode::Directed};
    int minRespondersPerTurn {1};
    int maxRespondersPerTurn {2};
    QString directedTargetParticipantId;
    bool persistHistory {true};
    int historyMaxMessages {200};
    QString roundTableNextParticipantId;
    // Legacy schema-1 fallback only. New saves use the stable participant id above.
    int roundTableCursor {0};
    quint64 turnGeneration {0};
    qint64 nextMessageSequence {1};
    QString activeTurnId;
    QVector<ParticipantRelationship> relationships;
};

void normalizeRoomReferences(AIConversationRoom &room);
bool validateRoomForPersistence(const AIConversationRoom &room, QString *errorMessage = nullptr);

#endif // AICONVERSATIONROOM_H

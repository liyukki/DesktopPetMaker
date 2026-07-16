#include "aiconversationroom.h"

#include <QSet>

#include <algorithm>

void normalizeRoomReferences(AIConversationRoom &room)
{
    QStringList enabledIds;
    for (const AIConversationParticipant &participant : room.participants) {
        if (participant.enabled) {
            enabledIds.append(participant.participantId);
        }
    }

    if (!room.directedTargetParticipantId.isEmpty()
        && !enabledIds.contains(room.directedTargetParticipantId)) {
        room.directedTargetParticipantId.clear();
    }

    if (enabledIds.isEmpty()) {
        room.roundTableNextParticipantId.clear();
        room.roundTableCursor = 0;
        room.minRespondersPerTurn = 1;
        room.maxRespondersPerTurn = 1;
    } else {
        if (!enabledIds.contains(room.roundTableNextParticipantId)) {
            room.roundTableNextParticipantId = enabledIds.first();
        }
        room.roundTableCursor = enabledIds.indexOf(room.roundTableNextParticipantId);
        room.minRespondersPerTurn = qBound(1, room.minRespondersPerTurn, enabledIds.size());
        room.maxRespondersPerTurn = qBound(room.minRespondersPerTurn,
                                           room.maxRespondersPerTurn,
                                           enabledIds.size());
    }

    QSet<QString> participantIds;
    for (const AIConversationParticipant &participant : room.participants) {
        participantIds.insert(participant.participantId);
    }
    room.relationships.erase(
        std::remove_if(room.relationships.begin(), room.relationships.end(),
                       [&participantIds](const ParticipantRelationship &relationship) {
                           return relationship.fromParticipantId == relationship.toParticipantId
                               || !participantIds.contains(relationship.fromParticipantId)
                               || !participantIds.contains(relationship.toParticipantId);
                       }),
        room.relationships.end());
}

bool validateRoomForPersistence(const AIConversationRoom &room, QString *errorMessage)
{
    QSet<QString> participantIds;
    QSet<QString> enabledIds;
    for (const AIConversationParticipant &participant : room.participants) {
        if (participant.participantId.trimmed().isEmpty()
            || participantIds.contains(participant.participantId)
            || participant.speakingWeight < 0.0
            || participant.speakingWeight > 100.0) {
            if (errorMessage) *errorMessage = QStringLiteral("Invalid or duplicate participant data.");
            return false;
        }
        participantIds.insert(participant.participantId);
        if (participant.enabled) enabledIds.insert(participant.participantId);
    }
    if (!room.directedTargetParticipantId.isEmpty()
        && !enabledIds.contains(room.directedTargetParticipantId)) {
        if (errorMessage) *errorMessage = QStringLiteral("Directed target is not an enabled participant.");
        return false;
    }
    if (!room.roundTableNextParticipantId.isEmpty()
        && !enabledIds.contains(room.roundTableNextParticipantId)) {
        if (errorMessage) *errorMessage = QStringLiteral("Round-table next participant is not enabled.");
        return false;
    }
    for (const ParticipantRelationship &relationship : room.relationships) {
        if (relationship.fromParticipantId == relationship.toParticipantId
            || !participantIds.contains(relationship.fromParticipantId)
            || !participantIds.contains(relationship.toParticipantId)) {
            if (errorMessage) *errorMessage = QStringLiteral("Participant relationship contains an invalid reference.");
            return false;
        }
    }
    if (room.historyMaxMessages < 1 || room.historyMaxMessages > 2000) {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid room history limit.");
        return false;
    }
    return true;
}

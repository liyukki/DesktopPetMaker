#include <QtTest>

#include "aiconversationroommanager.h"
#include "aiconversationroomrepository.h"

namespace {
AIConversationRoom roomFixture()
{
    AIConversationRoom room;
    room.roomId = QStringLiteral("room-1");
    room.roomName = QStringLiteral("Room");
    room.minRespondersPerTurn = 1;
    room.maxRespondersPerTurn = 2;
    for (int i = 0; i < 3; ++i) {
        AIConversationParticipant participant;
        participant.participantId = QStringLiteral("p%1").arg(i);
        participant.projectId = QStringLiteral("project-%1").arg(i);
        participant.petProjectPath = QStringLiteral("pet-%1/pet.json").arg(i);
        participant.characterName = QStringLiteral("Pet%1").arg(i);
        participant.speakingWeight = 3 - i;
        room.participants.append(participant);
    }
    return room;
}
}

class MultiAiRoomTests : public QObject
{
    Q_OBJECT
private slots:
    void responderModesAndTranscript()
    {
        AIConversationRoom room = roomFixture();
        room.mode = AIConversationMode::Directed;
        auto selected = AIConversationRoomManager::selectRespondersForRoom(room, {QStringLiteral("p1")});
        QCOMPARE(selected.size(), 1);
        QCOMPARE(selected.first().participantId, QStringLiteral("p1"));
        QVERIFY(AIConversationRoomManager::selectRespondersForRoom(room, {QStringLiteral("missing")}).isEmpty());

        room.mode = AIConversationMode::FreeGroup;
        room.minRespondersPerTurn = 2;
        selected = AIConversationRoomManager::selectRespondersForRoom(room, {}, [](double) { return 0.0; });
        QCOMPARE(selected.size(), 2);
        QVERIFY(selected.at(0).participantId != selected.at(1).participantId);
        room.participants[0].speakingWeight = 0;
        room.participants[1].speakingWeight = 0;
        room.minRespondersPerTurn = 1;
        selected = AIConversationRoomManager::selectRespondersForRoom(room, {}, [](double) { return 0.0; });
        QCOMPARE(selected.size(), 1);
        QCOMPARE(selected.first().participantId, QStringLiteral("p2"));

        room = roomFixture();
        room.mode = AIConversationMode::RoundTable;
        room.minRespondersPerTurn = room.maxRespondersPerTurn = 1;
        room.roundTableNextParticipantId = QStringLiteral("p2");
        QCOMPARE(AIConversationRoomManager::selectRespondersForRoom(room).first().participantId, QStringLiteral("p2"));
        room.participants[2].enabled = false;
        QCOMPARE(AIConversationRoomManager::selectRespondersForRoom(room).first().participantId, QStringLiteral("p0"));

        ConversationMessage user;
        user.senderName = QStringLiteral("User");
        user.senderType = ConversationSenderType::User;
        user.content = QStringLiteral("hello");
        ConversationMessage control = user;
        control.senderType = ConversationSenderType::InternalControl;
        control.content = QStringLiteral("hidden control");
        room.history = {user, control};
        const QString transcript = AIConversationRoomManager::transcriptForRoom(room);
        QVERIFY(transcript.contains(QStringLiteral("User: hello")));
        QVERIFY(!transcript.contains(QStringLiteral("hidden control")));
    }

    void referencesRelationshipsStatusesAndRepositoryRoundTrip()
    {
        AIConversationRoom room = roomFixture();
        room.mode = AIConversationMode::Directed;
        room.directedTargetParticipantId = QStringLiteral("missing");
        room.roundTableNextParticipantId = QStringLiteral("missing");
        room.relationships.append({QStringLiteral("p0"), QStringLiteral("p1"), QStringLiteral("friend"), QStringLiteral("trusted")});
        ConversationMessage message;
        message.messageId = QStringLiteral("m1");
        message.sequence = 1;
        message.roomId = room.roomId;
        message.turnId = QStringLiteral("turn-1");
        message.senderId = QStringLiteral("p0");
        message.senderName = QStringLiteral("Pet0");
        message.content = QStringLiteral("hello");
        message.senderType = ConversationSenderType::Pet;
        message.status = ConversationMessageStatus::Completed;
        message.desktopDeliveryStatus = DesktopDeliveryStatus::Delivered;
        message.actionExecutionStatus = ActionExecutionStatus::Executed;
        message.actionId = QStringLiteral("wave");
        room.history.append(message);
        room.nextMessageSequence = 2;
        normalizeRoomReferences(room);
        QVERIFY(room.directedTargetParticipantId.isEmpty());
        QCOMPARE(room.roundTableNextParticipantId, QStringLiteral("p0"));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AIConversationRoomRepository repository(dir.filePath(QStringLiteral("rooms.json")));
        QString error;
        QVERIFY2(repository.save({room}, &error), qPrintable(error));
        QVector<AIConversationRoom> loaded;
        QVERIFY2(repository.load(&loaded, &error), qPrintable(error));
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().relationships.size(), 1);
        QCOMPARE(loaded.first().history.first().desktopDeliveryStatus, DesktopDeliveryStatus::Delivered);
        QCOMPARE(loaded.first().history.first().actionExecutionStatus, ActionExecutionStatus::Executed);
        QCOMPARE(loaded.first().history.first().actionId, QStringLiteral("wave"));
    }
};

QTEST_MAIN(MultiAiRoomTests)
#include "multi_ai_room_tests.moc"

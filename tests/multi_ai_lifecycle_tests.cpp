#include <QtTest>

#include "aiconversationroommanager.h"
#include "petairequestcoordinator.h"

class DelayedClient final : public IAIConversationChatClient
{
public:
    AIRequestHandle *sendMessage(const AIProviderProfile &, const AIChatRequest &,
                                 AIProvider::ResultCallback callback) override
    {
        callbacks.append(std::move(callback));
        return nullptr;
    }
    QVector<AIProvider::ResultCallback> callbacks;
};

class CancellableClient final : public IAIConversationChatClient
{
public:
    AIRequestHandle *sendMessage(const AIProviderProfile &, const AIChatRequest &,
                                 AIProvider::ResultCallback callback) override
    {
        auto *handle = new AIRequestHandle();
        QObject::connect(handle, &AIRequestHandle::finished, handle,
                         [callback](const AIRequestResult &result) { callback(result); });
        QObject::connect(handle, &AIRequestHandle::finished, handle, &QObject::deleteLater);
        lastHandle = handle;
        return handle;
    }
    QPointer<AIRequestHandle> lastHandle;
};

class AcceptingResolver final : public IAIConversationParticipantResolver
{
public:
    bool resolve(AIConversationParticipant *participant, QString *errorMessage) const override
    {
        Q_UNUSED(errorMessage)
        return participant && !participant->participantId.isEmpty();
    }
};

namespace {
QString addParticipant(AIConversationRoomManager &manager, const QString &roomId, const QString &id)
{
    AIConversationRoom *room = manager.room(roomId);
    AIConversationParticipant participant;
    participant.participantId = id;
    participant.projectId = QStringLiteral("project-%1").arg(id);
    participant.characterName = id;
    participant.providerProfileId = QStringLiteral("default");
    room->participants.append(participant);
    room->directedTargetParticipantId = id;
    return id;
}
}

class MultiAiLifecycleTests : public QObject
{
    Q_OBJECT
private slots:
    void leaseBlocksOverlapReleasesAndTimesOut()
    {
        PetAiRequestCoordinator &coordinator = PetAiRequestCoordinator::instance();
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QString error;
        auto first = coordinator.acquire(id, QString(), PetAiRequestSource::MultiAi, {}, &error, 5000);
        QVERIFY(first);
        QVERIFY(!coordinator.acquire(id, QString(), PetAiRequestSource::SingleChat, {}, &error, 5000));
        const QString firstRequestId = first->requestId();
        first.reset();
        auto second = coordinator.acquire(id, QString(), PetAiRequestSource::SingleChat, {}, &error, 1000);
        QVERIFY(second);
        QVERIFY(second->requestId() != firstRequestId);
        AIRequestHandle timeoutHandle;
        second->bindHandle(&timeoutHandle);
        QTRY_VERIFY_WITH_TIMEOUT(timeoutHandle.isCancelled(), 2000);
        second.reset();
        QVERIFY(!coordinator.isBusy(id, QString()));
        auto reused = coordinator.acquire(id, QString(), PetAiRequestSource::MultiAi, {}, &error, 1000);
        QVERIFY(reused);
        reused.reset();
    }

    void cancelAndRemoveDoNotLeaveBusyRoom()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CancellableClient client;
        AIConversationRoomManager manager(&client, 1, dir.filePath(QStringLiteral("rooms.json")));
        AcceptingResolver resolver;
        manager.setParticipantResolverForTesting(&resolver);
        const QString roomId = manager.createRoom(QStringLiteral("Lifecycle"));
        const QString participantId = addParticipant(manager, roomId, QStringLiteral("pet-a"));
        QVERIFY(manager.submitUserMessage(roomId, QStringLiteral("hello"), {participantId}));
        QVERIFY(manager.isRoomBusy(roomId));
        QVERIFY(client.lastHandle);
        QVERIFY(manager.cancelTurn(roomId));
        QTRY_VERIFY(!manager.isRoomBusy(roomId));
        QVERIFY(!client.lastHandle || client.lastHandle->isCancelled());

        QVERIFY(manager.submitUserMessage(roomId, QStringLiteral("again"), {participantId}));
        QVERIFY(manager.removeParticipant(roomId, participantId));
        QTRY_VERIFY(!manager.isRoomBusy(roomId));
        QVERIFY(manager.room(roomId)->participants.isEmpty());
    }

    void lateCallbackAfterManagerDestructionIsIgnored()
    {
        DelayedClient client;
        auto *manager = new AIConversationRoomManager(&client, 1);
        AcceptingResolver resolver;
        manager->setParticipantResolverForTesting(&resolver);
        const QString roomId = manager->createRoom(QStringLiteral("Delayed"));
        const QString participantId = addParticipant(*manager, roomId, QStringLiteral("pet-late"));
        QVERIFY(manager->submitUserMessage(roomId, QStringLiteral("hello"), {participantId}));
        QCOMPARE(client.callbacks.size(), 1);
        delete manager;
        client.callbacks.first()({true, QStringLiteral("late reply"), QString(), false});
        QVERIFY(true);
    }

    void deleteBusyRoomCancelsOwnership()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        CancellableClient client;
        AIConversationRoomManager manager(&client, 1, dir.filePath(QStringLiteral("rooms.json")));
        AcceptingResolver resolver;
        manager.setParticipantResolverForTesting(&resolver);
        const QString roomId = manager.createRoom(QStringLiteral("Delete"));
        const QString participantId = addParticipant(manager, roomId, QStringLiteral("pet-delete"));
        QVERIFY(manager.submitUserMessage(roomId, QStringLiteral("hello"), {participantId}));
        QVERIFY(manager.deleteRoom(roomId));
        QVERIFY(manager.room(roomId) == nullptr);
        QVERIFY(!client.lastHandle || client.lastHandle->isCancelled());
    }
};

QTEST_MAIN(MultiAiLifecycleTests)
#include "multi_ai_lifecycle_tests.moc"

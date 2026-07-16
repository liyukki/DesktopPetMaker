#include "petruntimeinstance.h"

#include "petspeechbubblewindow.h"
#include "runtimepetwindow.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QScreen>

PetRuntimeInstance::PetRuntimeInstance(const QString &projectPath, QObject *parent)
    : QObject(parent)
    , m_projectPath(QDir::cleanPath(QFileInfo(projectPath).absoluteFilePath()))
    , m_conversationContext(QSharedPointer<PetConversationContext>::create())
{
}

QString PetRuntimeInstance::displayName() const
{
    const QString characterName = m_project.aiCharacterName.trimmed();
    if (!characterName.isEmpty()) {
        return characterName;
    }
    return m_project.name.trimmed().isEmpty() ? QFileInfo(m_projectPath).absoluteDir().dirName() : m_project.name.trimmed();
}

PetSpeechBubbleWindow *PetRuntimeInstance::speechBubble() const
{
    return m_speechBubble.data();
}

bool PetRuntimeInstance::start(int preferredSpawnX)
{
    if (isRunning()) {
        return true;
    }

    QString error;
    if (!m_project.load(m_projectPath, &error)) {
        return false;
    }

    auto *window = new RuntimePetWindow(m_project, m_conversationContext);
    window->setAttribute(Qt::WA_DeleteOnClose);
    connect(window, &RuntimePetWindow::controlCenterRequested, this, &PetRuntimeInstance::controlCenterRequested);
    connect(window, &RuntimePetWindow::exitApplicationRequested, this, &PetRuntimeInstance::exitApplicationRequested);
    connect(window, &RuntimePetWindow::queuedAiActionFinished, this,
            [this](const QString &roomId, const QString &messageId, const RuntimeActionDispatchResult &result) {
                emit queuedAiActionFinished(m_projectPath, roomId, messageId, result);
            });
    connect(window, &QObject::destroyed, this, [this]() {
        m_runtimeWindow = nullptr;
        m_speechBubble = nullptr;
        emit statusChanged(m_projectPath, runtimeStatusText());
        emit stopped(m_projectPath);
    });

    if (!m_project.hasRuntimeAnchorScreen) {
        const QRect screen = QApplication::primaryScreen()
            ? QApplication::primaryScreen()->availableGeometry()
            : QRect(0, 0, 1536, 912);
        window->placeOnGroundAtX(preferredSpawnX >= 0 ? preferredSpawnX : screen.left() + 80);
    }

    m_runtimeWindow = window;
    m_speechBubble = window->speechBubble();
    window->show();
    window->raise();
    emit statusChanged(m_projectPath, runtimeStatusText());
    return true;
}

void PetRuntimeInstance::stop()
{
    if (m_runtimeWindow) {
        m_runtimeWindow->close();
    }
}

RuntimeReplyDeliveryResult PetRuntimeInstance::deliverRoomAiReply(const QString &reply,
                                                                  const QString &actionId,
                                                                  const QJsonObject &actionParameters,
                                                                  const QString &roomName,
                                                                  const QString &userMessage,
                                                                  const QString &roomId,
                                                                  const QString &messageId)
{
    if (m_runtimeWindow) {
        return m_runtimeWindow->deliverRoomAiReply(reply, actionId, actionParameters, roomName, userMessage, roomId, messageId);
    }
    return {false,
            {actionId.isEmpty() ? RuntimeActionDispatchStatus::NotRequested : RuntimeActionDispatchStatus::Failed,
             actionId.isEmpty() ? QString() : QStringLiteral("PetNotRunning"),
             actionId.isEmpty() ? QString() : QStringLiteral("The desktop pet is not running.")},
            QStringLiteral("PetNotRunning"),
            QStringLiteral("The desktop pet is not running.")};
}

QString PetRuntimeInstance::runtimeStatusText() const
{
    return isRunning() ? QStringLiteral("运行中") : QStringLiteral("已停止");
}

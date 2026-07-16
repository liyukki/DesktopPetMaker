#ifndef PETRUNTIMEINSTANCE_H
#define PETRUNTIMEINSTANCE_H

#include <QObject>
#include <QJsonObject>
#include <QPointer>
#include <QSharedPointer>
#include <QString>

#include "petconversationcontext.h"
#include "petproject.h"
#include "runtimeactionresult.h"

class RuntimePetWindow;
class PetSpeechBubbleWindow;

class PetRuntimeInstance : public QObject
{
    Q_OBJECT

public:
    explicit PetRuntimeInstance(const QString &projectPath, QObject *parent = nullptr);

    QString projectPath() const { return m_projectPath; }
    QString displayName() const;
    RuntimePetWindow *runtimeWindow() const { return m_runtimeWindow.data(); }
    PetSpeechBubbleWindow *speechBubble() const;
    QSharedPointer<PetConversationContext> conversationContext() const { return m_conversationContext; }
    bool start(int preferredSpawnX = -1);
    void stop();
    RuntimeReplyDeliveryResult deliverRoomAiReply(const QString &reply,
                                                  const QString &actionId = QString(),
                                                  const QJsonObject &actionParameters = {},
                                                  const QString &roomName = QString(),
                                                  const QString &userMessage = QString(),
                                                  const QString &roomId = QString(),
                                                  const QString &messageId = QString());
    bool isRunning() const { return !m_runtimeWindow.isNull(); }
    QString runtimeStatusText() const;
    PetProject project() const { return m_project; }

signals:
    void stopped(const QString &projectPath);
    void statusChanged(const QString &projectPath, const QString &status);
    void controlCenterRequested();
    void exitApplicationRequested();
    void queuedAiActionFinished(const QString &projectPath,
                                const QString &roomId,
                                const QString &messageId,
                                const RuntimeActionDispatchResult &result);

private:
    QString m_projectPath;
    PetProject m_project;
    QPointer<RuntimePetWindow> m_runtimeWindow;
    QPointer<PetSpeechBubbleWindow> m_speechBubble;
    QSharedPointer<PetConversationContext> m_conversationContext;
};

#endif // PETRUNTIMEINSTANCE_H

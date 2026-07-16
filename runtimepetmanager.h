#ifndef RUNTIMEPETMANAGER_H
#define RUNTIMEPETMANAGER_H

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include "runtimeactionresult.h"

class PetRuntimeInstance;
class RuntimePetWindow;

struct PetEntry
{
    QString displayName;
    QString projectName;
    QString petJsonPath;
    QString coverPath;
};

class RuntimePetManager : public QObject
{
    Q_OBJECT

public:
    explicit RuntimePetManager(QObject *parent = nullptr);
    ~RuntimePetManager() override;

    bool startPet(const QString &petJsonPath);
    void stopPet(const QString &petJsonPath);
    void stopAllPets();
    bool restartPet(const QString &petJsonPath);
    bool isPetRunning(const QString &petJsonPath) const;
    PetRuntimeInstance *instance(const QString &petJsonPath) const;
    RuntimeReplyDeliveryResult deliverRoomAiReply(const QString &petJsonPath,
                                                  const QString &reply,
                                                  const QString &actionId = QString(),
                                                  const QJsonObject &actionParameters = {},
                                                  const QString &roomName = QString(),
                                                  const QString &userMessage = QString(),
                                                  const QString &roomId = QString(),
                                                  const QString &messageId = QString());
    QVector<PetRuntimeInstance *> runningInstances() const;
    int runningPetCount() const;
    void showAllPets();
    void setAllPatrolEnabled(bool enabled);
    void setAllMousePassthrough(bool enabled);

    void restoreStartupPets();
    QStringList startupPetPaths() const;
    QString startupPetPath() const;
    QVector<PetEntry> availablePets() const;
    void exitApplication();

    // Compatibility helpers for existing V1 control-center code.
    void stopCurrentPet();
    bool switchPet(const QString &petJsonPath);
    QString currentPetPath() const;
    RuntimePetWindow *currentPet() const;

signals:
    void currentPetChanged(const QString &petJsonPath);
    void petStarted(const QString &path);
    void petStopped(const QString &path);
    void petStopped();
    void runningPetsChanged();
    void petStatusChanged(const QString &path, const QString &status);
    void controlCenterRequested();
    void queuedAiActionFinished(const QString &projectPath,
                                const QString &roomId,
                                const QString &messageId,
                                const RuntimeActionDispatchResult &result);

private:
    QString canonicalPath(const QString &petJsonPath) const;
    QString defaultPetPath() const;
    QString petPathForName(const QString &dirName) const;
    QString displayNameForPet(const QString &petJsonPath, const QString &fallback) const;
    void persistRunningList() const;
    void persistPaths(const QStringList &paths) const;
    QStringList currentRunningPaths() const;
    int nextSpawnX(const QRect &availableGeometry) const;
    void rememberCurrentPath(const QString &path);

    QHash<QString, PetRuntimeInstance *> m_instances;
    QSet<QString> m_pendingRestarts;
    QStringList m_exitRestoreSnapshot;
    QString m_currentPetPath;
    bool m_exitingApplication {false};
    bool m_stoppingAll {false};
};

#endif // RUNTIMEPETMANAGER_H

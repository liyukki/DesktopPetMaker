#ifndef PETAIREQUESTCOORDINATOR_H
#define PETAIREQUESTCOORDINATOR_H

#include <QDateTime>
#include <QEnableSharedFromThis>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSharedPointer>
#include <QString>

enum class PetAiRequestSource
{
    SingleChat,
    ProactiveChat,
    MultiAi
};

struct PetAiRequestInfo
{
    QString requestId;
    QString effectiveProjectId;
    QString normalizedProjectPath;
    PetAiRequestSource source {PetAiRequestSource::SingleChat};
    QString roomId;
    QDateTime startedAt;
    bool cancelled {false};
};

class PetAiRequestCoordinator;
class AIRequestHandle;

class PetAiRequestLease : public QEnableSharedFromThis<PetAiRequestLease>
{
public:
    ~PetAiRequestLease();

    QString requestId() const { return m_info.requestId; }
    PetAiRequestInfo info() const { return m_info; }
    bool active() const { return m_active; }
    void bindHandle(AIRequestHandle *handle);
    void release();

private:
    friend class PetAiRequestCoordinator;
    PetAiRequestLease(PetAiRequestCoordinator *coordinator,
                      const QString &key,
                      const PetAiRequestInfo &info,
                      int timeoutMs);

    QPointer<PetAiRequestCoordinator> m_coordinator;
    QString m_key;
    PetAiRequestInfo m_info;
    QPointer<AIRequestHandle> m_handle;
    int m_timeoutMs {35000};
    bool m_active {true};
};

class PetAiRequestCoordinator : public QObject
{
    Q_OBJECT

public:
    static PetAiRequestCoordinator &instance();

    QSharedPointer<PetAiRequestLease> acquire(const QString &effectiveProjectId,
                                               const QString &projectPath,
                                               PetAiRequestSource source,
                                               const QString &roomId = {},
                                               QString *errorMessage = nullptr,
                                               int timeoutMs = 35000);
    bool isBusy(const QString &effectiveProjectId, const QString &projectPath) const;
    PetAiRequestInfo activeRequest(const QString &effectiveProjectId, const QString &projectPath) const;
    int activeLeaseCount() const { return m_activeRequests.size(); }

private:
    friend class PetAiRequestLease;
    explicit PetAiRequestCoordinator(QObject *parent = nullptr);
    QString keyFor(const QString &effectiveProjectId, const QString &projectPath) const;
    QString normalizedPath(const QString &projectPath) const;
    void release(const QString &key, const QString &requestId);

    QHash<QString, PetAiRequestInfo> m_activeRequests;
};

#endif // PETAIREQUESTCOORDINATOR_H

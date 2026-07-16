#include "petairequestcoordinator.h"

#include "aiprovider.h"

#include <QFileInfo>
#include <QTimer>
#include <QUuid>

PetAiRequestLease::PetAiRequestLease(PetAiRequestCoordinator *coordinator,
                                     const QString &key,
                                     const PetAiRequestInfo &info,
                                     int timeoutMs)
    : m_coordinator(coordinator)
    , m_key(key)
    , m_info(info)
    , m_timeoutMs(qMax(1000, timeoutMs))
{
}

PetAiRequestLease::~PetAiRequestLease()
{
    release();
}

void PetAiRequestLease::release()
{
    if (!m_active) {
        return;
    }
    m_active = false;
    if (m_coordinator) {
        m_coordinator->release(m_key, m_info.requestId);
    }
}

void PetAiRequestLease::bindHandle(AIRequestHandle *handle)
{
    m_handle = handle;
    if (!m_active || !m_coordinator || !m_handle || m_handle->isFinished()) {
        return;
    }

    const QWeakPointer<PetAiRequestLease> weakLease(sharedFromThis());
    QTimer::singleShot(m_timeoutMs, m_coordinator, [weakLease]() {
        const auto activeLease = weakLease.toStrongRef();
        if (!activeLease || !activeLease->active() || !activeLease->m_handle
            || activeLease->m_handle->isFinished()) {
            return;
        }
        activeLease->m_handle->cancel(QStringLiteral("AI request timed out."), QStringLiteral("TimedOut"));
    });
}

PetAiRequestCoordinator &PetAiRequestCoordinator::instance()
{
    static PetAiRequestCoordinator coordinator;
    return coordinator;
}

PetAiRequestCoordinator::PetAiRequestCoordinator(QObject *parent)
    : QObject(parent)
{
}

QSharedPointer<PetAiRequestLease> PetAiRequestCoordinator::acquire(const QString &effectiveProjectId,
                                                                     const QString &projectPath,
                                                                     PetAiRequestSource source,
                                                                     const QString &roomId,
                                                                     QString *errorMessage,
                                                                     int timeoutMs)
{
    const QString key = keyFor(effectiveProjectId, projectPath);
    if (m_activeRequests.contains(key)) {
        if (errorMessage) {
            *errorMessage = tr("This desktop pet already has an AI request in progress.");
        }
        return {};
    }

    PetAiRequestInfo info;
    info.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    info.effectiveProjectId = effectiveProjectId.trimmed();
    info.normalizedProjectPath = normalizedPath(projectPath);
    info.source = source;
    info.roomId = roomId;
    info.startedAt = QDateTime::currentDateTimeUtc();
    m_activeRequests.insert(key, info);

    auto lease = QSharedPointer<PetAiRequestLease>(new PetAiRequestLease(this, key, info, timeoutMs));
    return lease;
}

bool PetAiRequestCoordinator::isBusy(const QString &effectiveProjectId, const QString &projectPath) const
{
    return m_activeRequests.contains(keyFor(effectiveProjectId, projectPath));
}

PetAiRequestInfo PetAiRequestCoordinator::activeRequest(const QString &effectiveProjectId,
                                                         const QString &projectPath) const
{
    return m_activeRequests.value(keyFor(effectiveProjectId, projectPath));
}

QString PetAiRequestCoordinator::keyFor(const QString &effectiveProjectId, const QString &projectPath) const
{
    const QString projectId = effectiveProjectId.trimmed();
    if (!projectId.isEmpty()) {
        return QStringLiteral("id:%1").arg(projectId);
    }

    const QString path = normalizedPath(projectPath);
    return path.isEmpty() ? QStringLiteral("anonymous") : QStringLiteral("path:%1").arg(path);
}

QString PetAiRequestCoordinator::normalizedPath(const QString &projectPath) const
{
    if (projectPath.trimmed().isEmpty()) {
        return {};
    }
    const QFileInfo info(projectPath);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

void PetAiRequestCoordinator::release(const QString &key, const QString &requestId)
{
    const auto it = m_activeRequests.constFind(key);
    if (it == m_activeRequests.constEnd() || it->requestId != requestId) {
        return;
    }
    m_activeRequests.remove(key);
}

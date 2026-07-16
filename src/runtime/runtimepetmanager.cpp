#include "runtimepetmanager.h"

#include "petruntimeinstance.h"
#include "petproject.h"
#include "petprojectregistry.h"
#include "runtimepetwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <algorithm>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>

RuntimePetManager::RuntimePetManager(QObject *parent)
    : QObject(parent)
{
}

RuntimePetManager::~RuntimePetManager()
{
    stopAllPets();
}

QString RuntimePetManager::canonicalPath(const QString &petJsonPath) const
{
    if (petJsonPath.trimmed().isEmpty()) {
        return QString();
    }
    const QFileInfo info(petJsonPath);
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty()) {
        return QDir::cleanPath(canonical);
    }
    return QDir::cleanPath(info.absoluteFilePath());
}

bool RuntimePetManager::startPet(const QString &petJsonPath)
{
    const QString path = canonicalPath(petJsonPath);
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return false;
    }
    if (m_instances.contains(path)) {
        rememberCurrentPath(path);
        return true;
    }

    auto *runtime = new PetRuntimeInstance(path, this);
    connect(runtime, &PetRuntimeInstance::stopped, this, [this](const QString &stoppedPath) {
        PetRuntimeInstance *old = m_instances.take(stoppedPath);
        if (old) {
            old->deleteLater();
        }
        if (canonicalPath(m_currentPetPath) == stoppedPath) {
            m_currentPetPath.clear();
            emit currentPetChanged(QString());
        }
        emit petStopped(stoppedPath);
        emit petStopped();
        emit runningPetsChanged();
        const bool shouldRestart = !m_exitingApplication && !m_stoppingAll && m_pendingRestarts.remove(stoppedPath) > 0;
        if (!m_exitingApplication) {
            persistRunningList();
        }
        if (m_stoppingAll && m_instances.isEmpty()) {
            m_stoppingAll = false;
            if (!m_exitingApplication) {
                persistRunningList();
            }
        }
        if (shouldRestart) {
            QTimer::singleShot(0, this, [this, stoppedPath]() {
                startPet(stoppedPath);
            });
        }
        if (m_exitingApplication && m_instances.isEmpty()) {
            QCoreApplication::quit();
        }
    });
    connect(runtime, &PetRuntimeInstance::statusChanged, this, &RuntimePetManager::petStatusChanged);
    connect(runtime, &PetRuntimeInstance::controlCenterRequested, this, &RuntimePetManager::controlCenterRequested);
    connect(runtime, &PetRuntimeInstance::exitApplicationRequested, this, &RuntimePetManager::exitApplication);
    connect(runtime, &PetRuntimeInstance::queuedAiActionFinished, this, &RuntimePetManager::queuedAiActionFinished);

    const QRect screen = QApplication::primaryScreen()
        ? QApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, 1536, 912);
    if (!runtime->start(nextSpawnX(screen))) {
        runtime->deleteLater();
        QMessageBox::warning(nullptr,
                             QObject::tr("Open Pet Failed"),
                             QObject::tr("Failed to open %1").arg(path));
        return false;
    }

    m_instances.insert(path, runtime);
    rememberCurrentPath(path);
    persistRunningList();
    emit petStarted(path);
    emit runningPetsChanged();
    return true;
}

void RuntimePetManager::stopPet(const QString &petJsonPath)
{
    const QString path = canonicalPath(petJsonPath);
    PetRuntimeInstance *runtime = m_instances.value(path, nullptr);
    if (runtime) {
        runtime->stop();
    }
}

void RuntimePetManager::stopAllPets()
{
    m_stoppingAll = true;
    m_pendingRestarts.clear();
    const auto values = m_instances.values();
    for (PetRuntimeInstance *runtime : values) {
        if (runtime) {
            runtime->stop();
        }
    }
    if (values.isEmpty()) {
        persistRunningList();
        m_stoppingAll = false;
    }
}

bool RuntimePetManager::restartPet(const QString &petJsonPath)
{
    const QString path = canonicalPath(petJsonPath);
    if (!m_instances.contains(path)) {
        return startPet(path);
    }
    if (m_pendingRestarts.contains(path)) {
        return true;
    }
    m_pendingRestarts.insert(path);
    PetRuntimeInstance *runtime = m_instances.value(path, nullptr);
    if (runtime) {
        runtime->stop();
    }
    return true;
}

bool RuntimePetManager::isPetRunning(const QString &petJsonPath) const
{
    return m_instances.contains(canonicalPath(petJsonPath));
}

PetRuntimeInstance *RuntimePetManager::instance(const QString &petJsonPath) const
{
    return m_instances.value(canonicalPath(petJsonPath), nullptr);
}

RuntimeReplyDeliveryResult RuntimePetManager::deliverRoomAiReply(const QString &petJsonPath,
                                                                 const QString &reply,
                                                                 const QString &actionId,
                                                                 const QJsonObject &actionParameters,
                                                                 const QString &roomName,
                                                                 const QString &userMessage,
                                                                 const QString &roomId,
                                                                 const QString &messageId)
{
    PetRuntimeInstance *runtime = instance(petJsonPath);
    if (!runtime || !runtime->isRunning()) {
        return {false,
                {actionId.isEmpty() ? RuntimeActionDispatchStatus::NotRequested : RuntimeActionDispatchStatus::Failed,
                 actionId.isEmpty() ? QString() : QStringLiteral("PetNotRunning"),
                 actionId.isEmpty() ? QString() : QStringLiteral("The desktop pet is not running.")},
                QStringLiteral("PetNotRunning"),
                QStringLiteral("The desktop pet is not running.")};
    }
    return runtime->deliverRoomAiReply(reply, actionId, actionParameters, roomName, userMessage, roomId, messageId);
}

QVector<PetRuntimeInstance *> RuntimePetManager::runningInstances() const
{
    QVector<PetRuntimeInstance *> result;
    for (PetRuntimeInstance *runtime : m_instances) {
        if (runtime && runtime->isRunning()) {
            result.append(runtime);
        }
    }
    return result;
}

int RuntimePetManager::runningPetCount() const
{
    return runningInstances().size();
}

void RuntimePetManager::showAllPets()
{
    for (PetRuntimeInstance *runtime : runningInstances()) {
        if (RuntimePetWindow *window = runtime->runtimeWindow()) {
            window->ensureVisibleOnAvailableScreens(true);
            window->show();
            window->raise();
        }
    }
}

void RuntimePetManager::setAllPatrolEnabled(bool enabled)
{
    for (PetRuntimeInstance *runtime : runningInstances()) {
        if (RuntimePetWindow *window = runtime->runtimeWindow()) {
            window->setRuntimePatrolEnabled(enabled);
        }
    }
}

void RuntimePetManager::setAllMousePassthrough(bool enabled)
{
    for (PetRuntimeInstance *runtime : runningInstances()) {
        if (RuntimePetWindow *window = runtime->runtimeWindow()) {
            window->setRuntimeMousePassthrough(enabled);
        }
    }
}

void RuntimePetManager::restoreStartupPets()
{
    const QStringList paths = startupPetPaths();
    for (const QString &path : paths) {
        startPet(path);
    }
}

QStringList RuntimePetManager::startupPetPaths() const
{
    QSettings settings;
    const bool restore = settings.value(QStringLiteral("app/restoreRunningPets"), true).toBool();
    QStringList paths;
    if (restore) {
        paths = settings.value(QStringLiteral("app/runningPetProjects")).toStringList();
    }
    if (restore && paths.isEmpty()) {
        const QString legacy = settings.value(QStringLiteral("app/activePetProject")).toString();
        if (!legacy.isEmpty()) {
            paths.append(legacy);
        }
    }
    paths.erase(std::remove_if(paths.begin(), paths.end(), [](const QString &path) {
                    return !QFileInfo::exists(path);
                }),
                paths.end());
    paths.removeDuplicates();
    if (paths.isEmpty()) {
        paths.append(startupPetPath());
    }
    return paths;
}

QString RuntimePetManager::startupPetPath() const
{
    const QString first = defaultPetPath();
    if (QFileInfo::exists(first)) {
        return first;
    }
    return petPathForName(QStringLiteral("pet_animated_1"));
}

QVector<PetEntry> RuntimePetManager::availablePets() const
{
    QVector<PetEntry> pets;
    PetProjectRegistry registry;
    registry.scan();
    for (const PetProjectEntry &registeredPet : registry.entries()) {
        PetEntry entry;
        entry.projectName = registeredPet.projectName;
        entry.displayName = registeredPet.displayName;
        entry.petJsonPath = registeredPet.petJsonPath;
        entry.coverPath = registeredPet.coverPath;
        pets.append(entry);
    }
    return pets;
}

void RuntimePetManager::exitApplication()
{
    m_exitRestoreSnapshot = currentRunningPaths();
    persistPaths(m_exitRestoreSnapshot);
    m_exitingApplication = true;
    m_pendingRestarts.clear();
    const auto values = m_instances.values();
    for (PetRuntimeInstance *runtime : values) {
        if (runtime) {
            runtime->stop();
        }
    }
    if (m_instances.isEmpty()) {
        QCoreApplication::quit();
    }
}

void RuntimePetManager::stopCurrentPet()
{
    if (!m_currentPetPath.isEmpty()) {
        stopPet(m_currentPetPath);
    }
}

bool RuntimePetManager::switchPet(const QString &petJsonPath)
{
    return startPet(petJsonPath);
}

QString RuntimePetManager::currentPetPath() const
{
    return m_currentPetPath;
}

RuntimePetWindow *RuntimePetManager::currentPet() const
{
    PetRuntimeInstance *runtime = instance(m_currentPetPath);
    return runtime ? runtime->runtimeWindow() : nullptr;
}

QString RuntimePetManager::defaultPetPath() const
{
    return petPathForName(QStringLiteral("pet_animated"));
}

QString RuntimePetManager::petPathForName(const QString &dirName) const
{
    const QString relative = QStringLiteral("%1/pet.json").arg(dirName);
    QStringList candidates;
    const QList<QDir> roots = {
        QDir(QCoreApplication::applicationDirPath()),
        QDir(QDir::currentPath())
    };
    for (QDir dir : roots) {
        for (int i = 0; i < 6; ++i) {
            candidates.append(QDir::cleanPath(dir.absoluteFilePath(relative)));
            if (!dir.cdUp()) {
                break;
            }
        }
    }
    candidates.removeDuplicates();
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return canonicalPath(candidate);
        }
    }
    return candidates.isEmpty() ? relative : canonicalPath(candidates.constFirst());
}

QString RuntimePetManager::displayNameForPet(const QString &petJsonPath, const QString &fallback) const
{
    PetProject project;
    if (!project.load(petJsonPath)) {
        return fallback;
    }
    const QString characterName = project.aiCharacterName.trimmed();
    if (!characterName.isEmpty()) {
        return characterName;
    }
    return project.name.trimmed().isEmpty() ? fallback : project.name.trimmed();
}

void RuntimePetManager::persistRunningList() const
{
    if (m_exitingApplication) {
        persistPaths(m_exitRestoreSnapshot);
        return;
    }
    persistPaths(currentRunningPaths());
}

void RuntimePetManager::persistPaths(const QStringList &paths) const
{
    QStringList cleanPaths = paths;
    cleanPaths.removeDuplicates();
    cleanPaths.sort();
    QSettings().setValue(QStringLiteral("app/runningPetProjects"), cleanPaths);
}

QStringList RuntimePetManager::currentRunningPaths() const
{
    QStringList paths = m_instances.keys();
    paths.sort();
    return paths;
}

int RuntimePetManager::nextSpawnX(const QRect &availableGeometry) const
{
    const int step = 240;
    const int margin = 80;
    const int count = qMax(0, m_instances.size());
    const int usableLeft = availableGeometry.left() + margin;
    const int usableRight = availableGeometry.right() - margin;
    if (usableRight <= usableLeft) {
        return availableGeometry.center().x();
    }
    const int span = qMax(1, usableRight - usableLeft);
    return usableLeft + (count * step) % span;
}

void RuntimePetManager::rememberCurrentPath(const QString &path)
{
    m_currentPetPath = canonicalPath(path);
    QSettings settings;
    settings.setValue(QStringLiteral("app/activePetProject"), m_currentPetPath);
    emit currentPetChanged(m_currentPetPath);
}

#include "petprojectregistry.h"

#include "petproject.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QSet>

PetProjectRegistry::PetProjectRegistry(const QString &rootPath)
    : m_rootPath(rootPath.isEmpty() ? defaultRootPath() : QDir::cleanPath(rootPath))
{
}

QString PetProjectRegistry::defaultRootPath() const
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        if (QFileInfo::exists(dir.filePath(QStringLiteral("pro/CMakeLists.txt")))
            || QFileInfo::exists(dir.filePath(QStringLiteral("pet_animated/pet.json")))) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QDir::currentPath();
}

void PetProjectRegistry::scan()
{
    m_entries.clear();
    QSet<QString> seen;
    auto addUnique = [this, &seen](const QString &path, bool legacy, bool external) {
        const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        if (seen.contains(clean)) {
            return;
        }
        seen.insert(clean);
        addProject(clean, legacy, external);
    };

    QDir petsDir(QDir(m_rootPath).filePath(QStringLiteral("pets")));
    const QFileInfoList managedDirs = petsDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &dirInfo : managedDirs) {
        addUnique(QDir(dirInfo.absoluteFilePath()).filePath(QStringLiteral("pet.json")), false, false);
    }

    addUnique(QDir(m_rootPath).filePath(QStringLiteral("pet_animated/pet.json")), true, false);
    addUnique(QDir(m_rootPath).filePath(QStringLiteral("pet_animated_1/pet.json")), true, false);

    const QStringList external = QSettings().value(QStringLiteral("pets/externalProjects")).toStringList();
    for (const QString &path : external) {
        addUnique(path, false, true);
    }
}

void PetProjectRegistry::addProject(const QString &petJsonPath, bool legacy, bool external)
{
    if (!QFileInfo::exists(petJsonPath)) {
        return;
    }
    PetProject project;
    if (!project.load(petJsonPath)) {
        return;
    }
    PetProjectEntry entry;
    entry.petJsonPath = QDir::cleanPath(QFileInfo(petJsonPath).absoluteFilePath());
    entry.projectDirectory = QFileInfo(entry.petJsonPath).absolutePath();
    entry.projectId = project.effectiveProjectId();
    entry.projectName = project.name;
    entry.displayName = project.aiCharacterName.trimmed().isEmpty() ? project.name : project.aiCharacterName.trimmed();
    entry.coverPath = project.absolutePathFor(project.coverPath);
    entry.actionCount = project.actions.size();
    entry.legacyProject = legacy;
    entry.externalProject = external;
    m_entries.append(entry);
}

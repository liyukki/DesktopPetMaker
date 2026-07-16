#ifndef PETPROJECTREGISTRY_H
#define PETPROJECTREGISTRY_H

#include <QString>
#include <QVector>

struct PetProjectEntry
{
    QString petJsonPath;
    QString projectDirectory;
    QString projectId;
    QString projectName;
    QString displayName;
    QString coverPath;
    int actionCount {0};
    bool legacyProject {false};
    bool externalProject {false};
};

class PetProjectRegistry
{
public:
    explicit PetProjectRegistry(const QString &rootPath = QString());

    void scan();
    QVector<PetProjectEntry> entries() const { return m_entries; }
    QString rootPath() const { return m_rootPath; }

private:
    void addProject(const QString &petJsonPath, bool legacy, bool external);
    QString defaultRootPath() const;

    QString m_rootPath;
    QVector<PetProjectEntry> m_entries;
};

#endif // PETPROJECTREGISTRY_H

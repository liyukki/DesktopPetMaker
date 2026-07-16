#ifndef SHIMEJIIMPORTWIZARD_H
#define SHIMEJIIMPORTWIZARD_H

#include <QDialog>
#include <QPoint>
#include <QPointF>
#include <QHash>
#include <QStringList>

#include "petproject.h"

struct ShimejiPose
{
    QString image;
    QPoint imageAnchor;
    QPointF velocity;
    int duration {0};
};

struct ShimejiActionImport
{
    QString name;
    QString type;
    QString borderType;
    QVector<ShimejiPose> poses;
    QStringList unsupportedConditions;
};

struct ShimejiBehaviorImport
{
    QString name;
    QString actionName;
    double frequency {1.0};
};

struct ShimejiBatchImportResult
{
    bool ok {false};
    int actionsImported {0};
    int behaviorsImported {0};
    QHash<QString, QString> actionNameToActionId;
    QString errorMessage;
    QStringList warnings;
};

class IAssetTransactionFileSystem
{
public:
    virtual ~IAssetTransactionFileSystem() = default;
    virtual bool renameDirectory(const QString &source, const QString &target) = 0;
    virtual bool removeTree(const QString &path) = 0;
    virtual bool updateAutoCover(PetProject &project, QString *errorMessage) = 0;
    virtual bool saveProject(PetProject &project, QString *errorMessage) = 0;
};

class ShimejiImportWizard : public QDialog
{
    Q_OBJECT

public:
    explicit ShimejiImportWizard(QWidget *parent = nullptr);
    static QStringList parseActionNames(const QString &actionsXmlPath, QStringList *unsupported = nullptr);
    static QVector<ShimejiActionImport> parseActions(const QString &actionsXmlPath, QStringList *unsupported = nullptr);
    static QVector<ShimejiBehaviorImport> parseBehaviors(const QString &behaviorsXmlPath, QStringList *unsupported = nullptr);
    static bool importActionToProject(PetProject &project,
                                      const QString &imageSetDir,
                                      const ShimejiActionImport &actionImport,
                                      const QString &systemRole,
                                      QString *createdActionId = nullptr,
                                      QString *errorMessage = nullptr);
    static ShimejiBatchImportResult importPackageToProject(
        PetProject &project,
        const QString &imageSetDir,
        const QVector<ShimejiActionImport> &actions,
        const QVector<ShimejiBehaviorImport> &behaviors = {},
        const QString &singleActionSystemRole = QStringLiteral("None"));
    static int importBehaviorsToProject(PetProject &project,
                                        const QVector<ShimejiBehaviorImport> &behaviors,
                                        const QHash<QString, QString> &actionNameToActionId);
    static void setAssetTransactionFileSystemForTesting(IAssetTransactionFileSystem *fileSystem);
};

#endif // SHIMEJIIMPORTWIZARD_H

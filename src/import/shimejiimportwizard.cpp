#include "shimejiimportwizard.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <QVBoxLayout>
#include <QXmlStreamReader>

namespace {
class DefaultAssetTransactionFileSystem final : public IAssetTransactionFileSystem
{
public:
    bool renameDirectory(const QString &source, const QString &target) override { return QDir().rename(source, target); }
    bool removeTree(const QString &path) override { return !QFileInfo::exists(path) || QDir(path).removeRecursively(); }
    bool updateAutoCover(PetProject &project, QString *errorMessage) override { return project.updateAutoCover(errorMessage); }
    bool saveProject(PetProject &project, QString *errorMessage) override { return project.save(errorMessage); }
};

DefaultAssetTransactionFileSystem g_defaultTransactionFileSystem;
IAssetTransactionFileSystem *g_transactionFileSystemForTesting = nullptr;

IAssetTransactionFileSystem &transactionFileSystem()
{
    return g_transactionFileSystemForTesting ? *g_transactionFileSystemForTesting : g_defaultTransactionFileSystem;
}

bool writeTransactionJournal(const QString &path, const QJsonObject &journal)
{
    QSaveFile file(path);
    const QByteArray data = QJsonDocument(journal).toJson(QJsonDocument::Compact);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}

void recoverInterruptedShimejiTransactions(const PetProject &project)
{
    const QString root = QDir(project.projectDir).filePath(QStringLiteral(".asset_staging"));
    QDir staging(root);
    for (const QString &name : staging.entryList({QStringLiteral("shimeji_batch_*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString transactionRoot = staging.filePath(name);
        QFile journalFile(QDir(transactionRoot).filePath(QStringLiteral("transaction.json")));
        if (!journalFile.open(QIODevice::ReadOnly)) continue;
        const QJsonObject journal = QJsonDocument::fromJson(journalFile.readAll()).object();
        if (journal.value(QStringLiteral("phase")).toString() == QStringLiteral("committed")) {
            QDir(transactionRoot).removeRecursively();
            continue;
        }
        for (const QJsonValue &value : journal.value(QStringLiteral("committedActionDirs")).toArray()) {
            QDir(value.toString()).removeRecursively();
        }
        const QString petJsonPath = journal.value(QStringLiteral("petJsonPath")).toString();
        const QByteArray originalJson = QByteArray::fromBase64(journal.value(QStringLiteral("originalPetJson")).toString().toLatin1());
        if (!petJsonPath.isEmpty() && !originalJson.isEmpty()) {
            QSaveFile output(petJsonPath);
            if (output.open(QIODevice::WriteOnly) && output.write(originalJson) == originalJson.size()) output.commit();
        }
        const QString coverPath = journal.value(QStringLiteral("coverPath")).toString();
        if (!coverPath.isEmpty()) {
            if (journal.value(QStringLiteral("coverExisted")).toBool()) {
                const QByteArray cover = QByteArray::fromBase64(journal.value(QStringLiteral("originalCover")).toString().toLatin1());
                QSaveFile output(coverPath);
                if (output.open(QIODevice::WriteOnly) && output.write(cover) == cover.size()) output.commit();
            } else {
                QFile::remove(coverPath);
            }
        }
        QDir(transactionRoot).removeRecursively();
    }
}

QPoint parsePoint(const QString &value)
{
    const QStringList parts = value.split(',', Qt::SkipEmptyParts);
    return parts.size() >= 2 ? QPoint(parts.at(0).trimmed().toInt(), parts.at(1).trimmed().toInt()) : QPoint();
}

QPointF parsePointF(const QString &value)
{
    const QStringList parts = value.split(',', Qt::SkipEmptyParts);
    return parts.size() >= 2 ? QPointF(parts.at(0).trimmed().toDouble(), parts.at(1).trimmed().toDouble()) : QPointF();
}

QString safeActionId(const QString &name)
{
    QString id = QStringLiteral("shimeji_%1").arg(name.trimmed().toLower());
    id.replace(QRegularExpression(QStringLiteral("[^a-z0-9_-]")), QStringLiteral("_"));
    id.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    return id.trimmed().isEmpty() ? QStringLiteral("shimeji_action") : id;
}

QString resolveImagePath(const QString &imageSetDir, const QString &image, QString *errorMessage)
{
    const QDir root(imageSetDir);
    const QString rootCanonical = QFileInfo(imageSetDir).canonicalFilePath();
    QString stripped = image.trimmed();
    while (stripped.startsWith('/') || stripped.startsWith('\\')) {
        stripped.remove(0, 1);
    }
    const QString candidate = QFileInfo(root.filePath(stripped)).canonicalFilePath();
    if (candidate.isEmpty() || !QFileInfo::exists(candidate)) {
        if (errorMessage) *errorMessage = QStringLiteral("Shimeji image is missing: %1").arg(image);
        return QString();
    }
    const Qt::CaseSensitivity pathSensitivity =
#ifdef Q_OS_WIN
        Qt::CaseInsensitive;
#else
        Qt::CaseSensitive;
#endif
    const QString normalizedRoot = QDir::cleanPath(rootCanonical);
    const QString normalizedCandidate = QDir::cleanPath(candidate);
    const QString rootWithSeparator = normalizedRoot.endsWith(QLatin1Char('/'))
        ? normalizedRoot
        : normalizedRoot + QLatin1Char('/');
    if (!rootCanonical.isEmpty()
        && normalizedCandidate.compare(normalizedRoot, pathSensitivity) != 0
        && !normalizedCandidate.startsWith(rootWithSeparator, pathSensitivity)) {
        if (errorMessage) *errorMessage = QStringLiteral("Shimeji image path escapes selected image set: %1").arg(image);
        return QString();
    }
    return candidate;
}

bool saveOnCanvas(const QImage &source, const QString &targetPath, const QSize &canvasSize, QString *errorMessage)
{
    if (source.isNull()) {
        if (errorMessage) *errorMessage = QStringLiteral("Source image cannot be read.");
        return false;
    }
    if (source.width() > canvasSize.width() || source.height() > canvasSize.height()) {
        if (errorMessage) *errorMessage = QStringLiteral("Shimeji frame is larger than pet canvas.");
        return false;
    }
    QImage canvas(canvasSize, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.drawImage((canvas.width() - source.width()) / 2,
                      canvas.height() - source.height(),
                      source.convertToFormat(QImage::Format_ARGB32_Premultiplied));
    painter.end();
    QDir().mkpath(QFileInfo(targetPath).absolutePath());
    if (!canvas.save(targetPath, "PNG")) {
        if (errorMessage) *errorMessage = QStringLiteral("Failed to write Shimeji frame: %1").arg(targetPath);
        return false;
    }
    return true;
}
}

ShimejiImportWizard::ShimejiImportWizard(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Shimeji 导入向导"));
    auto *layout = new QVBoxLayout(this);
    auto *label = new QLabel(QStringLiteral("第一版支持 Stay / Move / Animate 动画元数据导入；Java 行为标记为部分兼容。"), this);
    label->setWordWrap(true);
    layout->addWidget(label);
}

QStringList ShimejiImportWizard::parseActionNames(const QString &actionsXmlPath, QStringList *unsupported)
{
    QFile file(actionsXmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QStringList actions;
    QXmlStreamReader xml(&file);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) {
            continue;
        }
        if (xml.name() == QStringLiteral("Action")) {
            const QString name = xml.attributes().value(QStringLiteral("Name")).toString();
            const QString type = xml.attributes().value(QStringLiteral("Type")).toString();
            if (!name.isEmpty()) {
                actions.append(name);
            }
            if (unsupported && !(type == QStringLiteral("Stay") || type == QStringLiteral("Move") || type == QStringLiteral("Animate"))) {
                unsupported->append(QStringLiteral("%1:%2").arg(name, type));
            }
        }
    }
    if (xml.hasError()) {
        if (unsupported) {
            unsupported->append(QStringLiteral("XML_ERROR line %1 column %2: %3")
                                    .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(xml.errorString()));
        }
        return {};
    }
    return actions;
}

QVector<ShimejiActionImport> ShimejiImportWizard::parseActions(const QString &actionsXmlPath, QStringList *unsupported)
{
    QFile file(actionsXmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QVector<ShimejiActionImport> actions;
    QXmlStreamReader xml(&file);
    ShimejiActionImport current;
    int actionDepth = 0;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QStringLiteral("Action")) {
            ++actionDepth;
            if (actionDepth == 1) {
                current = {};
                current.name = xml.attributes().value(QStringLiteral("Name")).toString();
                current.type = xml.attributes().value(QStringLiteral("Type")).toString();
                current.borderType = xml.attributes().value(QStringLiteral("BorderType")).toString();
                if (unsupported && !(current.type == QStringLiteral("Stay") || current.type == QStringLiteral("Move") || current.type == QStringLiteral("Animate"))) {
                    unsupported->append(QStringLiteral("%1:%2").arg(current.name, current.type));
                }
            } else if (unsupported) {
                unsupported->append(QStringLiteral("Nested Action inside %1").arg(current.name));
            }
        } else if (actionDepth == 1 && xml.isStartElement() && xml.name() == QStringLiteral("Animation")) {
            const QString condition = xml.attributes().value(QStringLiteral("Condition")).toString();
            if (!condition.isEmpty()) {
                current.unsupportedConditions.append(condition);
            }
        } else if (actionDepth == 1 && xml.isStartElement() && xml.name() == QStringLiteral("Pose")) {
            ShimejiPose pose;
            pose.image = xml.attributes().value(QStringLiteral("Image")).toString();
            pose.imageAnchor = parsePoint(xml.attributes().value(QStringLiteral("ImageAnchor")).toString());
            pose.velocity = parsePointF(xml.attributes().value(QStringLiteral("Velocity")).toString());
            pose.duration = xml.attributes().value(QStringLiteral("Duration")).toInt();
            current.poses.append(pose);
        } else if (xml.isEndElement() && xml.name() == QStringLiteral("Action") && actionDepth > 0) {
            if (actionDepth == 1) {
                actions.append(current);
            }
            --actionDepth;
        }
    }
    if (xml.hasError()) {
        if (unsupported) {
            unsupported->append(QStringLiteral("XML_ERROR line %1 column %2: %3")
                                    .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(xml.errorString()));
        }
        return {};
    }
    return actions;
}

QVector<ShimejiBehaviorImport> ShimejiImportWizard::parseBehaviors(const QString &behaviorsXmlPath, QStringList *unsupported)
{
    QFile file(behaviorsXmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QVector<ShimejiBehaviorImport> behaviors;
    QXmlStreamReader xml(&file);
    ShimejiBehaviorImport current;
    bool inBehavior = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QStringLiteral("Behavior")) {
            inBehavior = true;
            current = {};
            current.name = xml.attributes().value(QStringLiteral("Name")).toString();
            current.frequency = xml.attributes().value(QStringLiteral("Frequency")).toDouble();
            if (current.frequency <= 0.0) {
                current.frequency = xml.attributes().value(QStringLiteral("frequency")).toDouble();
            }
            if (current.frequency <= 0.0) {
                current.frequency = 1.0;
            }
        } else if (inBehavior && xml.isStartElement()) {
            if (xml.name() == QStringLiteral("Action")
                || xml.name() == QStringLiteral("ActionReference")
                || xml.name() == QStringLiteral("ActionRef")) {
                QString actionName = xml.attributes().value(QStringLiteral("Name")).toString();
                if (actionName.isEmpty()) {
                    actionName = xml.attributes().value(QStringLiteral("Action")).toString();
                }
                if (actionName.isEmpty()) {
                    actionName = xml.attributes().value(QStringLiteral("ActionName")).toString();
                }
                if (!actionName.isEmpty() && current.actionName.isEmpty()) {
                    current.actionName = actionName;
                }
            } else if (unsupported) {
                unsupported->append(QStringLiteral("Unsupported behavior element %1 in %2")
                                        .arg(xml.name().toString(), current.name));
            }
        } else if (xml.isEndElement() && xml.name() == QStringLiteral("Behavior") && inBehavior) {
            if (!current.name.isEmpty() && !current.actionName.isEmpty()) {
                behaviors.append(current);
            }
            inBehavior = false;
        }
    }
    if (xml.hasError()) {
        if (unsupported) {
            unsupported->append(QStringLiteral("XML_ERROR line %1 column %2: %3")
                                    .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(xml.errorString()));
        }
        return {};
    }
    return behaviors;
}

bool ShimejiImportWizard::importActionToProject(PetProject &project,
                                                const QString &imageSetDir,
                                                const ShimejiActionImport &actionImport,
                                                const QString &systemRole,
                                                QString *createdActionId,
                                                QString *errorMessage)
{
    const ShimejiBatchImportResult result = importPackageToProject(
        project, imageSetDir, {actionImport}, {}, systemRole);
    if (!result.ok) {
        if (errorMessage) *errorMessage = result.errorMessage;
        return false;
    }
    if (createdActionId) *createdActionId = result.actionNameToActionId.value(actionImport.name);
    return true;
}

ShimejiBatchImportResult ShimejiImportWizard::importPackageToProject(
    PetProject &project,
    const QString &imageSetDir,
    const QVector<ShimejiActionImport> &actions,
    const QVector<ShimejiBehaviorImport> &behaviors,
    const QString &singleActionSystemRole)
{
    ShimejiBatchImportResult result;
    recoverInterruptedShimejiTransactions(project);
    const PetProject originalProject = project;
    if (actions.isEmpty()) {
        result.errorMessage = QStringLiteral("No Shimeji actions were selected for import.");
        return result;
    }

    struct PreparedAction {
        ShimejiActionImport source;
        QString actionId;
        QVector<QImage> images;
    };
    QVector<PreparedAction> prepared;
    QSize requiredCanvas = project.canvasSize;
    for (const ShimejiActionImport &action : actions) {
        if (action.poses.isEmpty()) {
            result.errorMessage = QStringLiteral("Shimeji action has no poses: %1").arg(action.name);
            return result;
        }
        PreparedAction item;
        item.source = action;
        for (const ShimejiPose &pose : action.poses) {
            QString resolveError;
            const QString imagePath = resolveImagePath(imageSetDir, pose.image, &resolveError);
            if (imagePath.isEmpty()) {
                result.errorMessage = resolveError;
                return result;
            }
            QImage image(imagePath);
            if (image.isNull()) {
                result.errorMessage = QStringLiteral("Shimeji image cannot be read: %1").arg(pose.image);
                return result;
            }
            requiredCanvas.setWidth(qMax(requiredCanvas.width(), image.width()));
            requiredCanvas.setHeight(qMax(requiredCanvas.height(), image.height()));
            item.images.append(image);
        }
        prepared.append(item);
    }

    PetProject candidate = project;
    if (!candidate.canvasSize.isValid() || candidate.canvasSize.isEmpty()) {
        candidate.canvasSize = requiredCanvas;
        candidate.anchor = QPoint(requiredCanvas.width() / 2, requiredCanvas.height());
    }
    for (const PreparedAction &item : prepared) {
        for (const QImage &image : item.images) {
            if (image.width() > candidate.canvasSize.width() || image.height() > candidate.canvasSize.height()) {
                result.errorMessage = QStringLiteral("Shimeji frame is larger than the existing pet canvas.");
                return result;
            }
        }
    }

    const QString transactionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString stagingRoot = QDir(candidate.projectDir).filePath(
        QStringLiteral(".asset_staging/shimeji_batch_%1").arg(transactionId));
    QFile originalJsonFile(candidate.petJsonPath());
    const QByteArray originalPetJson = originalJsonFile.open(QIODevice::ReadOnly) ? originalJsonFile.readAll() : QByteArray();
    originalJsonFile.close();
    const QString coverPath = candidate.absolutePathFor(candidate.coverPath);
    const bool coverExisted = QFileInfo::exists(coverPath);
    QByteArray originalCover;
    if (coverExisted) {
        QFile coverFile(coverPath);
        if (coverFile.open(QIODevice::ReadOnly)) originalCover = coverFile.readAll();
    }
    QStringList stagedActionDirs;
    QStringList committedActionDirs;
    const auto rollback = [&]() {
        for (const QString &path : committedActionDirs) {
            if (!transactionFileSystem().removeTree(path)) result.warnings.append(QStringLiteral("Failed to remove committed action: %1").arg(path));
        }
        if (coverExisted) {
            QSaveFile restored(coverPath);
            if (!restored.open(QIODevice::WriteOnly) || restored.write(originalCover) != originalCover.size() || !restored.commit())
                result.warnings.append(QStringLiteral("Failed to restore the original cover."));
        } else if (!QFile::remove(coverPath) && QFileInfo::exists(coverPath)) {
            result.warnings.append(QStringLiteral("Failed to remove generated cover during rollback."));
        }
        if (!originalPetJson.isEmpty()) {
            QSaveFile restoredProject(candidate.petJsonPath());
            if (!restoredProject.open(QIODevice::WriteOnly)
                || restoredProject.write(originalPetJson) != originalPetJson.size()
                || !restoredProject.commit()) {
                result.warnings.append(QStringLiteral("Failed to restore the original pet.json."));
            }
        }
        if (!transactionFileSystem().removeTree(stagingRoot) && QFileInfo::exists(stagingRoot)) {
            result.warnings.append(QStringLiteral("Failed to remove Shimeji staging directory: %1").arg(stagingRoot));
        }
        project = originalProject;
    };

    const QString requestedRole = actions.size() == 1 && !singleActionSystemRole.trimmed().isEmpty()
        ? singleActionSystemRole.trimmed()
        : QStringLiteral("None");
    if (requestedRole != QStringLiteral("None")) {
        for (auto it = candidate.actions.begin(); it != candidate.actions.end(); ++it) {
            if (it.value().systemRole == requestedRole && it.value().frames.isEmpty()) {
                it.value().systemRole = QStringLiteral("None");
            }
        }
    }

    for (PreparedAction &item : prepared) {
        QString actionId = safeActionId(item.source.name);
        int suffix = 2;
        while (candidate.actions.contains(actionId)) {
            actionId = QStringLiteral("%1_%2").arg(safeActionId(item.source.name)).arg(suffix++);
        }
        item.actionId = actionId;
        const QString actionStagingDir = QDir(stagingRoot).filePath(actionId);
        stagedActionDirs.append(actionStagingDir);
        const QPoint referenceAnchor = item.source.poses.first().imageAnchor;

        PetAction action;
        action.id = actionId;
        action.name = actionId;
        action.displayName = item.source.name.trimmed().isEmpty() ? actionId : item.source.name.trimmed();
        action.sourceType = ActionSourceType::ShimejiImported;
        action.shimejiSource.originalActionName = item.source.name;
        action.shimejiSource.originalType = item.source.type;
        action.shimejiSource.borderType = item.source.borderType;
        action.shimejiSource.unsupportedConditions = item.source.unsupportedConditions;
        for (const ShimejiBehaviorImport &behavior : behaviors) {
            if (behavior.actionName == item.source.name) action.shimejiSource.behaviorNames.append(behavior.name);
        }
        action.systemRole = actions.size() == 1 ? requestedRole : QStringLiteral("None");
        action.fps = 8;
        action.loop = true;
        for (int index = 0; index < item.images.size(); ++index) {
            const QString fileName = QStringLiteral("%1.png").arg(index + 1, 4, 10, QChar('0'));
            const QString stagedPath = QDir(actionStagingDir).filePath(fileName);
            QString frameError;
            if (!saveOnCanvas(item.images.at(index), stagedPath, candidate.canvasSize, &frameError)) {
                result.errorMessage = frameError;
                rollback();
                return result;
            }
            PetFrame frame;
            frame.path = QStringLiteral("assets/%1/%2").arg(actionId, fileName);
            frame.offset = referenceAnchor - item.source.poses.at(index).imageAnchor;
            action.frames.append(frame);
            action.frameDurationsMs.append(qMax(1, item.source.poses.at(index).duration > 0
                                                       ? item.source.poses.at(index).duration
                                                       : 125));
        }
        action.expectedFrameCount = action.frames.size();
        candidate.actions.insert(actionId, action);
        result.actionNameToActionId.insert(item.source.name, actionId);
    }

    result.behaviorsImported = importBehaviorsToProject(candidate, behaviors, result.actionNameToActionId);
    QJsonObject journal {
        {QStringLiteral("phase"), QStringLiteral("committing")},
        {QStringLiteral("petJsonPath"), candidate.petJsonPath()},
        {QStringLiteral("originalPetJson"), QString::fromLatin1(originalPetJson.toBase64())},
        {QStringLiteral("coverPath"), coverPath},
        {QStringLiteral("coverExisted"), coverExisted},
        {QStringLiteral("originalCover"), QString::fromLatin1(originalCover.toBase64())},
        {QStringLiteral("committedActionDirs"), QJsonArray()}
    };
    if (!writeTransactionJournal(QDir(stagingRoot).filePath(QStringLiteral("transaction.json")), journal)) {
        result.errorMessage = QStringLiteral("Failed to create Shimeji transaction journal.");
        rollback();
        return result;
    }
    for (const PreparedAction &item : prepared) {
        const QString finalDir = QDir(candidate.projectDir).filePath(QStringLiteral("assets/%1").arg(item.actionId));
        if (QFileInfo::exists(finalDir)) {
            result.errorMessage = QStringLiteral("Shimeji target action directory already exists: %1").arg(finalDir);
            rollback();
            return result;
        }
        QDir().mkpath(QFileInfo(finalDir).absolutePath());
        if (!transactionFileSystem().renameDirectory(QDir(stagingRoot).filePath(item.actionId), finalDir)) {
            result.errorMessage = QStringLiteral("Failed to commit staged Shimeji action: %1").arg(item.actionId);
            rollback();
            return result;
        }
        committedActionDirs.append(finalDir);
        QJsonArray committed;
        for (const QString &path : committedActionDirs) committed.append(path);
        journal.insert(QStringLiteral("committedActionDirs"), committed);
        if (!writeTransactionJournal(QDir(stagingRoot).filePath(QStringLiteral("transaction.json")), journal)) {
            result.errorMessage = QStringLiteral("Failed to update Shimeji transaction journal.");
            rollback();
            return result;
        }
    }

    if (candidate.coverMode != QStringLiteral("custom") && !transactionFileSystem().updateAutoCover(candidate, &result.errorMessage)) {
        rollback();
        return result;
    }

    QStringList baselineErrors;
    QStringList ignoredBaselineWarnings;
    originalProject.validate(&baselineErrors, &ignoredBaselineWarnings);
    QStringList validationErrors;
    QStringList validationWarnings;
    candidate.validate(&validationErrors, &validationWarnings);
    QStringList introducedErrors;
    for (const QString &validationError : validationErrors) {
        if (!baselineErrors.contains(validationError)) introducedErrors.append(validationError);
    }
    if (!introducedErrors.isEmpty()) {
        result.errorMessage = introducedErrors.join(QStringLiteral("\n"));
        rollback();
        return result;
    }
    if (!transactionFileSystem().saveProject(candidate, &result.errorMessage)) {
        rollback();
        return result;
    }

    journal.insert(QStringLiteral("phase"), QStringLiteral("committed"));
    if (!writeTransactionJournal(QDir(stagingRoot).filePath(QStringLiteral("transaction.json")), journal)) {
        result.errorMessage = QStringLiteral("Failed to finalize Shimeji transaction journal.");
        rollback();
        return result;
    }

    project = candidate;
    result.actionsImported = prepared.size();
    result.warnings.append(validationWarnings);
    if (!transactionFileSystem().removeTree(stagingRoot) && QFileInfo::exists(stagingRoot)) {
        result.warnings.append(QStringLiteral("Imported successfully, but staging cleanup failed: %1").arg(stagingRoot));
    }
    result.ok = true;
    return result;
}

void ShimejiImportWizard::setAssetTransactionFileSystemForTesting(IAssetTransactionFileSystem *fileSystem)
{
    g_transactionFileSystemForTesting = fileSystem;
}

int ShimejiImportWizard::importBehaviorsToProject(PetProject &project,
                                                  const QVector<ShimejiBehaviorImport> &behaviors,
                                                  const QHash<QString, QString> &actionNameToActionId)
{
    int imported = 0;
    for (const ShimejiBehaviorImport &behavior : behaviors) {
        const QString actionId = actionNameToActionId.value(behavior.actionName);
        if (actionId.isEmpty() || !project.actions.contains(actionId)) {
            continue;
        }
        PetBehaviorRule rule;
        rule.id = QStringLiteral("shimeji_%1").arg(behavior.name.toLower());
        rule.id.replace(QRegularExpression(QStringLiteral("[^a-z0-9_-]")), QStringLiteral("_"));
        rule.displayName = behavior.name;
        rule.triggerType = BehaviorTriggerType::RandomIdle;
        rule.actionId = actionId;
        rule.weight = qMax(0.01, behavior.frequency);
        rule.cooldownMs = 0;
        rule.requireOnGround = true;
        rule.requireIdle = true;
        rule.disabledWhileAiBusy = true;
        project.behaviorRules.append(rule);
        ++imported;
    }
    return imported;
}

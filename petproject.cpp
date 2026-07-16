#include "petproject.h"

#include <algorithm>

#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMovie>
#include <QPainter>
#include <QDebug>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <QUuid>
#include <QCryptographicHash>

namespace {

QJsonObject pointToJson(const QPoint &point)
{
    QJsonObject obj;
    obj.insert("x", point.x());
    obj.insert("y", point.y());
    return obj;
}

QPoint pointFromJson(const QJsonObject &obj, const QPoint &fallback = QPoint())
{
    return QPoint(obj.value("x").toInt(fallback.x()), obj.value("y").toInt(fallback.y()));
}

bool validatePngForCanvas(const QString &sourcePath, const QSize &canvasSize, QString *errorMessage)
{
    QImage source(sourcePath);
    if (source.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PNG cannot be read: %1").arg(sourcePath);
        }
        return false;
    }

    if (source.width() > canvasSize.width() || source.height() > canvasSize.height()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PNG %1x%2 is larger than canvas %3x%4: %5")
                                .arg(source.width())
                                .arg(source.height())
                                .arg(canvasSize.width())
                                .arg(canvasSize.height())
                                .arg(sourcePath);
        }
        return false;
    }

    return true;
}

bool copyNormalizedImage(const QImage &source,
                         const QString &targetPath,
                         const QSize &canvasSize,
                         QString *errorMessage)
{
    if (source.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Image cannot be read.");
        }
        return false;
    }

    QImage canvas(canvasSize, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    const int x = (canvasSize.width() - source.width()) / 2;
    const int y = canvasSize.height() - source.height();
    painter.drawImage(QPoint(x, y), source.convertToFormat(QImage::Format_ARGB32_Premultiplied));
    painter.end();

    QDir().mkpath(QFileInfo(targetPath).absolutePath());
    if (!canvas.save(targetPath, "PNG")) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write normalized PNG: %1").arg(targetPath);
        }
        return false;
    }

    return true;
}

bool copyNormalizedPng(const QString &sourcePath,
                       const QString &targetPath,
                       const QSize &canvasSize,
                       QString *errorMessage)
{
    QImage source(sourcePath);
    if (source.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PNG cannot be read: %1").arg(sourcePath);
        }
        return false;
    }

    return copyNormalizedImage(source, targetPath, canvasSize, errorMessage);
}

QRect alphaBoundingRect(const QImage &image)
{
    QRect result;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) == 0) {
                continue;
            }
            const QRect pixelRect(x, y, 1, 1);
            result = result.isNull() ? pixelRect : result.united(pixelRect);
        }
    }
    return result;
}

QVector<PetFrame> syncedFramesFromAssets(const QString &projectDir,
                                         const QString &stateName,
                                         const QVector<PetFrame> &existingFrames)
{
    if (!existingFrames.isEmpty()) {
        return existingFrames;
    }

    const QString assetDirPath = QDir(projectDir).filePath(QStringLiteral("assets/%1").arg(stateName));
    QDir assetDir(assetDirPath);
    const QStringList fileNames = assetDir.entryList(QStringList() << "*.png", QDir::Files, QDir::Name);
    if (fileNames.isEmpty()) {
        return existingFrames;
    }

    QStringList sortedFiles = fileNames;
    std::sort(sortedFiles.begin(), sortedFiles.end(), [](const QString &left, const QString &right) {
        return PetProject::naturalLess(left, right);
    });

    QMap<QString, QPoint> offsetByPath;
    QMap<QString, QPoint> autoOffsetByPath;
    for (const PetFrame &frame : existingFrames) {
        offsetByPath.insert(QDir::cleanPath(frame.path), frame.offset);
        autoOffsetByPath.insert(QDir::cleanPath(frame.path), frame.autoOffset);
    }

    QVector<PetFrame> frames;
    for (const QString &fileName : sortedFiles) {
        PetFrame frame;
        frame.path = QStringLiteral("assets/%1/%2").arg(stateName, fileName);
        frame.offset = offsetByPath.value(QDir::cleanPath(frame.path), QPoint());
        frame.autoOffset = autoOffsetByPath.value(QDir::cleanPath(frame.path), QPoint());
        frames.append(frame);
    }
    return frames;
}

QString quotedPowerShellPath(const QString &path)
{
    QString native = QDir::toNativeSeparators(path);
    native.replace("'", "''");
    return QStringLiteral("'%1'").arg(native);
}

PowerShellArchiveCommandRunner g_defaultArchiveRunner;
IArchiveCommandRunner *g_archiveRunnerForTesting = nullptr;

IArchiveCommandRunner &archiveCommandRunner()
{
    return g_archiveRunnerForTesting ? *g_archiveRunnerForTesting : g_defaultArchiveRunner;
}

bool validatePetpackArchiveEntries(const QString &archivePath, QString *errorMessage)
{
    const QFileInfo archiveInfo(archivePath);
    constexpr qint64 kMaxArchiveBytes = 256LL * 1024 * 1024;
    if (!archiveInfo.exists() || !archiveInfo.isFile() || archiveInfo.size() <= 0
        || archiveInfo.size() > kMaxArchiveBytes) {
        if (errorMessage) *errorMessage = QStringLiteral("Petpack archive size is invalid or exceeds 256 MB.");
        return false;
    }

    const QString command = QStringLiteral(
        "Add-Type -AssemblyName System.IO.Compression; Add-Type -AssemblyName System.IO.Compression.FileSystem; "
        "$a=[System.IO.Compression.ZipFile]::OpenRead(%1); "
        "try { $count=0; [int64]$total=0; "
        "$allowed=@('.json','.png','.gif','.webp','.jpg','.jpeg','.txt','.md'); "
        "foreach($e in $a.Entries){ "
        "$n=$e.FullName.Replace([char]92,[char]47); "
        "if([string]::IsNullOrWhiteSpace($n) -or $n.Length -gt 240 -or $n.StartsWith('/') -or $n -match '^[A-Za-z]:' -or $n.StartsWith('//')){throw 'unsafe archive path'}; "
        "$parts=$n.Split('/'); if($parts.Count -gt 12 -or $parts -contains '..'){throw 'unsafe archive traversal or depth'}; "
        "$mode=($e.ExternalAttributes -shr 16) -band 0xF000; if($mode -eq 0xA000){throw 'symbolic link entry is forbidden'}; "
        "if(-not $n.EndsWith('/')){ $count++; [int64]$total += $e.Length; "
        "if($count -gt 4096 -or $e.Length -gt 67108864 -or $total -gt 536870912){throw 'archive resource limit exceeded'}; "
        "if($e.CompressedLength -gt 0 -and ($e.Length / $e.CompressedLength) -gt 200){throw 'archive compression ratio limit exceeded'}; "
        "$ext=[System.IO.Path]::GetExtension($n).ToLowerInvariant(); if($allowed -notcontains $ext){throw ('forbidden file type: '+$ext)} } } } "
        "finally { $a.Dispose() }")
        .arg(quotedPowerShellPath(archivePath));
    if (!archiveCommandRunner().run(command, errorMessage)) {
        if (errorMessage && !errorMessage->startsWith(QStringLiteral("Petpack"))) {
            *errorMessage = QStringLiteral("Petpack security preflight failed: %1").arg(*errorMessage);
        }
        return false;
    }
    return true;
}

bool validateExtractedPetpackTree(const QString &extractRoot, QString *errorMessage)
{
    const QString root = QDir(extractRoot).canonicalPath();
    if (root.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Petpack extract root is invalid.");
        return false;
    }
    const QString rootPrefix = QDir::cleanPath(root) + QDir::separator();
    qint64 totalBytes = 0;
    int fileCount = 0;
    QDirIterator it(root,
                    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        if (info.isSymbolicLink()
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
            || info.isJunction()
#endif
        ) {
            if (errorMessage) *errorMessage = QStringLiteral("Petpack contains a symbolic link or junction: %1").arg(info.filePath());
            return false;
        }
        const QString canonical = info.canonicalFilePath();
        if (canonical.isEmpty()
            || (canonical != root && !QDir::cleanPath(canonical).startsWith(rootPrefix, Qt::CaseInsensitive))) {
            if (errorMessage) *errorMessage = QStringLiteral("Petpack extracted path escapes its root: %1").arg(info.filePath());
            return false;
        }
        if (info.isFile()) {
            ++fileCount;
            totalBytes += info.size();
            if (fileCount > 4096 || info.size() > 64LL * 1024 * 1024 || totalBytes > 512LL * 1024 * 1024) {
                if (errorMessage) *errorMessage = QStringLiteral("Petpack extracted content exceeds resource limits.");
                return false;
            }
        }
    }
    return true;
}

bool copyRecursively(const QString &sourcePath, const QString &targetPath, QString *errorMessage)
{
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Source path does not exist: %1").arg(sourcePath);
        }
        return false;
    }

    if (sourceInfo.isDir()) {
        QDir sourceDir(sourcePath);
        QDir().mkpath(targetPath);
        const QFileInfoList entries = sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
        for (const QFileInfo &entry : entries) {
            if (!copyRecursively(entry.absoluteFilePath(), QDir(targetPath).filePath(entry.fileName()), errorMessage)) {
                return false;
            }
        }
        return true;
    }

    QDir().mkpath(QFileInfo(targetPath).absolutePath());
    QFile::remove(targetPath);
    if (!QFile::copy(sourcePath, targetPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to copy %1 to %2").arg(sourcePath, targetPath);
        }
        return false;
    }
    return true;
}

QString legacyDisplayNameForAction(const QString &id)
{
    static const QMap<QString, QString> names {
        {QStringLiteral("idle"), QStringLiteral("待机")},
        {QStringLiteral("click"), QStringLiteral("点击反馈")},
        {QStringLiteral("drag"), QStringLiteral("拖拽")},
        {QStringLiteral("drop"), QStringLiteral("落地")},
        {QStringLiteral("walk"), QStringLiteral("走路")},
        {QStringLiteral("walk_left"), QStringLiteral("向左走")},
        {QStringLiteral("walk_right"), QStringLiteral("向右走")},
        {QStringLiteral("sleep"), QStringLiteral("睡觉")},
        {QStringLiteral("look_around"), QStringLiteral("左看右看")},
        {QStringLiteral("wave"), QStringLiteral("挥手")},
        {QStringLiteral("nod"), QStringLiteral("点头")},
        {QStringLiteral("shake_head"), QStringLiteral("摇头")},
        {QStringLiteral("stretch"), QStringLiteral("伸懒腰")},
        {QStringLiteral("jump"), QStringLiteral("跳跃")},
        {QStringLiteral("dance"), QStringLiteral("跳舞")},
        {QStringLiteral("rhythm_step"), QStringLiteral("节奏踏步")},
        {QStringLiteral("peek"), QStringLiteral("侧身探头")},
        {QStringLiteral("look_far"), QStringLiteral("抬手远望")},
        {QStringLiteral("adjust_clothes"), QStringLiteral("整理衣服")},
        {QStringLiteral("hands_behind_sway"), QStringLiteral("双手背后摇摆")},
        {QStringLiteral("listen"), QStringLiteral("侧耳倾听")},
        {QStringLiteral("talk"), QStringLiteral("说话")}
    };
    return names.value(id, id);
}

QString legacySystemRoleForAction(const QString &id)
{
    if (id == QStringLiteral("idle")) return QStringLiteral("Idle");
    if (id == QStringLiteral("walk_left")) return QStringLiteral("WalkLeft");
    if (id == QStringLiteral("walk_right")) return QStringLiteral("WalkRight");
    if (id == QStringLiteral("drag")) return QStringLiteral("Dragging");
    if (id == QStringLiteral("drop")) return QStringLiteral("Falling");
    if (id == QStringLiteral("sleep")) return QStringLiteral("Sleeping");
    if (id == QStringLiteral("talk")) return QStringLiteral("AITalking");
    if (id == QStringLiteral("nod")) return QStringLiteral("AIThinking");
    if (id == QStringLiteral("click")) return QStringLiteral("ClickReaction");
    return QStringLiteral("None");
}

bool legacyContextMenuAction(const QString &id)
{
    return id == QStringLiteral("look_around")
        || id == QStringLiteral("wave")
        || id == QStringLiteral("nod")
        || id == QStringLiteral("shake_head")
        || id == QStringLiteral("stretch");
}

QString actionSourceTypeToString(ActionSourceType type)
{
    switch (type) {
    case ActionSourceType::GifImported: return QStringLiteral("GifImported");
    case ActionSourceType::SpriteSheet: return QStringLiteral("SpriteSheet");
    case ActionSourceType::ProceduralGenerated: return QStringLiteral("ProceduralGenerated");
    case ActionSourceType::ShimejiImported: return QStringLiteral("ShimejiImported");
    case ActionSourceType::PngSequence:
    default:
        return QStringLiteral("PngSequence");
    }
}

ActionSourceType actionSourceTypeFromString(const QString &value)
{
    if (value == QStringLiteral("GifImported")) return ActionSourceType::GifImported;
    if (value == QStringLiteral("SpriteSheet")) return ActionSourceType::SpriteSheet;
    if (value == QStringLiteral("ProceduralGenerated")) return ActionSourceType::ProceduralGenerated;
    if (value == QStringLiteral("ShimejiImported")) return ActionSourceType::ShimejiImported;
    return ActionSourceType::PngSequence;
}

QString behaviorTriggerToString(BehaviorTriggerType type)
{
    switch (type) {
    case BehaviorTriggerType::ManualOnly: return QStringLiteral("ManualOnly");
    case BehaviorTriggerType::ProactiveChatFollowUp: return QStringLiteral("ProactiveChatFollowUp");
    case BehaviorTriggerType::RandomIdle:
    default:
        return QStringLiteral("RandomIdle");
    }
}

BehaviorTriggerType behaviorTriggerFromString(const QString &value)
{
    if (value == QStringLiteral("ManualOnly")) return BehaviorTriggerType::ManualOnly;
    if (value == QStringLiteral("ProactiveChatFollowUp")) return BehaviorTriggerType::ProactiveChatFollowUp;
    return BehaviorTriggerType::RandomIdle;
}

QString safeProjectDirName(const QString &name)
{
    QString cleaned = name.trimmed();
    cleaned.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
    return cleaned.isEmpty() ? QStringLiteral("ImportedPet") : cleaned;
}

QVector<QImage> autoScaleFramesToFit(const QVector<QImage> &frames, const QSize &canvasSize)
{
    if (frames.isEmpty() || !canvasSize.isValid() || canvasSize.isEmpty()) {
        return frames;
    }

    bool needsScale = false;
    double scale = 1.0;
    QSize largestFrame;
    for (const QImage &frame : frames) {
        if (frame.isNull()) {
            continue;
        }

        if (frame.width() > largestFrame.width() || frame.height() > largestFrame.height()) {
            largestFrame = frame.size();
        }

        if (frame.width() > canvasSize.width() || frame.height() > canvasSize.height()) {
            needsScale = true;
            const double scaleX = static_cast<double>(canvasSize.width()) / frame.width();
            const double scaleY = static_cast<double>(canvasSize.height()) / frame.height();
            scale = qMin(scale, qMin(scaleX, scaleY));
        }
    }

    if (!needsScale) {
        return frames;
    }

    QVector<QImage> scaledFrames;
    scaledFrames.reserve(frames.size());
    for (const QImage &frame : frames) {
        if (frame.isNull()) {
            scaledFrames.append(frame);
            continue;
        }

        const int newWidth = qMax(1, qRound(frame.width() * scale));
        const int newHeight = qMax(1, qRound(frame.height() * scale));
        scaledFrames.append(frame.scaled(newWidth, newHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    if (!scaledFrames.isEmpty()) {
        qDebug() << "Auto-scaled GIF frames from max frame" << largestFrame
                 << "to first frame" << scaledFrames.first().size()
                 << "for canvas" << canvasSize;
    }

    return scaledFrames;
}

QVector<QImage> scaleFramesByFactor(const QVector<QImage> &frames, double scale)
{
    scale = qBound(0.1, scale, 4.0);
    if (frames.isEmpty() || qFuzzyCompare(scale, 1.0)) {
        return frames;
    }

    QVector<QImage> scaledFrames;
    scaledFrames.reserve(frames.size());
    for (const QImage &frame : frames) {
        if (frame.isNull()) {
            scaledFrames.append(frame);
            continue;
        }

        const int newWidth = qMax(1, qRound(frame.width() * scale));
        const int newHeight = qMax(1, qRound(frame.height() * scale));
        scaledFrames.append(frame.scaled(newWidth, newHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    if (!scaledFrames.isEmpty()) {
        qDebug() << "Applied GIF import scale" << scale
                 << "from first frame" << frames.first().size()
                 << "to" << scaledFrames.first().size();
    }

    return scaledFrames;
}

QVector<QImage> readGifFramesWithMovie(const QString &gifFile)
{
    QVector<QImage> frames;
    QMovie movie(gifFile);
    if (!movie.isValid()) {
        return frames;
    }

    movie.setCacheMode(QMovie::CacheAll);
    movie.start();

    const int frameCount = movie.frameCount();
    const int maxFrames = frameCount > 0 ? qMin(frameCount, 1000) : 1000;
    QImage previousFrame;

    for (int i = 0; i < maxFrames; ++i) {
        QImage frame = movie.currentImage();
        if (frame.isNull()) {
            break;
        }

        if (previousFrame.isNull() || frame != previousFrame) {
            frames.append(frame);
            previousFrame = frame;
        }

        if (frameCount > 0 && i + 1 >= frameCount) {
            break;
        }
        if (!movie.jumpToNextFrame()) {
            break;
        }
    }

    movie.stop();
    return frames;
}

} // namespace

QStringList PetProject::templateNames()
{
    return {QStringLiteral("simple"), QStringLiteral("basic"), QStringLiteral("extended")};
}

QStringList PetProject::statesForTemplate(const QString &templateName)
{
    if (templateName == QStringLiteral("extended")) {
        return {
            QStringLiteral("idle"),
            QStringLiteral("click"),
            QStringLiteral("drag"),
            QStringLiteral("drop"),
            QStringLiteral("walk"),
            QStringLiteral("walk_left"),
            QStringLiteral("walk_right"),
            QStringLiteral("sleep"),
            QStringLiteral("wake"),
            QStringLiteral("happy"),
            QStringLiteral("sad")
        };
    }

    if (templateName == QStringLiteral("basic")) {
        return {
            QStringLiteral("idle"),
            QStringLiteral("click"),
            QStringLiteral("drag"),
            QStringLiteral("drop"),
            QStringLiteral("walk"),
            QStringLiteral("walk_left"),
            QStringLiteral("walk_right"),
            QStringLiteral("sleep"),
            QStringLiteral("happy")
        };
    }

    return {
        QStringLiteral("idle"),
        QStringLiteral("click"),
        QStringLiteral("drag"),
        QStringLiteral("drop"),
        QStringLiteral("walk"),
        QStringLiteral("walk_left"),
        QStringLiteral("walk_right"),
        QStringLiteral("sleep")
    };
}

PetProject PetProject::createNew(const QString &projectDir, const QString &name, const QString &templateName)
{
    PetProject project;
    project.name = name.trimmed().isEmpty() ? QStringLiteral("Untitled Pet") : name.trimmed();
    project.projectId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    project.templateName = templateName;
    project.projectDir = QDir::cleanPath(projectDir);

    for (const QString &stateName : statesForTemplate(templateName)) {
        PetAction action;
        action.id = stateName;
        action.name = stateName;
        action.displayName = legacyDisplayNameForAction(stateName);
        action.systemRole = legacySystemRoleForAction(stateName);
        action.fps = (stateName == QStringLiteral("click") || stateName == QStringLiteral("drop")) ? 12 : 8;
        action.loop = !(stateName == QStringLiteral("click") || stateName == QStringLiteral("drop"));
        action.next = (stateName == QStringLiteral("click") || stateName == QStringLiteral("drop"))
            ? QStringLiteral("idle")
            : QString();
        action.nextActionId = action.next;
        project.actions.insert(stateName, action);
    }

    QDir().mkpath(QDir(project.projectDir).filePath("assets"));
    return project;
}

bool PetProject::naturalLess(const QString &left, const QString &right)
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    return collator.compare(QFileInfo(left).fileName(), QFileInfo(right).fileName()) < 0;
}

bool PetProject::isValid() const
{
    return !projectDir.isEmpty() && !actions.isEmpty();
}

bool PetProject::load(const QString &petJsonPath, QString *errorMessage)
{
    QFile file(petJsonPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot open project file: %1").arg(petJsonPath);
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid pet.json: %1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = doc.object();
    version = root.value("version").toInt(1);
    schemaVersion = root.value("schemaVersion").toInt(version >= 3 ? version : 1);
    projectId = root.value("projectId").toString();
    name = root.value("name").toString(QStringLiteral("Untitled Pet"));
    templateName = root.value("template").toString(QStringLiteral("simple"));
    projectDir = QFileInfo(petJsonPath).absolutePath();
    coverPath = root.value("cover").toString(QStringLiteral("cover.png"));
    coverMode = root.value("coverMode").toString(QStringLiteral("auto"));

    const QJsonObject canvas = root.value("canvas").toObject();
    canvasSize = QSize(canvas.value("width").toInt(), canvas.value("height").toInt());
    anchor = pointFromJson(root.value("anchor").toObject(), QPoint(canvasSize.width() / 2, canvasSize.height()));

    const QJsonObject runtime = root.value("runtime").toObject();
    scale = qBound(0.1, runtime.value("scale").toDouble(1.0), 8.0);
    mousePassthrough = runtime.value("mousePassthrough").toBool(false);
    locked = runtime.value("locked").toBool(false);
    topMost = runtime.value("topMost").toBool(true);
    invertWalkDirection = runtime.value("invertWalkDirection").toBool(false);
    patrolEnabled = runtime.value("patrolEnabled").toBool(true);
    hasRuntimeAnchorScreen = runtime.contains("anchorScreen");
    runtimeAnchorScreen = pointFromJson(runtime.value("anchorScreen").toObject());

    const QJsonObject ai = root.value("ai").toObject();
    aiCharacterName = ai.value("characterName").toString();
    aiSystemPrompt = ai.value("systemPrompt").toString();
    aiProviderProfileId = ai.value("providerProfileId").toString();

    const QJsonObject render = root.value(QStringLiteral("render")).toObject();
    renderBackend = render.value(QStringLiteral("backend")).toString(QStringLiteral("sprite")).toLower();
    if (renderBackend != QStringLiteral("live2d")) renderBackend = QStringLiteral("sprite");
    live2dModelMetadata = render.value(QStringLiteral("live2dModelMetadata")).toObject();

    actions.clear();
    const QJsonObject actionsObj = root.value("actions").toObject();
    for (const QString &stateName : actionsObj.keys()) {
        const QJsonObject actionObj = actionsObj.value(stateName).toObject();
        PetAction action;
        action.id = stateName;
        action.name = stateName;
        action.displayName = actionObj.value("displayName").toString(legacyDisplayNameForAction(stateName));
        action.englishName = actionObj.value("englishName").toString();
        action.description = actionObj.value("description").toString();
        action.sourceType = actionSourceTypeFromString(actionObj.value("sourceType").toString(QStringLiteral("PngSequence")));
        action.fps = qMax(1, actionObj.value("fps").toInt(8));
        action.loop = actionObj.value("loop").toBool(true);
        action.playCount = qMax(1, actionObj.value("playCount").toInt(1));
        action.nextActionId = actionObj.value("nextActionId").toString(actionObj.value("next").toString());
        action.next = action.nextActionId;
        action.offset = pointFromJson(actionObj.value("offset").toObject());
        action.expectedFrameCount = qMax(0, actionObj.value("frameCount").toInt(0));
        action.spriteSheetRows = qMax(1, actionObj.value("spriteSheetRows").toInt(1));
        action.spriteSheetColumns = qMax(1, actionObj.value("spriteSheetColumns").toInt(1));
        const QJsonObject spriteSource = actionObj.value(QStringLiteral("spriteSheetSource")).toObject();
        action.spriteSheetSource.sourceFileName = QFileInfo(spriteSource.value(QStringLiteral("sourceFileName")).toString()).fileName();
        action.spriteSheetSource.rows = qMax(1, spriteSource.value(QStringLiteral("rows")).toInt(action.spriteSheetRows));
        action.spriteSheetSource.columns = qMax(1, spriteSource.value(QStringLiteral("columns")).toInt(action.spriteSheetColumns));
        action.spriteSheetSource.frameWidth = qMax(0, spriteSource.value(QStringLiteral("frameWidth")).toInt());
        action.spriteSheetSource.frameHeight = qMax(0, spriteSource.value(QStringLiteral("frameHeight")).toInt());
        action.spriteSheetSource.marginLeft = qMax(0, spriteSource.value(QStringLiteral("marginLeft")).toInt());
        action.spriteSheetSource.marginTop = qMax(0, spriteSource.value(QStringLiteral("marginTop")).toInt());
        action.spriteSheetSource.marginRight = qMax(0, spriteSource.value(QStringLiteral("marginRight")).toInt());
        action.spriteSheetSource.marginBottom = qMax(0, spriteSource.value(QStringLiteral("marginBottom")).toInt());
        action.spriteSheetSource.spacingX = qMax(0, spriteSource.value(QStringLiteral("spacingX")).toInt());
        action.spriteSheetSource.spacingY = qMax(0, spriteSource.value(QStringLiteral("spacingY")).toInt());
        action.spriteSheetSource.readingOrder = spriteSource.value(QStringLiteral("readingOrder")).toString(QStringLiteral("LeftToRightTopToBottom"));
        action.spriteSheetSource.startFrame = qMax(0, spriteSource.value(QStringLiteral("startFrame")).toInt());
        action.spriteSheetSource.maxFrames = qMax(0, spriteSource.value(QStringLiteral("maxFrames")).toInt());
        action.spriteSheetSource.skipTransparentFrames = spriteSource.value(QStringLiteral("skipTransparentFrames")).toBool(false);
        const QJsonObject shimejiSource = actionObj.value(QStringLiteral("shimejiSource")).toObject();
        action.shimejiSource.originalActionName = shimejiSource.value(QStringLiteral("originalActionName")).toString();
        action.shimejiSource.originalType = shimejiSource.value(QStringLiteral("originalType")).toString();
        action.shimejiSource.borderType = shimejiSource.value(QStringLiteral("borderType")).toString();
        for (const QJsonValue &value : shimejiSource.value(QStringLiteral("unsupportedConditions")).toArray())
            action.shimejiSource.unsupportedConditions.append(value.toString());
        for (const QJsonValue &value : shimejiSource.value(QStringLiteral("behaviorNames")).toArray())
            action.shimejiSource.behaviorNames.append(value.toString());
        action.previewScale = actionObj.value("previewScale").toDouble(1.0);
        action.footBaselineOffset = actionObj.value("footBaselineOffset").toInt(0);
        action.allowHorizontalDisplacement = actionObj.value("allowHorizontalDisplacement").toBool(false);
        action.mirrorSupported = actionObj.value("mirrorSupported").toBool(false);
        action.defaultPrompt = actionObj.value("defaultPrompt").toString();
        action.negativePrompt = actionObj.value("negativePrompt").toString();
        action.systemRole = actionObj.value("systemRole").toString(legacySystemRoleForAction(stateName));
        action.showInContextMenu = actionObj.value("showInContextMenu").toBool(legacyContextMenuAction(stateName));
        action.menuGroup = actionObj.value("menuGroup").toString(legacyContextMenuAction(stateName) ? QStringLiteral("互动动作") : QString());
        action.allowAiTrigger = actionObj.value("allowAiTrigger").toBool(legacyContextMenuAction(stateName));
        const QJsonArray allowedStatesArray = actionObj.value(QStringLiteral("aiAllowedStates")).toArray();
        for (const QJsonValue &stateValue : allowedStatesArray) {
            const QString allowedState = stateValue.toString().trimmed();
            if (!allowedState.isEmpty()) action.aiAllowedStates.append(allowedState);
        }
        action.aiAllowedStates.removeDuplicates();
        if (allowedStatesArray.isEmpty()) {
            action.aiAllowedStates = {QStringLiteral("Normal")};
        }
        action.aiParameterSchema = actionObj.value(QStringLiteral("aiParameterSchema")).toObject();
        action.allowRandomTrigger = actionObj.value("allowRandomTrigger").toBool(legacyContextMenuAction(stateName));
        action.randomWeight = actionObj.value("randomWeight").toDouble(1.0);
        action.randomCooldownMs = actionObj.value("randomCooldownMs").toInt(0);
        const QJsonArray durationsArray = actionObj.value("frameDurationsMs").toArray();
        for (const QJsonValue &durationValue : durationsArray) {
            action.frameDurationsMs.append(qMax(1, durationValue.toInt()));
        }
        const QJsonObject eventsObj = actionObj.value("events").toObject();
        for (const QString &eventName : eventsObj.keys()) {
            const QString targetState = eventsObj.value(eventName).toString();
            if (!eventName.isEmpty() && !targetState.isEmpty()) {
                action.events.insert(eventName, targetState);
            }
        }

        const QJsonArray framesArray = actionObj.value("frames").toArray();
        for (const QJsonValue &frameValue : framesArray) {
            const QJsonObject frameObj = frameValue.toObject();
            PetFrame frame;
            frame.path = frameObj.value("path").toString();
            frame.offset = pointFromJson(frameObj.value("offset").toObject());
            frame.autoOffset = pointFromJson(frameObj.value("autoOffset").toObject());
            if (!frame.path.isEmpty()) {
                action.frames.append(frame);
            }
        }
        if (schemaVersion < 3) {
            action.frames = syncedFramesFromAssets(projectDir, stateName, action.frames);
        }
        actions.insert(stateName, action);
    }

    if (schemaVersion < 3 && actions.isEmpty()) {
        for (const QString &stateName : statesForTemplate(templateName)) {
            PetAction action;
            action.id = stateName;
            action.name = stateName;
            action.displayName = legacyDisplayNameForAction(stateName);
            action.systemRole = legacySystemRoleForAction(stateName);
            actions.insert(stateName, action);
        }
    }
    if (schemaVersion < 3) {
        for (const QString &stateName : statesForTemplate(templateName)) {
            if (!actions.contains(stateName)) {
                PetAction action;
                action.id = stateName;
                action.name = stateName;
                action.displayName = legacyDisplayNameForAction(stateName);
                action.systemRole = legacySystemRoleForAction(stateName);
                actions.insert(action.name, action);
            }
        }
    }

    behaviorRules.clear();
    const QJsonArray behaviorArray = root.value(QStringLiteral("behaviorRules")).toArray();
    for (const QJsonValue &value : behaviorArray) {
        const QJsonObject obj = value.toObject();
        PetBehaviorRule rule;
        rule.id = obj.value(QStringLiteral("id")).toString();
        rule.displayName = obj.value(QStringLiteral("displayName")).toString();
        rule.triggerType = behaviorTriggerFromString(obj.value(QStringLiteral("triggerType")).toString());
        rule.actionId = obj.value(QStringLiteral("actionId")).toString();
        rule.weight = obj.value(QStringLiteral("weight")).toDouble(1.0);
        rule.cooldownMs = obj.value(QStringLiteral("cooldownMs")).toInt(0);
        rule.requireOnGround = obj.value(QStringLiteral("requireOnGround")).toBool(true);
        rule.requireIdle = obj.value(QStringLiteral("requireIdle")).toBool(true);
        rule.disabledWhileAiBusy = obj.value(QStringLiteral("disabledWhileAiBusy")).toBool(true);
        if (!rule.id.isEmpty() && !rule.actionId.isEmpty()) {
            behaviorRules.append(rule);
        }
    }

    return true;
}

bool removeDirectoryChecked(const QString &path, QString *errorMessage)
{
    QDir dir(path);
    if (!dir.exists()) {
        return true;
    }
    if (!dir.removeRecursively()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to remove directory: %1").arg(path);
        }
        return false;
    }
    return true;
}

bool renameDirectoryChecked(const QString &from, const QString &to, QString *errorMessage)
{
    if (!QFileInfo::exists(from)) {
        return true;
    }
    QDir().mkpath(QFileInfo(to).absolutePath());
    QFile::remove(to);
    removeDirectoryChecked(to, nullptr);
    if (!QDir().rename(from, to)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to rename %1 to %2").arg(from, to);
        }
        return false;
    }
    return true;
}

QByteArray readFileIfExists(const QString &path, bool *exists)
{
    *exists = QFileInfo::exists(path);
    if (!*exists) {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

bool restoreFileSnapshot(const QString &path, bool existed, const QByteArray &data, QString *errorMessage)
{
    if (!existed) {
        if (QFileInfo::exists(path) && !QFile::remove(path)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Failed to remove rollback file: %1").arg(path);
            }
            return false;
        }
        return true;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open rollback file: %1").arg(path);
        }
        return false;
    }
    if (file.write(data) != data.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write rollback file: %1").arg(path);
        }
        return false;
    }
    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to commit rollback file: %1").arg(path);
        }
        return false;
    }
    return true;
}

bool validateStagedPngDir(const QString &dirPath, int expectedFrames, QString *errorMessage)
{
    QDir dir(dirPath);
    const QStringList files = dir.entryList(QStringList() << QStringLiteral("*.png"), QDir::Files, QDir::Name);
    if (files.size() != expectedFrames) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Staged frame count mismatch: expected %1 got %2").arg(expectedFrames).arg(files.size());
        }
        return false;
    }
    for (const QString &file : files) {
        QImage image(dir.filePath(file));
        if (image.isNull()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Staged frame is unreadable: %1").arg(dir.filePath(file));
            }
            return false;
        }
    }
    return true;
}

bool PetProject::isLegalActionId(const QString &id)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_-]+$"));
    return !id.isEmpty() && pattern.match(id).hasMatch();
}

void PetProject::ensureProjectId()
{
    if (projectId.trimmed().isEmpty()) {
        projectId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
}

bool PetProject::save(QString *errorMessage)
{
    if (projectDir.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Project directory is empty.");
        }
        return false;
    }

    QDir().mkpath(projectDir);
    ensureProjectId();

    QJsonObject root;
    root.insert("version", version);
    root.insert("schemaVersion", 3);
    root.insert("projectId", projectId);
    root.insert("name", name);
    root.insert("template", templateName);
    root.insert("cover", coverPath);
    root.insert("coverMode", coverMode);

    QJsonObject canvas;
    canvas.insert("width", canvasSize.width());
    canvas.insert("height", canvasSize.height());
    root.insert("canvas", canvas);
    root.insert("anchor", pointToJson(anchor));

    QJsonObject runtime;
    runtime.insert("scale", scale);
    runtime.insert("mousePassthrough", mousePassthrough);
    runtime.insert("locked", locked);
    runtime.insert("topMost", topMost);
    runtime.insert("invertWalkDirection", invertWalkDirection);
    runtime.insert("patrolEnabled", patrolEnabled);
    if (hasRuntimeAnchorScreen) {
        runtime.insert("anchorScreen", pointToJson(runtimeAnchorScreen));
    }
    root.insert("runtime", runtime);

    QJsonObject ai;
    ai.insert("characterName", aiCharacterName);
    ai.insert("systemPrompt", aiSystemPrompt);
    ai.insert("providerProfileId", aiProviderProfileId);
    root.insert("ai", ai);

    QJsonObject render;
    render.insert(QStringLiteral("backend"), renderBackend == QStringLiteral("live2d")
                                                   ? QStringLiteral("live2d") : QStringLiteral("sprite"));
    render.insert(QStringLiteral("live2dModelMetadata"), live2dModelMetadata);
    root.insert(QStringLiteral("render"), render);

    QJsonObject actionsObj;
    for (auto it = actions.constBegin(); it != actions.constEnd(); ++it) {
        const PetAction &action = it.value();
        Q_ASSERT(action.id.isEmpty() || action.id == it.key());
        QJsonObject actionObj;
        actionObj.insert("id", it.key());
        actionObj.insert("displayName", action.displayName.isEmpty() ? legacyDisplayNameForAction(it.key()) : action.displayName);
        actionObj.insert("englishName", action.englishName);
        actionObj.insert("description", action.description);
        actionObj.insert("sourceType", actionSourceTypeToString(action.sourceType));
        actionObj.insert("fps", action.fps);
        actionObj.insert("loop", action.loop);
        actionObj.insert("playCount", action.playCount);
        actionObj.insert("next", action.nextActionId);
        actionObj.insert("nextActionId", action.nextActionId);
        actionObj.insert("offset", pointToJson(action.offset));
        actionObj.insert("frameCount", action.expectedFrameCount);
        actionObj.insert("spriteSheetRows", action.spriteSheetRows);
        actionObj.insert("spriteSheetColumns", action.spriteSheetColumns);
        QJsonObject spriteSource;
        spriteSource.insert(QStringLiteral("sourceFileName"), QFileInfo(action.spriteSheetSource.sourceFileName).fileName());
        spriteSource.insert(QStringLiteral("rows"), action.spriteSheetSource.rows);
        spriteSource.insert(QStringLiteral("columns"), action.spriteSheetSource.columns);
        spriteSource.insert(QStringLiteral("frameWidth"), action.spriteSheetSource.frameWidth);
        spriteSource.insert(QStringLiteral("frameHeight"), action.spriteSheetSource.frameHeight);
        spriteSource.insert(QStringLiteral("marginLeft"), action.spriteSheetSource.marginLeft);
        spriteSource.insert(QStringLiteral("marginTop"), action.spriteSheetSource.marginTop);
        spriteSource.insert(QStringLiteral("marginRight"), action.spriteSheetSource.marginRight);
        spriteSource.insert(QStringLiteral("marginBottom"), action.spriteSheetSource.marginBottom);
        spriteSource.insert(QStringLiteral("spacingX"), action.spriteSheetSource.spacingX);
        spriteSource.insert(QStringLiteral("spacingY"), action.spriteSheetSource.spacingY);
        spriteSource.insert(QStringLiteral("readingOrder"), action.spriteSheetSource.readingOrder);
        spriteSource.insert(QStringLiteral("startFrame"), action.spriteSheetSource.startFrame);
        spriteSource.insert(QStringLiteral("maxFrames"), action.spriteSheetSource.maxFrames);
        spriteSource.insert(QStringLiteral("skipTransparentFrames"), action.spriteSheetSource.skipTransparentFrames);
        actionObj.insert(QStringLiteral("spriteSheetSource"), spriteSource);
        QJsonObject shimejiSource;
        shimejiSource.insert(QStringLiteral("originalActionName"), action.shimejiSource.originalActionName);
        shimejiSource.insert(QStringLiteral("originalType"), action.shimejiSource.originalType);
        shimejiSource.insert(QStringLiteral("borderType"), action.shimejiSource.borderType);
        shimejiSource.insert(QStringLiteral("unsupportedConditions"), QJsonArray::fromStringList(action.shimejiSource.unsupportedConditions));
        shimejiSource.insert(QStringLiteral("behaviorNames"), QJsonArray::fromStringList(action.shimejiSource.behaviorNames));
        actionObj.insert(QStringLiteral("shimejiSource"), shimejiSource);
        actionObj.insert("previewScale", action.previewScale);
        actionObj.insert("footBaselineOffset", action.footBaselineOffset);
        actionObj.insert("allowHorizontalDisplacement", action.allowHorizontalDisplacement);
        actionObj.insert("mirrorSupported", action.mirrorSupported);
        actionObj.insert("defaultPrompt", action.defaultPrompt);
        actionObj.insert("negativePrompt", action.negativePrompt);
        actionObj.insert("systemRole", action.systemRole.isEmpty() ? legacySystemRoleForAction(it.key()) : action.systemRole);
        actionObj.insert("showInContextMenu", action.showInContextMenu);
        actionObj.insert("menuGroup", action.menuGroup);
        actionObj.insert("allowAiTrigger", action.allowAiTrigger);
        QJsonArray aiAllowedStates;
        for (const QString &allowedState : action.aiAllowedStates) aiAllowedStates.append(allowedState);
        actionObj.insert(QStringLiteral("aiAllowedStates"), aiAllowedStates);
        actionObj.insert(QStringLiteral("aiParameterSchema"), action.aiParameterSchema);
        actionObj.insert("allowRandomTrigger", action.allowRandomTrigger);
        actionObj.insert("randomWeight", action.randomWeight);
        actionObj.insert("randomCooldownMs", action.randomCooldownMs);
        QJsonArray durationsArray;
        for (int duration : action.frameDurationsMs) {
            durationsArray.append(duration);
        }
        actionObj.insert("frameDurationsMs", durationsArray);
        QJsonObject eventsObj;
        for (auto eventIt = action.events.constBegin(); eventIt != action.events.constEnd(); ++eventIt) {
            eventsObj.insert(eventIt.key(), eventIt.value());
        }
        actionObj.insert("events", eventsObj);

        QJsonArray framesArray;
        for (const PetFrame &frame : action.frames) {
            QJsonObject frameObj;
            frameObj.insert("path", frame.path);
            frameObj.insert("offset", pointToJson(frame.offset));
            if (!frame.autoOffset.isNull()) {
                frameObj.insert("autoOffset", pointToJson(frame.autoOffset));
            }
            framesArray.append(frameObj);
        }
        actionObj.insert("frames", framesArray);
        actionsObj.insert(it.key(), actionObj);
    }
    root.insert("actions", actionsObj);

    QJsonArray behaviorArray;
    for (const PetBehaviorRule &rule : behaviorRules) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), rule.id);
        obj.insert(QStringLiteral("displayName"), rule.displayName);
        obj.insert(QStringLiteral("triggerType"), behaviorTriggerToString(rule.triggerType));
        obj.insert(QStringLiteral("actionId"), rule.actionId);
        obj.insert(QStringLiteral("weight"), rule.weight);
        obj.insert(QStringLiteral("cooldownMs"), rule.cooldownMs);
        obj.insert(QStringLiteral("requireOnGround"), rule.requireOnGround);
        obj.insert(QStringLiteral("requireIdle"), rule.requireIdle);
        obj.insert(QStringLiteral("disabledWhileAiBusy"), rule.disabledWhileAiBusy);
        behaviorArray.append(obj);
    }
    root.insert(QStringLiteral("behaviorRules"), behaviorArray);

    QSaveFile file(petJsonPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot write pet.json: %1").arg(file.fileName());
        }
        return false;
    }

    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot write all data to pet.json: %1").arg(file.fileName());
        }
        return false;
    }
    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot commit pet.json: %1").arg(file.fileName());
        }
        return false;
    }
    return true;
}

bool PetProject::saveRuntimeStatePatch(const RuntimeStatePatch &patch, QString *errorMessage) const
{
    const QString path = petJsonPath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot open project file for runtime patch: %1").arg(path);
        }
        return false;
    }

    const QByteArray currentData = file.readAll();
    file.close();
    QJsonParseError parseError;
    const QJsonDocument currentDoc = QJsonDocument::fromJson(currentData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !currentDoc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid project JSON for runtime patch: %1").arg(parseError.errorString());
        }
        return false;
    }

    QJsonObject root = currentDoc.object();
    QJsonObject runtime = root.value(QStringLiteral("runtime")).toObject();
    if (patch.hasMousePassthrough) {
        runtime.insert(QStringLiteral("mousePassthrough"), patch.mousePassthrough);
    }
    if (patch.hasLocked) {
        runtime.insert(QStringLiteral("locked"), patch.locked);
    }
    if (patch.hasTopMost) {
        runtime.insert(QStringLiteral("topMost"), patch.topMost);
    }
    if (patch.hasPatrolEnabled) {
        runtime.insert(QStringLiteral("patrolEnabled"), patch.patrolEnabled);
    }
    if (patch.clearAnchorScreen) {
        runtime.remove(QStringLiteral("anchorScreen"));
    } else if (patch.hasAnchorScreen) {
        runtime.insert(QStringLiteral("anchorScreen"), pointToJson(patch.anchorScreen));
    }
    root.insert(QStringLiteral("runtime"), runtime);

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot write runtime patch: %1").arg(path);
        }
        return false;
    }

    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (out.write(data) != data.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot write all runtime patch data: %1").arg(path);
        }
        return false;
    }
    if (!out.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot commit runtime patch: %1").arg(path);
        }
        return false;
    }
    return true;
}

bool PetProject::importPngFrames(const QString &stateName, const QStringList &sourceFiles, QString *errorMessage)
{
    return importPngFrames(stateName, sourceFiles, ImportFramesOptions {}, errorMessage);
}

bool PetProject::importPngFrames(const QString &stateName,
                                 const QStringList &sourceFiles,
                                 const ImportFramesOptions &options,
                                 QString *errorMessage)
{
    if (sourceFiles.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PNG import file list is empty.");
        }
        return false;
    }

    const PetProject originalProject = *this;
    const QString cleanedState = stateName.trimmed();
    if (!isLegalActionId(cleanedState)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid action id: %1").arg(stateName);
        }
        return false;
    }
    QStringList sortedFiles = sourceFiles;
    std::sort(sortedFiles.begin(), sortedFiles.end(), PetProject::naturalLess);

    if (!actions.contains(cleanedState)) {
        PetAction action;
        action.id = cleanedState;
        action.name = cleanedState;
        actions.insert(cleanedState, action);
    }

    QImage first(sortedFiles.first());
    if (first.isNull()) {
        *this = originalProject;
        if (errorMessage) {
            *errorMessage = QStringLiteral("First PNG cannot be read: %1").arg(sortedFiles.first());
        }
        return false;
    }

    if (!canvasSize.isValid() || canvasSize.isEmpty()) {
        if (actions.value(cleanedState).systemRole != systemRoleName(SystemActionRole::Idle)) {
            *this = originalProject;
            if (errorMessage) {
                *errorMessage = QStringLiteral("Import an Idle role action first so its first frame can initialize the canvas.");
            }
            return false;
        }
        canvasSize = first.size();
        anchor = QPoint(canvasSize.width() / 2, canvasSize.height());
    }

    for (const QString &sourcePath : sortedFiles) {
        if (!validatePngForCanvas(sourcePath, canvasSize, errorMessage)) {
            *this = originalProject;
            return false;
        }
    }

    const QString transactionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString stagingRoot = QDir(projectDir).filePath(QStringLiteral(".asset_staging/%1").arg(transactionId));
    const QString stagedActionDir = QDir(stagingRoot).filePath(cleanedState);
    const QString stateAssetDir = QDir(projectDir).filePath(QStringLiteral("assets/%1").arg(cleanedState));
    const QString backupDir = QDir(projectDir).filePath(QStringLiteral(".asset_staging/%1_backup_%2").arg(cleanedState, transactionId));

    QVector<PetFrame> importedFrames;
    int index = 1;
    for (const QString &sourcePath : sortedFiles) {
        const QString fileName = QStringLiteral("%1.png").arg(index, 4, 10, QChar('0'));
        const QString stagedPath = QDir(stagedActionDir).filePath(fileName);
        if (!copyNormalizedPng(sourcePath, stagedPath, canvasSize, errorMessage)) {
            *this = originalProject;
            removeDirectoryChecked(stagingRoot, nullptr);
            return false;
        }

        PetFrame frame;
        frame.path = QStringLiteral("assets/%1/%2").arg(cleanedState, fileName);
        importedFrames.append(frame);
        ++index;
    }
    if (!validateStagedPngDir(stagedActionDir, importedFrames.size(), errorMessage)) {
        *this = originalProject;
        removeDirectoryChecked(stagingRoot, nullptr);
        return false;
    }

    actions[cleanedState].frames = importedFrames;
    actions[cleanedState].sourceType = options.sourceType;
    actions[cleanedState].frameDurationsMs = options.frameDurationsMs;
    if (options.hasSpriteSheetMetadata) {
        actions[cleanedState].spriteSheetSource = options.spriteSheetMetadata;
        actions[cleanedState].spriteSheetRows = options.spriteSheetMetadata.rows;
        actions[cleanedState].spriteSheetColumns = options.spriteSheetMetadata.columns;
    }
    actions[cleanedState].id = cleanedState;
    for (int i = 0; i < actions[cleanedState].frames.size() && i < options.frameOffsets.size(); ++i) {
        actions[cleanedState].frames[i].offset = options.frameOffsets.at(i);
    }

    bool liveMoved = false;
    bool stagedMoved = false;
    bool coverExisted = false;
    const QString coverSnapshotPath = absolutePathFor(QStringLiteral("cover.png"));
    const QByteArray coverSnapshot = readFileIfExists(coverSnapshotPath, &coverExisted);
    if (!renameDirectoryChecked(stateAssetDir, backupDir, errorMessage)) {
        *this = originalProject;
        removeDirectoryChecked(stagingRoot, nullptr);
        return false;
    }
    liveMoved = QFileInfo::exists(backupDir);
    if (!renameDirectoryChecked(stagedActionDir, stateAssetDir, errorMessage)) {
        if (liveMoved) { renameDirectoryChecked(backupDir, stateAssetDir, nullptr); }
        *this = originalProject;
        removeDirectoryChecked(stagingRoot, nullptr);
        return false;
    }
    stagedMoved = true;

    if (actions.value(cleanedState).systemRole == systemRoleName(SystemActionRole::Idle) && coverMode == QStringLiteral("auto")) {
        if (!updateAutoCover(errorMessage)) {
            if (stagedMoved) { removeDirectoryChecked(stateAssetDir, nullptr); }
            if (liveMoved) { renameDirectoryChecked(backupDir, stateAssetDir, nullptr); }
            QString rollbackError;
            if (!restoreFileSnapshot(coverSnapshotPath, coverExisted, coverSnapshot, &rollbackError) && errorMessage) {
                *errorMessage += QStringLiteral("; cover rollback failed: %1").arg(rollbackError);
            }
            *this = originalProject;
            removeDirectoryChecked(stagingRoot, nullptr);
            return false;
        }
    }

    if (!save(errorMessage)) {
        if (stagedMoved) { removeDirectoryChecked(stateAssetDir, nullptr); }
        if (liveMoved) { renameDirectoryChecked(backupDir, stateAssetDir, nullptr); }
        QString rollbackError;
        if (!restoreFileSnapshot(coverSnapshotPath, coverExisted, coverSnapshot, &rollbackError) && errorMessage) {
            *errorMessage += QStringLiteral("; cover rollback failed: %1").arg(rollbackError);
        }
        *this = originalProject;
        removeDirectoryChecked(stagingRoot, nullptr);
        return false;
    }

    removeDirectoryChecked(backupDir, nullptr);
    removeDirectoryChecked(stagingRoot, nullptr);
    return true;
}

bool PetProject::importGifFrames(const QString &stateName,
                                 const QString &gifFile,
                                 double importScale,
                                 QString *errorMessage)
{
    const PetProject originalProject = *this;
    if (gifFile.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("GIF file path is empty.");
        }
        return false;
    }

    QVector<QImage> frames = readGifFramesWithMovie(gifFile);
    if (frames.isEmpty()) {
        QImageReader reader(gifFile);
        reader.setAutoDetectImageFormat(true);
        QImage frame = reader.read();
        if (!frame.isNull()) {
            frames.append(frame);
        }
    }

    if (frames.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("GIF cannot be read: %1").arg(gifFile);
        }
        return false;
    }

    frames = scaleFramesByFactor(frames, importScale);

    const QString cleanedState = stateName.trimmed();
    if (!isLegalActionId(cleanedState)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid action id: %1").arg(stateName);
        }
        return false;
    }
    if (!actions.contains(cleanedState)) {
        PetAction action;
        action.id = cleanedState;
        action.name = cleanedState;
        actions.insert(cleanedState, action);
    }

    if (!canvasSize.isValid() || canvasSize.isEmpty()) {
        if (actions.value(cleanedState).systemRole != systemRoleName(SystemActionRole::Idle)) {
            *this = originalProject;
            if (errorMessage) {
                *errorMessage = QStringLiteral("Import an Idle role action first so its first frame can initialize the canvas.");
            }
            return false;
        }
        canvasSize = frames.first().size();
        anchor = QPoint(canvasSize.width() / 2, canvasSize.height());
    }

    const int originalFrameCount = frames.size();
    frames = autoScaleFramesToFit(frames, canvasSize);
    if (frames.size() != originalFrameCount) {
        qWarning() << "GIF frame count changed during auto-scaling:" << originalFrameCount << "->" << frames.size();
    }

    const QString transactionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString stagingRoot = QDir(projectDir).filePath(QStringLiteral(".asset_staging/%1").arg(transactionId));
    const QString stagedActionDir = QDir(stagingRoot).filePath(cleanedState);
    const QString stateAssetDir = QDir(projectDir).filePath(QStringLiteral("assets/%1").arg(cleanedState));
    const QString backupDir = QDir(projectDir).filePath(QStringLiteral(".asset_staging/%1_backup_%2").arg(cleanedState, transactionId));

    QVector<PetFrame> importedFrames;
    int index = 1;
    for (const QImage &frameImage : frames) {
        const QString fileName = QStringLiteral("%1.png").arg(index, 4, 10, QChar('0'));
        const QString stagedPath = QDir(stagedActionDir).filePath(fileName);
        if (!copyNormalizedImage(frameImage, stagedPath, canvasSize, errorMessage)) {
            *this = originalProject;
            removeDirectoryChecked(stagingRoot, nullptr);
            return false;
        }

        PetFrame frame;
        frame.path = QStringLiteral("assets/%1/%2").arg(cleanedState, fileName);
        importedFrames.append(frame);
        ++index;
    }
    if (!validateStagedPngDir(stagedActionDir, importedFrames.size(), errorMessage)) {
        *this = originalProject;
        removeDirectoryChecked(stagingRoot, nullptr);
        return false;
    }

    actions[cleanedState].frames = importedFrames;
    actions[cleanedState].sourceType = ActionSourceType::GifImported;
    actions[cleanedState].id = cleanedState;
    qDebug() << "Imported" << frames.size() << "GIF frame(s) for state" << cleanedState
             << "within canvas" << canvasSize;

    bool liveMoved = false;
    bool stagedMoved = false;
    bool coverExisted = false;
    const QString coverSnapshotPath = absolutePathFor(QStringLiteral("cover.png"));
    const QByteArray coverSnapshot = readFileIfExists(coverSnapshotPath, &coverExisted);
    if (!renameDirectoryChecked(stateAssetDir, backupDir, errorMessage)) {
        *this = originalProject;
        removeDirectoryChecked(stagingRoot, nullptr);
        return false;
    }
    liveMoved = QFileInfo::exists(backupDir);
    if (!renameDirectoryChecked(stagedActionDir, stateAssetDir, errorMessage)) {
        if (liveMoved) { renameDirectoryChecked(backupDir, stateAssetDir, nullptr); }
        *this = originalProject;
        removeDirectoryChecked(stagingRoot, nullptr);
        return false;
    }
    stagedMoved = true;

    if (actions.value(cleanedState).systemRole == systemRoleName(SystemActionRole::Idle) && coverMode == QStringLiteral("auto")) {
        if (!updateAutoCover(errorMessage)) {
            if (stagedMoved) { removeDirectoryChecked(stateAssetDir, nullptr); }
            if (liveMoved) { renameDirectoryChecked(backupDir, stateAssetDir, nullptr); }
            QString rollbackError;
            if (!restoreFileSnapshot(coverSnapshotPath, coverExisted, coverSnapshot, &rollbackError) && errorMessage) {
                *errorMessage += QStringLiteral("; cover rollback failed: %1").arg(rollbackError);
            }
            *this = originalProject;
            removeDirectoryChecked(stagingRoot, nullptr);
            return false;
        }
    }

    if (!save(errorMessage)) {
        if (stagedMoved) { removeDirectoryChecked(stateAssetDir, nullptr); }
        if (liveMoved) { renameDirectoryChecked(backupDir, stateAssetDir, nullptr); }
        QString rollbackError;
        if (!restoreFileSnapshot(coverSnapshotPath, coverExisted, coverSnapshot, &rollbackError) && errorMessage) {
            *errorMessage += QStringLiteral("; cover rollback failed: %1").arg(rollbackError);
        }
        *this = originalProject;
        removeDirectoryChecked(stagingRoot, nullptr);
        return false;
    }

    removeDirectoryChecked(backupDir, nullptr);
    removeDirectoryChecked(stagingRoot, nullptr);
    return true;
}

bool PetProject::updateAutoCover(QString *errorMessage)
{
    const QString idleId = actionForRole(SystemActionRole::Idle, QStringLiteral("idle"));
    const PetAction idleAction = actions.value(idleId);
    if (idleAction.frames.isEmpty()) {
        return true;
    }

    const QString firstFrame = absolutePathFor(idleAction.frames.first().path);
    QImage image(firstFrame);
    if (image.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot read idle first frame for cover: %1").arg(firstFrame);
        }
        return false;
    }

    coverPath = QStringLiteral("cover.png");
    coverMode = QStringLiteral("auto");
    if (!image.save(absolutePathFor(coverPath), "PNG")) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot write auto cover: %1").arg(absolutePathFor(coverPath));
        }
        return false;
    }

    return true;
}

bool PetProject::setCustomCover(const QString &sourceFile, QString *errorMessage)
{
    QImage image(sourceFile);
    if (image.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cover image cannot be read: %1").arg(sourceFile);
        }
        return false;
    }

    coverPath = QStringLiteral("cover_custom.png");
    coverMode = QStringLiteral("custom");
    if (!image.save(absolutePathFor(coverPath), "PNG")) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot write custom cover: %1").arg(absolutePathFor(coverPath));
        }
        return false;
    }

    return save(errorMessage);
}

QPoint PetProject::suggestedAnchorFromIdle() const
{
    const PetAction idleAction = actions.value(actionForRole(SystemActionRole::Idle, QStringLiteral("idle")));
    if (idleAction.frames.isEmpty() || !canvasSize.isValid()) {
        return anchor;
    }

    QImage image(absolutePathFor(idleAction.frames.first().path));
    if (image.isNull()) {
        return anchor;
    }

    const QRect bbox = alphaBoundingRect(image);
    if (bbox.isNull()) {
        return anchor;
    }

    return QPoint(bbox.center().x(), bbox.bottom() + 1);
}

bool PetProject::validate(QStringList *errors, QStringList *warnings) const
{
    QStringList localErrors;
    QStringList localWarnings;

    if (version < 1 || version > 1) {
        localErrors.append(QStringLiteral("Unsupported project version: %1").arg(version));
    }
    if (projectDir.isEmpty()) {
        localErrors.append(QStringLiteral("Project directory is empty."));
    }
    if (!QFileInfo::exists(petJsonPath())) {
        localErrors.append(QStringLiteral("Missing pet.json: %1").arg(petJsonPath()));
    }
    if (!canvasSize.isValid() || canvasSize.isEmpty()) {
        localErrors.append(QStringLiteral("Canvas size is not initialized."));
    }
    if (actions.isEmpty()) {
        localErrors.append(QStringLiteral("No actions found in project."));
    }

    const QString idleAction = actionForRole(SystemActionRole::Idle);
    if (idleAction.isEmpty() || !actions.contains(idleAction) || actions.value(idleAction).frames.isEmpty()) {
        localErrors.append(QStringLiteral("Required role Idle must exist and contain at least one frame."));
    }
    if (actionForRole(SystemActionRole::Dragging).isEmpty()) {
        localErrors.append(QStringLiteral("Required role Dragging is missing."));
    }
    if (actionForRole(SystemActionRole::Falling).isEmpty()) {
        localErrors.append(QStringLiteral("Required role Falling is missing."));
    }
    if (patrolEnabled) {
        if (actionForRole(SystemActionRole::WalkLeft).isEmpty()) {
            localWarnings.append(QStringLiteral("Patrol is enabled but role WalkLeft is missing."));
        }
        if (actionForRole(SystemActionRole::WalkRight).isEmpty()) {
            localWarnings.append(QStringLiteral("Patrol is enabled but role WalkRight is missing."));
        }
    }

    if (!coverPath.isEmpty()) {
        const QString coverAbs = absolutePathFor(coverPath);
        if (!QFileInfo::exists(coverAbs)) {
            localErrors.append(QStringLiteral("Cover file is missing: %1").arg(coverPath));
        } else if (QImage(coverAbs).isNull()) {
            localErrors.append(QStringLiteral("Cover image cannot be read: %1").arg(coverPath));
        }
    }

    for (auto it = actions.constBegin(); it != actions.constEnd(); ++it) {
        const QString stateName = it.key();
        const PetAction &action = it.value();
        if (!action.id.isEmpty() && action.id != stateName) {
            localErrors.append(QStringLiteral("Action key '%1' does not match action.id '%2'.")
                                   .arg(stateName, action.id));
        }
        if (action.frames.isEmpty()) {
            localWarnings.append(QStringLiteral("State '%1' has no frames.").arg(stateName));
        }
        const QString nextId = action.nextActionId;
        if (!nextId.isEmpty() && !actions.contains(nextId)) {
            localErrors.append(QStringLiteral("State '%1' next references missing state '%2'.")
                                   .arg(stateName, nextId));
        }
        if (!action.frameDurationsMs.isEmpty() && action.frameDurationsMs.size() != action.frames.size()) {
            localWarnings.append(QStringLiteral("State '%1' frameDurationsMs length does not match frames length.")
                                     .arg(stateName));
        }
        for (auto eventIt = action.events.constBegin(); eventIt != action.events.constEnd(); ++eventIt) {
            if (!actions.contains(eventIt.value())) {
                localErrors.append(QStringLiteral("State '%1' event '%2' references missing state '%3'.")
                                       .arg(stateName, eventIt.key(), eventIt.value()));
            }
        }

        for (const PetFrame &frame : action.frames) {
            if (frame.path.isEmpty()) {
                localErrors.append(QStringLiteral("State '%1' contains a frame with an empty path.").arg(stateName));
                continue;
            }
            const QString frameAbs = absolutePathFor(frame.path);
            if (!QFileInfo::exists(frameAbs)) {
                localErrors.append(QStringLiteral("Missing frame file in state '%1': %2").arg(stateName, frame.path));
                continue;
            }
            if (QImage(frameAbs).isNull()) {
                localErrors.append(QStringLiteral("PNG cannot be read in state '%1': %2").arg(stateName, frame.path));
            }
        }
    }

    QMap<QString, QStringList> roleOwners;
    for (auto it = actions.constBegin(); it != actions.constEnd(); ++it) {
        const QString role = it.value().systemRole;
        if (!role.isEmpty() && role != QStringLiteral("None")) {
            roleOwners[role].append(it.key());
        }
    }
    for (auto it = roleOwners.constBegin(); it != roleOwners.constEnd(); ++it) {
        if (it.value().size() > 1) {
            localErrors.append(QStringLiteral("System Role %1 is assigned to multiple actions: %2")
                                   .arg(it.key(), it.value().join(QStringLiteral(", "))));
        }
    }

    for (const PetBehaviorRule &rule : behaviorRules) {
        if (!actions.contains(rule.actionId)) {
            localErrors.append(QStringLiteral("Behavior rule '%1' references missing action '%2'.")
                                   .arg(rule.id, rule.actionId));
        }
    }

    if (errors) {
        *errors = localErrors;
    }
    if (warnings) {
        *warnings = localWarnings;
    }
    return localErrors.isEmpty();
}

bool PetProject::exportPetpack(const QString &petpackPath, QString *errorMessage) const
{
    QStringList errors;
    QStringList warnings;
    if (!validate(&errors, &warnings)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Project validation failed:\n%1").arg(errors.join('\n'));
        }
        return false;
    }

    if (!QFileInfo::exists(QDir(projectDir).filePath("assets"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot export without assets directory.");
        }
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create temporary directory for export.");
        }
        return false;
    }

    const QString stagingDir = QDir(tempDir.path()).filePath("package");
    QDir().mkpath(stagingDir);

    QFile sourcePetJson(petJsonPath());
    if (!sourcePetJson.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot read pet.json for export: %1").arg(petJsonPath());
        }
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument packageJson = QJsonDocument::fromJson(sourcePetJson.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !packageJson.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot parse pet.json for export: %1").arg(parseError.errorString());
        }
        return false;
    }

    QJsonObject packageRoot = packageJson.object();
    packageRoot.insert("cover", "cover.png");
    packageJson.setObject(packageRoot);

    QFile stagedPetJson(QDir(stagingDir).filePath("pet.json"));
    if (!stagedPetJson.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot stage pet.json for export.");
        }
        return false;
    }
    stagedPetJson.write(packageJson.toJson(QJsonDocument::Indented));
    stagedPetJson.close();

    if (!copyRecursively(QDir(projectDir).filePath("assets"), QDir(stagingDir).filePath("assets"), errorMessage)) {
        return false;
    }

    const QString coverAbs = absolutePathFor(coverPath);
    if (QFileInfo::exists(coverAbs)
        && !copyRecursively(coverAbs, QDir(stagingDir).filePath("cover.png"), errorMessage)) {
        return false;
    }

    const QString tempZip = QDir(tempDir.path()).filePath("package.zip");
    const QString command = QStringLiteral("Compress-Archive -Path %1 -DestinationPath %2 -Force")
                                .arg(quotedPowerShellPath(QDir(stagingDir).filePath("*")),
                                     quotedPowerShellPath(tempZip));
    if (!archiveCommandRunner().run(command, errorMessage)) {
        return false;
    }

    QDir().mkpath(QFileInfo(petpackPath).absolutePath());
    QFile stagedZip(tempZip);
    QSaveFile output(petpackPath);
    if (!stagedZip.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to stage petpack output: %1").arg(petpackPath);
        }
        return false;
    }
    QByteArray chunk;
    while (!(chunk = stagedZip.read(1024 * 1024)).isEmpty()) {
        if (output.write(chunk) != chunk.size()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Failed to write petpack: %1").arg(petpackPath);
            }
            return false;
        }
    }
    if (stagedZip.error() != QFile::NoError || !output.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to atomically commit petpack: %1").arg(petpackPath);
        }
        return false;
    }

    return true;
}

bool PetProject::importPetpack(const QString &petpackPath,
                               const QString &targetParentDir,
                               QString *importedPetJsonPath,
                               QString *errorMessage)
{
    if (!QFileInfo::exists(petpackPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Petpack does not exist: %1").arg(petpackPath);
        }
        return false;
    }
    if (!validatePetpackArchiveEntries(petpackPath, errorMessage)) {
        return false;
    }
    const QStorageInfo targetStorage(QFileInfo(targetParentDir).absolutePath());
    const qint64 requiredFreeBytes = qMin<qint64>(512LL * 1024 * 1024,
                                                  QFileInfo(petpackPath).size() * 4 + 64LL * 1024 * 1024);
    if (targetStorage.isValid() && targetStorage.isReady()
        && targetStorage.bytesAvailable() < requiredFreeBytes) {
        if (errorMessage) *errorMessage = QStringLiteral("Not enough free disk space to import this petpack safely.");
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create temporary directory for import.");
        }
        return false;
    }

    const QString tempZip = QDir(tempDir.path()).filePath("package.zip");
    if (!QFile::copy(petpackPath, tempZip)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to stage petpack for import.");
        }
        return false;
    }

    const QString extractDir = QDir(tempDir.path()).filePath("extract");
    const QString command = QStringLiteral("Expand-Archive -LiteralPath %1 -DestinationPath %2 -Force")
                                .arg(quotedPowerShellPath(tempZip), quotedPowerShellPath(extractDir));
    if (!archiveCommandRunner().run(command, errorMessage)) {
        return false;
    }
    if (!validateExtractedPetpackTree(extractDir, errorMessage)) {
        return false;
    }

    QString packageRoot = extractDir;
    if (!QFileInfo::exists(QDir(packageRoot).filePath("pet.json"))) {
        const QFileInfoList dirs = QDir(extractDir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &dirInfo : dirs) {
            if (QFileInfo::exists(QDir(dirInfo.absoluteFilePath()).filePath("pet.json"))) {
                packageRoot = dirInfo.absoluteFilePath();
                break;
            }
        }
    }

    if (!QFileInfo::exists(QDir(packageRoot).filePath("pet.json"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid petpack: pet.json is missing.");
        }
        return false;
    }
    if (!QFileInfo::exists(QDir(packageRoot).filePath("assets"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid petpack: assets directory is missing.");
        }
        return false;
    }

    PetProject packageProject;
    if (!packageProject.load(QDir(packageRoot).filePath("pet.json"), errorMessage)) {
        return false;
    }
    QStringList errors;
    QStringList warnings;
    if (!packageProject.validate(&errors, &warnings)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Imported petpack failed validation:\n%1").arg(errors.join('\n'));
        }
        return false;
    }

    const QString baseName = safeProjectDirName(packageProject.name);
    QString targetDir = QDir(targetParentDir).filePath(baseName);
    int suffix = 2;
    while (QFileInfo::exists(targetDir)) {
        targetDir = QDir(targetParentDir).filePath(QStringLiteral("%1_%2").arg(baseName).arg(suffix++));
    }

    const QString stagedTargetDir = QDir(targetParentDir).filePath(
        QStringLiteral(".petpack_staging_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (!copyRecursively(packageRoot, stagedTargetDir, errorMessage)) {
        removeDirectoryChecked(stagedTargetDir, nullptr);
        return false;
    }
    PetProject stagedProject;
    if (!stagedProject.load(QDir(stagedTargetDir).filePath("pet.json"), errorMessage)
        || !stagedProject.validate(&errors, &warnings)) {
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = QStringLiteral("Staged petpack failed validation:\n%1").arg(errors.join('\n'));
        }
        removeDirectoryChecked(stagedTargetDir, nullptr);
        return false;
    }
    if (!renameDirectoryChecked(stagedTargetDir, targetDir, errorMessage)) {
        removeDirectoryChecked(stagedTargetDir, nullptr);
        return false;
    }

    if (importedPetJsonPath) {
        *importedPetJsonPath = QDir(targetDir).filePath("pet.json");
    }
    return true;
}

bool PetProject::validatePetpackArchiveForTesting(const QString &petpackPath, QString *errorMessage)
{
    return validatePetpackArchiveEntries(petpackPath, errorMessage);
}

void PetProject::setArchiveCommandRunnerForTesting(IArchiveCommandRunner *runner)
{
    g_archiveRunnerForTesting = runner;
}

bool PetProject::validateExtractedPetpackTreeForTesting(const QString &extractRoot, QString *errorMessage)
{
    return validateExtractedPetpackTree(extractRoot, errorMessage);
}

QString PetProject::petJsonPath() const
{
    return QDir(projectDir).filePath("pet.json");
}

QString PetProject::effectiveProjectId() const
{
    if (!projectId.trimmed().isEmpty()) {
        return projectId.trimmed();
    }
    const QByteArray path = QDir::cleanPath(petJsonPath()).toUtf8();
    return QStringLiteral("legacy:%1").arg(QString::fromLatin1(QCryptographicHash::hash(path, QCryptographicHash::Sha1).toHex()));
}

QString PetProject::absolutePathFor(const QString &relativePath) const
{
    if (QDir::isAbsolutePath(relativePath)) {
        return QDir::cleanPath(relativePath);
    }
    return QDir(projectDir).filePath(relativePath);
}

QStringList PetProject::actionNames() const
{
    QStringList names;
    for (const QString &stateName : statesForTemplate(templateName)) {
        if (actions.contains(stateName)) {
            names.append(stateName);
        }
    }

    QStringList extras = actions.keys();
    for (const QString &stateName : names) {
        extras.removeAll(stateName);
    }
    std::sort(extras.begin(), extras.end(), PetProject::naturalLess);
    names.append(extras);
    return names;
}

QString PetProject::systemRoleName(SystemActionRole role)
{
    switch (role) {
    case SystemActionRole::Idle: return QStringLiteral("Idle");
    case SystemActionRole::WalkLeft: return QStringLiteral("WalkLeft");
    case SystemActionRole::WalkRight: return QStringLiteral("WalkRight");
    case SystemActionRole::Dragging: return QStringLiteral("Dragging");
    case SystemActionRole::Falling: return QStringLiteral("Falling");
    case SystemActionRole::Landing: return QStringLiteral("Landing");
    case SystemActionRole::Sleeping: return QStringLiteral("Sleeping");
    case SystemActionRole::AIThinking: return QStringLiteral("AIThinking");
    case SystemActionRole::AITalking: return QStringLiteral("AITalking");
    case SystemActionRole::ClickReaction: return QStringLiteral("ClickReaction");
    case SystemActionRole::None:
    default:
        return QStringLiteral("None");
    }
}

SystemActionRole PetProject::systemRoleFromName(const QString &name)
{
    if (name == QStringLiteral("Idle")) return SystemActionRole::Idle;
    if (name == QStringLiteral("WalkLeft")) return SystemActionRole::WalkLeft;
    if (name == QStringLiteral("WalkRight")) return SystemActionRole::WalkRight;
    if (name == QStringLiteral("Dragging")) return SystemActionRole::Dragging;
    if (name == QStringLiteral("Falling")) return SystemActionRole::Falling;
    if (name == QStringLiteral("Landing")) return SystemActionRole::Landing;
    if (name == QStringLiteral("Sleeping")) return SystemActionRole::Sleeping;
    if (name == QStringLiteral("AIThinking")) return SystemActionRole::AIThinking;
    if (name == QStringLiteral("AITalking")) return SystemActionRole::AITalking;
    if (name == QStringLiteral("ClickReaction")) return SystemActionRole::ClickReaction;
    return SystemActionRole::None;
}

QString PetProject::actionForRole(SystemActionRole role, const QString &fallback) const
{
    const QString roleName = systemRoleName(role);
    for (auto it = actions.constBegin(); it != actions.constEnd(); ++it) {
        if (it.value().systemRole == roleName && !it.value().frames.isEmpty()) {
            return it.key();
        }
    }
    if (!fallback.isEmpty() && actions.contains(fallback) && !actions.value(fallback).frames.isEmpty()) {
        return fallback;
    }
    return QString();
}

QStringList PetProject::aiTriggerActionIds() const
{
    QStringList ids;
    for (auto it = actions.constBegin(); it != actions.constEnd(); ++it) {
        if (it.value().allowAiTrigger && !it.value().frames.isEmpty()) {
            ids.append(it.key());
        }
    }
    return ids;
}

QVector<AIActionDescriptor> PetProject::aiTriggerActionDescriptors() const
{
    QVector<AIActionDescriptor> descriptors;
    for (auto it = actions.constBegin(); it != actions.constEnd(); ++it) {
        const PetAction &action = it.value();
        if (action.allowAiTrigger && !action.frames.isEmpty()) {
            AIActionDescriptor descriptor;
            descriptor.id = it.key();
            descriptor.displayName = action.displayName.trimmed().isEmpty() ? it.key() : action.displayName.trimmed();
            descriptor.description = action.description.trimmed();
            descriptor.category = action.systemRole.trimmed().isEmpty() ? QStringLiteral("Custom") : action.systemRole;
            descriptor.allowAiTrigger = true;
            descriptor.allowedStates = action.aiAllowedStates.isEmpty()
                ? QStringList {QStringLiteral("Normal")}
                : action.aiAllowedStates;
            descriptor.parameterSchema = action.aiParameterSchema;
            descriptors.append(descriptor);
        }
    }
    return descriptors;
}

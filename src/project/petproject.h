#ifndef PETPROJECT_H
#define PETPROJECT_H

#include <QMap>
#include <QPair>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

#include "aiactiondescriptor.h"
#include "archivecommandrunner.h"
#include "petbehaviorrule.h"

enum class ActionSourceType
{
    PngSequence,
    GifImported,
    SpriteSheet,
    ProceduralGenerated,
    ShimejiImported
};

enum class SystemActionRole
{
    None,
    Idle,
    WalkLeft,
    WalkRight,
    Dragging,
    Falling,
    Landing,
    Sleeping,
    AIThinking,
    AITalking,
    ClickReaction
};

struct SpriteSheetSourceMetadata
{
    QString sourceFileName;
    int rows {1};
    int columns {1};
    int frameWidth {0};
    int frameHeight {0};
    int marginLeft {0};
    int marginTop {0};
    int marginRight {0};
    int marginBottom {0};
    int spacingX {0};
    int spacingY {0};
    QString readingOrder {QStringLiteral("LeftToRightTopToBottom")};
    int startFrame {0};
    int maxFrames {0};
    bool skipTransparentFrames {false};
};

struct ShimejiSourceMetadata
{
    QString originalActionName;
    QString originalType;
    QString borderType;
    QStringList unsupportedConditions;
    QStringList behaviorNames;
};

struct ImportFramesOptions
{
    ActionSourceType sourceType {ActionSourceType::PngSequence};
    QVector<int> frameDurationsMs;
    QVector<QPoint> frameOffsets;
    bool hasSpriteSheetMetadata {false};
    SpriteSheetSourceMetadata spriteSheetMetadata;
};

struct PetFrame
{
    QString path;
    QPoint offset;
    QPoint autoOffset;
};

struct PetAction
{
    QString id;
    QString name;
    QString displayName;
    QString englishName;
    QString description;
    ActionSourceType sourceType {ActionSourceType::PngSequence};
    int fps {8};
    bool loop {true};
    int playCount {1};
    QString next;
    QString nextActionId;
    QPoint offset;
    int expectedFrameCount {0};
    int spriteSheetRows {1};
    int spriteSheetColumns {1};
    SpriteSheetSourceMetadata spriteSheetSource;
    ShimejiSourceMetadata shimejiSource;
    double previewScale {1.0};
    int footBaselineOffset {0};
    bool allowHorizontalDisplacement {false};
    bool mirrorSupported {false};
    QString defaultPrompt;
    QString negativePrompt;
    QString systemRole;
    bool showInContextMenu {false};
    QString menuGroup;
    bool allowAiTrigger {false};
    QStringList aiAllowedStates {QStringLiteral("Normal")};
    QJsonObject aiParameterSchema;
    bool allowRandomTrigger {false};
    double randomWeight {1.0};
    int randomCooldownMs {0};
    QVector<int> frameDurationsMs;
    QMap<QString, QString> events;
    QVector<PetFrame> frames;
};

class PetProject
{
public:
    struct RuntimeStatePatch {
        bool hasAnchorScreen {false};
        QPoint anchorScreen;
        bool clearAnchorScreen {false};
        bool hasLocked {false};
        bool locked {false};
        bool hasMousePassthrough {false};
        bool mousePassthrough {false};
        bool hasTopMost {false};
        bool topMost {false};
        bool hasPatrolEnabled {false};
        bool patrolEnabled {true};
    };

    static QStringList templateNames();
    static QStringList statesForTemplate(const QString &templateName);
    static PetProject createNew(const QString &projectDir, const QString &name, const QString &templateName);
    static bool naturalLess(const QString &left, const QString &right);
    static bool isLegalActionId(const QString &id);

    bool isValid() const;
    bool load(const QString &petJsonPath, QString *errorMessage = nullptr);
    void ensureProjectId();
    bool save(QString *errorMessage = nullptr);
    bool saveRuntimeStatePatch(const RuntimeStatePatch &patch, QString *errorMessage = nullptr) const;
    bool importPngFrames(const QString &stateName, const QStringList &sourceFiles, QString *errorMessage = nullptr);
    bool importPngFrames(const QString &stateName,
                         const QStringList &sourceFiles,
                         const ImportFramesOptions &options,
                         QString *errorMessage = nullptr);
    bool importGifFrames(const QString &stateName,
                         const QString &gifFile,
                         double importScale = 1.0,
                         QString *errorMessage = nullptr);
    bool updateAutoCover(QString *errorMessage = nullptr);
    bool setCustomCover(const QString &sourceFile, QString *errorMessage = nullptr);
    QPoint suggestedAnchorFromIdle() const;
    bool validate(QStringList *errors, QStringList *warnings = nullptr) const;
    bool exportPetpack(const QString &petpackPath, QString *errorMessage = nullptr) const;

    static bool importPetpack(const QString &petpackPath,
                              const QString &targetParentDir,
                              QString *importedPetJsonPath,
                              QString *errorMessage = nullptr);
    static bool validatePetpackArchiveForTesting(const QString &petpackPath,
                                                 QString *errorMessage = nullptr);
    static bool validateExtractedPetpackTreeForTesting(const QString &extractRoot,
                                                       QString *errorMessage = nullptr);
    static void setArchiveCommandRunnerForTesting(IArchiveCommandRunner *runner);

    QString petJsonPath() const;
    QString absolutePathFor(const QString &relativePath) const;
    QStringList actionNames() const;
    QString effectiveProjectId() const;
    QString actionForRole(SystemActionRole role, const QString &fallback = QString()) const;
    QStringList aiTriggerActionIds() const;
    QVector<AIActionDescriptor> aiTriggerActionDescriptors() const;
    static QString systemRoleName(SystemActionRole role);
    static SystemActionRole systemRoleFromName(const QString &name);

    int schemaVersion {3};
    int version {1};
    QString projectId;
    QString name;
    QString templateName {"simple"};
    QString projectDir;
    QSize canvasSize;
    QPoint anchor;
    QPoint runtimeAnchorScreen;
    bool hasRuntimeAnchorScreen {false};
    double scale {1.0};
    bool mousePassthrough {false};
    bool locked {false};
    bool topMost {true};
    bool invertWalkDirection {false};
    bool patrolEnabled {true};
    QString aiCharacterName;
    QString aiSystemPrompt;
    QString aiProviderProfileId;
    QString renderBackend {QStringLiteral("sprite")};
    QJsonObject live2dModelMetadata;
    QString coverPath {"cover.png"};
    QString coverMode {"auto"};
    QMap<QString, PetAction> actions;
    QVector<PetBehaviorRule> behaviorRules;
};

#endif // PETPROJECT_H

#include "motionpromptlibrary.h"

#include <QMap>
#include <QtGlobal>

namespace {
QString commonNegativePrompt()
{
    return QStringLiteral(
        "extra arms, extra legs, extra fingers, missing limbs, duplicated body parts, "
        "inconsistent costume, inconsistent hairstyle, deformed hands, broken anatomy, "
        "cropped character, changing accessories, changing character identity, text, "
        "watermark, background, frame border, shadow base, inconsistent scale, "
        "inconsistent camera angle, large horizontal displacement, extra props");
}

QString buildPrompt(const QString &id,
                    const QString &englishName,
                    int frameCount,
                    int rows,
                    int columns,
                    const QString &storyboard,
                    bool loop,
                    bool allowHorizontalDisplacement)
{
    return QStringLiteral(
        "Create a transparent-background sprite sheet for one Project Sekai style chibi character.\n"
        "Action ID: %1\n"
        "English action name: %2\n"
        "Frames: %3\n"
        "Sprite sheet layout: %4 rows x %5 columns, left-to-right then top-to-bottom chronological order.\n"
        "Each cell must have the same size, transparent padding, stable character center, fixed canvas, fixed camera, fixed body scale, and fixed foot baseline.\n"
        "Character consistency: keep the exact same face shape, hairstyle, hair color, eyes, costume, accessories, body proportions, and cute chibi visual style in every frame.\n"
        "Motion storyboard:\n%6\n"
        "Horizontal displacement: %7.\n"
        "Ending rule: the first and last frame must naturally connect to the standard standing idle pose%8.\n"
        "Do not add text, frame numbers, borders, watermarks, scene background, shadow floor, extra props, or extra characters.\n")
        .arg(id,
             englishName,
             QString::number(frameCount),
             QString::number(rows),
             QString::number(columns),
             storyboard.trimmed(),
             allowHorizontalDisplacement ? QStringLiteral("small controlled pose shift only") : QStringLiteral("not allowed; keep both feet anchored in the same canvas area"),
             loop ? QStringLiteral(", and the loop must be smooth when frame 8 returns to frame 1") : QString());
}

MotionPromptSpec makeSpec(const QString &id,
                          const QString &displayName,
                          const QString &englishName,
                          const QString &category,
                          const QString &description,
                          const QString &storyboard,
                          int frameCount,
                          int fps,
                          bool loop,
                          bool allowHorizontalDisplacement,
                          bool mirrorSupported)
{
    MotionPromptSpec spec;
    spec.id = id;
    spec.displayName = displayName;
    spec.englishName = englishName;
    spec.category = category;
    spec.description = description;
    spec.storyboard = storyboard;
    spec.frameCount = frameCount;
    spec.fps = fps;
    spec.loop = loop;
    spec.rows = frameCount <= 6 ? 2 : 2;
    spec.columns = frameCount <= 6 ? 3 : 4;
    spec.allowHorizontalDisplacement = allowHorizontalDisplacement;
    spec.mirrorSupported = mirrorSupported;
    spec.prompt = buildPrompt(id, englishName, frameCount, spec.rows, spec.columns, storyboard, loop, allowHorizontalDisplacement);
    spec.negativePrompt = commonNegativePrompt();
    return spec;
}

const QMap<QString, MotionPromptSpec> &specs()
{
    static const QMap<QString, MotionPromptSpec> table {
        {QStringLiteral("rhythm_step"),
         makeSpec(QStringLiteral("rhythm_step"),
                  QStringLiteral("节奏踏步"),
                  QStringLiteral("Rhythm Step"),
                  QStringLiteral("舞台动作"),
                  QStringLiteral("左右脚交替轻点地面，身体随节奏轻微上下弹动，适合循环播放。"),
                  QStringLiteral("Frame 1: neutral standing pose, front or slight three-quarter view.\n"
                                 "Frame 2: left foot lightly taps the ground, body dips slightly, arms swing naturally.\n"
                                 "Frame 3: body rebounds upward, hair and costume follow with soft inertia.\n"
                                 "Frame 4: return near neutral without horizontal travel.\n"
                                 "Frame 5: right foot lightly taps the ground, body dips slightly, opposite arm swing.\n"
                                 "Frame 6: body rebounds upward, hair and costume settle.\n"
                                 "Frame 7: small rhythmic bounce, both feet close to the original baseline.\n"
                                 "Frame 8: back to clean standing pose matching frame 1 for looping."),
                  8, 12, true, false, true)},
        {QStringLiteral("peek"),
         makeSpec(QStringLiteral("peek"),
                  QStringLiteral("侧身探头"),
                  QStringLiteral("Peek"),
                  QStringLiteral("互动动作"),
                  QStringLiteral("身体向一侧轻倾，头部和上半身探出观察，然后缩回站好。"),
                  QStringLiteral("Frame 1: standard standing pose, both feet fixed.\n"
                                 "Frame 2: shoulders and upper body begin leaning to one side.\n"
                                 "Frame 3: head and upper body peek farther from the side, curious expression.\n"
                                 "Frame 4: hold the peek pose briefly, feet do not drift.\n"
                                 "Frame 5: start retracting, torso returns toward center.\n"
                                 "Frame 6: head follows back, hair and clothing settle.\n"
                                 "Frame 7: almost neutral, no canvas drift.\n"
                                 "Frame 8: standard standing pose matching idle."),
                  8, 9, false, false, true)},
        {QStringLiteral("look_far"),
         makeSpec(QStringLiteral("look_far"),
                  QStringLiteral("抬手远望"),
                  QStringLiteral("Look Far"),
                  QStringLiteral("互动动作"),
                  QStringLiteral("抬手遮在额前向远处看，轻微前倾并短暂停留。"),
                  QStringLiteral("Frame 1: neutral standing pose.\n"
                                 "Frame 2: one hand begins lifting toward the forehead.\n"
                                 "Frame 3: hand reaches above the forehead as if blocking light, body leans forward slightly.\n"
                                 "Frame 4: head turns a little upward or toward the distance, optional tiny tiptoe.\n"
                                 "Frame 5: hold the looking pose, no telescope, hat, or new prop.\n"
                                 "Frame 6: hand starts lowering, body straightens.\n"
                                 "Frame 7: arm returns close to resting position.\n"
                                 "Frame 8: standard standing pose matching idle."),
                  8, 8, false, false, true)},
        {QStringLiteral("adjust_clothes"),
         makeSpec(QStringLiteral("adjust_clothes"),
                  QStringLiteral("整理衣服"),
                  QStringLiteral("Adjust Clothes"),
                  QStringLiteral("待机动作"),
                  QStringLiteral("低头检查并轻轻整理袖口、领结、衣摆或外套，最后放松站好。"),
                  QStringLiteral("Frame 1: neutral standing pose.\n"
                                 "Frame 2: character looks down calmly at sleeve, bow, hem, or jacket.\n"
                                 "Frame 3: one hand reaches to adjust the clothing detail.\n"
                                 "Frame 4: both posture and hand movement stay small and quiet.\n"
                                 "Frame 5: finish a gentle tidying motion without changing costume structure.\n"
                                 "Frame 6: head begins lifting, hands return naturally.\n"
                                 "Frame 7: satisfied or relaxed small expression.\n"
                                 "Frame 8: standard standing pose matching idle."),
                  8, 8, false, false, true)},
        {QStringLiteral("hands_behind_sway"),
         makeSpec(QStringLiteral("hands_behind_sway"),
                  QStringLiteral("双手背后摇摆"),
                  QStringLiteral("Hands Behind Sway"),
                  QStringLiteral("待机动作"),
                  QStringLiteral("双手放到身后，身体轻轻左右摇摆，可爱但克制，适合循环。"),
                  QStringLiteral("Frame 1: neutral standing pose, hands begin moving behind the back.\n"
                                 "Frame 2: both hands settle behind the back without intersecting the torso.\n"
                                 "Frame 3: body sways slightly left, head tilts gently, feet stay fixed.\n"
                                 "Frame 4: return toward center with soft hair and clothing inertia.\n"
                                 "Frame 5: body sways slightly right, shoulders remain anatomically correct.\n"
                                 "Frame 6: return toward center.\n"
                                 "Frame 7: tiny relaxed bounce or tiptoe, hands remain behind back.\n"
                                 "Frame 8: loop-ready pose close to frame 1."),
                  8, 10, true, false, true)},
        {QStringLiteral("listen"),
         makeSpec(QStringLiteral("listen"),
                  QStringLiteral("侧耳倾听"),
                  QStringLiteral("Listen"),
                  QStringLiteral("互动动作"),
                  QStringLiteral("一只手放在耳边，头部轻倾，认真听玩家说话后恢复。"),
                  QStringLiteral("Frame 1: neutral standing pose.\n"
                                 "Frame 2: character leans slightly forward.\n"
                                 "Frame 3: one hand lifts toward the ear, head begins tilting.\n"
                                 "Frame 4: hand rests beside the ear, attentive listening expression.\n"
                                 "Frame 5: hold the listening pose, no headphones, microphone, or new prop.\n"
                                 "Frame 6: hand lowers, head returns toward center.\n"
                                 "Frame 7: body straightens, clothing and hair settle.\n"
                                 "Frame 8: standard standing pose matching idle."),
                  8, 8, false, false, true)},
    };
    return table;
}
}

QStringList MotionPromptLibrary::templates()
{
    return {QStringLiteral("idle"), QStringLiteral("walk_left"), QStringLiteral("walk_right"),
            QStringLiteral("click"), QStringLiteral("drag"), QStringLiteral("drop"),
            QStringLiteral("sleep"), QStringLiteral("nod"), QStringLiteral("shake_head"),
            QStringLiteral("wave"), QStringLiteral("stretch"), QStringLiteral("jump"),
            QStringLiteral("dance"), QStringLiteral("rhythm_step"), QStringLiteral("peek"),
            QStringLiteral("look_far"), QStringLiteral("adjust_clothes"),
            QStringLiteral("hands_behind_sway"), QStringLiteral("listen")};
}

MotionPromptSpec MotionPromptLibrary::specForAction(const QString &actionName)
{
    const QString key = actionName.trimmed();
    if (specs().contains(key)) {
        return specs().value(key);
    }
    MotionPromptSpec spec;
    spec.id = key.isEmpty() ? QStringLiteral("idle") : key;
    spec.displayName = spec.id;
    spec.englishName = spec.id;
    spec.category = QStringLiteral("自定义动作");
    spec.description = QStringLiteral("自定义动作。");
    spec.frameCount = 8;
    spec.fps = 8;
    spec.rows = 2;
    spec.columns = 4;
    spec.prompt = buildPrompt(spec.id,
                              spec.englishName,
                              spec.frameCount,
                              spec.rows,
                              spec.columns,
                              QStringLiteral("Create a smooth readable motion and return to idle."),
                              false,
                              false);
    spec.negativePrompt = commonNegativePrompt();
    return spec;
}

QString MotionPromptLibrary::promptForAction(const QString &actionName, int frameCount, int rows, int columns, const QString &steps)
{
    MotionPromptSpec spec = specForAction(actionName);
    const QString storyboard = steps.trimmed().isEmpty() ? spec.storyboard : steps.trimmed();
    const int effectiveFrames = qMax(1, frameCount);
    const int effectiveRows = qMax(1, rows);
    const int effectiveColumns = qMax(1, columns);
    return buildPrompt(spec.id,
                       spec.englishName.isEmpty() ? spec.id : spec.englishName,
                       effectiveFrames,
                       effectiveRows,
                       effectiveColumns,
                       storyboard,
                       spec.loop,
                       spec.allowHorizontalDisplacement);
}

QString MotionPromptLibrary::negativePromptForAction(const QString &actionName)
{
    return specForAction(actionName).negativePrompt;
}

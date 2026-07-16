#include "aiconversationroommanager.h"
#include "assetqualityanalyzer.h"
#include "motionpromptlibrary.h"
#include "petproject.h"
#include "spritesheetslicer.h"

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QTemporaryDir>

#include <iostream>

namespace {
bool check(bool condition, const char *name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    return condition;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    QTemporaryDir dir;
    ok &= check(dir.isValid(), "temporary workspace");

    QString error;
    PetProject project = PetProject::createNew(dir.filePath(QStringLiteral("pet")),
                                               QStringLiteral("Smoke Pet"),
                                               QStringLiteral("simple"));
    ok &= check(project.save(&error), "project save");
    PetProject reloaded;
    ok &= check(reloaded.load(project.petJsonPath(), &error)
                    && reloaded.projectId == project.projectId,
                "project save/load identity");

    QImage sheet(QSize(32, 16), QImage::Format_ARGB32);
    sheet.fill(Qt::transparent);
    sheet.fill(Qt::red);
    SpriteSheetSliceOptions options;
    options.rows = 1;
    options.columns = 2;
    const SpriteSheetSliceResult slices = SpriteSheetSlicer::sliceFrameResult(sheet, options, &error);
    ok &= check(slices.frames.size() == 2
                    && slices.frames.first().size() == QSize(16, 16),
                "sprite sheet pipeline");

    const QString frameA = dir.filePath(QStringLiteral("frame-a.png"));
    const QString frameB = dir.filePath(QStringLiteral("frame-b.png"));
    slices.frames.at(0).save(frameA, "PNG");
    slices.frames.at(1).save(frameB, "PNG");
    const AssetQualityReport quality = AssetQualityAnalyzer::analyzeFrames({frameA, frameB});
    ok &= check(quality.allFramesReadable && quality.canvasSizeConsistent,
                "asset quality pipeline");

    AIConversationRoom room;
    room.mode = AIConversationMode::Directed;
    room.minRespondersPerTurn = room.maxRespondersPerTurn = 1;
    AIConversationParticipant participant;
    participant.participantId = QStringLiteral("pet-a");
    participant.characterName = QStringLiteral("Pet A");
    room.participants.append(participant);
    const auto responders = AIConversationRoomManager::selectRespondersForRoom(room, {participant.participantId});
    ok &= check(responders.size() == 1 && responders.first().participantId == participant.participantId,
                "multi-AI responder routing");

    const MotionPromptSpec motion = MotionPromptLibrary::specForAction(QStringLiteral("wave"));
    ok &= check(!motion.prompt.isEmpty() && motion.frameCount > 0,
                "motion prompt library");
    return ok ? 0 : 1;
}

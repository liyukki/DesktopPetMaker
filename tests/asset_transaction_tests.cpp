#include <QtTest>

#include "petproject.h"
#include "proceduralmotiongenerator.h"
#include "test_support.h"

#include <QCryptographicHash>

namespace {
PetProject fixture(const QString &path)
{
    PetProject project = PetProject::createNew(path, QStringLiteral("AssetFixture"), QStringLiteral("simple"));
    project.canvasSize = QSize(24, 32);
    project.anchor = QPoint(12, 32);
    PetAction idle;
    idle.id = QStringLiteral("idle");
    idle.systemRole = QStringLiteral("Idle");
    project.actions.insert(idle.id, idle);
    return project;
}

QByteArray hashFile(const QString &path)
{
    return QCryptographicHash::hash(TestSupport::readBytes(path), QCryptographicHash::Sha256);
}
}

class AssetTransactionTests : public QObject
{
    Q_OBJECT
private slots:
    void pngCommitAndReload()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PetProject project = fixture(dir.filePath(QStringLiteral("project")));
        const QString first = TestSupport::writePng(dir.filePath(QStringLiteral("a.png")), QSize(24, 32), Qt::red);
        const QString second = TestSupport::writePng(dir.filePath(QStringLiteral("b.png")), QSize(24, 32), Qt::blue);
        QString error;
        QVERIFY2(project.importPngFrames(QStringLiteral("idle"), {first, second}, &error), qPrintable(error));
        QCOMPARE(project.actions.value(QStringLiteral("idle")).frames.size(), 2);
        QVERIFY(QFileInfo::exists(project.absolutePathFor(project.coverPath)));
        PetProject reloaded;
        QVERIFY2(reloaded.load(project.petJsonPath(), &error), qPrintable(error));
        QCOMPARE(reloaded.actions.value(QStringLiteral("idle")).frames.size(), 2);
    }

    void failedImportPreservesJsonFramesAndCover()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PetProject project = fixture(dir.filePath(QStringLiteral("project")));
        const QString source = TestSupport::writePng(dir.filePath(QStringLiteral("source.png")), QSize(24, 32), Qt::red);
        QString error;
        QVERIFY2(project.importPngFrames(QStringLiteral("idle"), {source}, &error), qPrintable(error));
        const QString frame = project.absolutePathFor(project.actions.value(QStringLiteral("idle")).frames.first().path);
        const QString cover = project.absolutePathFor(project.coverPath);
        const QByteArray jsonBefore = TestSupport::readBytes(project.petJsonPath());
        const QByteArray frameBefore = hashFile(frame);
        const QByteArray coverBefore = hashFile(cover);

        QVERIFY(!project.importPngFrames(QStringLiteral("idle"),
                                         {source, dir.filePath(QStringLiteral("missing.png"))}, &error));
        QCOMPARE(TestSupport::readBytes(project.petJsonPath()), jsonBefore);
        QCOMPARE(hashFile(frame), frameBefore);
        QCOMPARE(hashFile(cover), coverBefore);
        QCOMPARE(project.actions.value(QStringLiteral("idle")).frames.size(), 1);
        QVERIFY(!project.importPngFrames(QStringLiteral("idle"), {}, &error));
    }

    void gifAndProceduralSourcesPersist()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PetProject project = fixture(dir.filePath(QStringLiteral("project")));
        const QString pseudoGif = TestSupport::writePng(dir.filePath(QStringLiteral("fixture.gif")), QSize(24, 32), Qt::green);
        QString error;
        QVERIFY2(project.importGifFrames(QStringLiteral("idle"), pseudoGif, 1.0, &error), qPrintable(error));
        QCOMPARE(project.actions.value(QStringLiteral("idle")).sourceType, ActionSourceType::GifImported);

        PetAction wave;
        wave.id = QStringLiteral("wave");
        project.actions.insert(wave.id, wave);
        const QString source = TestSupport::writePng(dir.filePath(QStringLiteral("procedural.png")), QSize(24, 32), Qt::yellow);
        const QString generatedDir = dir.filePath(QStringLiteral("generated"));
        const QStringList generated = ProceduralMotionGenerator::generateFrames(
            source, generatedDir, QStringLiteral("wave"), QStringLiteral("soft"), 4, &error);
        QVERIFY2(!generated.isEmpty(), qPrintable(error));
        ImportFramesOptions options;
        options.sourceType = ActionSourceType::ProceduralGenerated;
        QVERIFY2(project.importPngFrames(QStringLiteral("wave"), generated, options, &error), qPrintable(error));
        QCOMPARE(project.actions.value(QStringLiteral("wave")).sourceType, ActionSourceType::ProceduralGenerated);
        PetProject reloaded;
        QVERIFY2(reloaded.load(project.petJsonPath(), &error), qPrintable(error));
        QCOMPARE(reloaded.actions.value(QStringLiteral("wave")).frames.size(), generated.size());
    }
};

QTEST_MAIN(AssetTransactionTests)
#include "asset_transaction_tests.moc"

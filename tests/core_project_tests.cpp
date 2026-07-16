#include <QtTest>

#include "petproject.h"
#include "test_support.h"

class CoreProjectTests : public QObject
{
    Q_OBJECT
private slots:
    void saveAndReloadProject()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PetProject project;
        project.name = QStringLiteral("CoreFixture");
        project.projectDir = dir.path();
        project.canvasSize = QSize(32, 40);
        project.anchor = QPoint(16, 40);
        PetAction idle;
        idle.id = QStringLiteral("idle");
        idle.systemRole = QStringLiteral("Idle");
        const QString frame = TestSupport::writePng(dir.filePath("idle.png"), project.canvasSize);
        QVERIFY(!frame.isEmpty());
        idle.frames.append({QStringLiteral("idle.png"), {}, {}});
        project.actions.insert(idle.id, idle);
        QString error;
        QVERIFY2(project.save(&error), qPrintable(error));
        PetProject loaded;
        QVERIFY2(loaded.load(project.petJsonPath(), &error), qPrintable(error));
        QCOMPARE(loaded.projectId, project.projectId);
        QCOMPARE(loaded.actionForRole(SystemActionRole::Idle), QStringLiteral("idle"));
    }
};

QTEST_MAIN(CoreProjectTests)
#include "core_project_tests.moc"

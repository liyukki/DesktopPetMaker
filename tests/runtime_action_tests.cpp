#include <QtTest>
#include <QMessageBox>

#include "runtimepetwindow.h"
#include "renderbackend.h"
#include "test_support.h"

class RuntimeActionTests : public QObject
{
    Q_OBJECT

    static PetProject makeProject(const QString &root)
    {
        PetProject project;
        project.name = QStringLiteral("RuntimeFixture");
        project.projectDir = root;
        project.canvasSize = QSize(24, 32);
        project.anchor = QPoint(12, 32);
        project.patrolEnabled = false;
        QDir().mkpath(QDir(root).filePath("assets/idle"));
        QDir().mkpath(QDir(root).filePath("assets/wave"));
        TestSupport::writePng(QDir(root).filePath("assets/idle/0001.png"), QSize(24, 32), Qt::blue);
        TestSupport::writePng(QDir(root).filePath("assets/wave/0001.png"), QSize(24, 32), Qt::red);
        PetAction idle;
        idle.id = QStringLiteral("idle");
        idle.name = idle.id;
        idle.systemRole = QStringLiteral("Idle");
        idle.frames.append({QStringLiteral("assets/idle/0001.png"), {}, {}});
        project.actions.insert(idle.id, idle);
        PetAction wave;
        wave.id = QStringLiteral("wave");
        wave.name = wave.id;
        wave.allowAiTrigger = true;
        wave.mirrorSupported = true;
        wave.aiAllowedStates = {QStringLiteral("Normal")};
        wave.fps = 10;
        wave.frames.append({QStringLiteral("assets/wave/0001.png"), {}, {}});
        const QJsonObject number {{QStringLiteral("type"), QStringLiteral("number")}};
        const QJsonObject integer {{QStringLiteral("type"), QStringLiteral("integer")}};
        wave.aiParameterSchema = {
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("additionalProperties"), false},
            {QStringLiteral("properties"), QJsonObject {
                {QStringLiteral("playbackSpeed"), number},
                {QStringLiteral("mirror"), QJsonObject {{QStringLiteral("type"), QStringLiteral("boolean")}}},
                {QStringLiteral("scale"), number},
                {QStringLiteral("offsetX"), integer},
                {QStringLiteral("offsetY"), integer}
            }}
        };
        project.actions.insert(wave.id, wave);
        const auto addRoleAction = [&](const QString &id, const QString &role) {
            PetAction action;
            action.id = id;
            action.name = id;
            action.systemRole = role;
            action.frames.append({QStringLiteral("assets/idle/0001.png"), {}, {}});
            project.actions.insert(id, action);
        };
        addRoleAction(QStringLiteral("walk_left"), QStringLiteral("WalkLeft"));
        addRoleAction(QStringLiteral("drag"), QStringLiteral("Dragging"));
        addRoleAction(QStringLiteral("fall"), QStringLiteral("Falling"));
        addRoleAction(QStringLiteral("sleep"), QStringLiteral("Sleeping"));
        addRoleAction(QStringLiteral("think"), QStringLiteral("AIThinking"));
        addRoleAction(QStringLiteral("talk"), QStringLiteral("AITalking"));
        QString error;
        if (!project.save(&error)) qFatal("Fixture save failed: %s", qPrintable(error));
        return project;
    }

private slots:
    void renderBackendsShareBehaviorStatesAndDegradeSafely()
    {
        class FakeLive2DBackend final : public IRenderBackend {
        public:
            QString backendId() const override { return QStringLiteral("fake-live2d"); }
            RenderBackendAvailability availability() const override { return RenderBackendAvailability::Available; }
            RenderBackendLoadResult load(const PetProject &) override { loaded = true; return {true, {}, {}}; }
            void unload() override { loaded = false; }
            void setBehaviorState(const QString &state) override { states.append(state); }
            QString behaviorState() const override { return states.isEmpty() ? QString() : states.last(); }
            void update(double deltaSeconds) override { elapsed += deltaSeconds; }
            QPixmap render(const RenderFrameContext &context) override { return context.spriteFrame; }
            bool hitTest(const QPointF &) const override { return true; }
            QStringList states;
            bool loaded {false};
            double elapsed {0.0};
        };
        QTemporaryDir dir;
        PetProject project = makeProject(dir.path());
        auto fake = std::make_unique<FakeLive2DBackend>();
        FakeLive2DBackend *fakeObserver = fake.get();
        RuntimePetWindow window(project, {}, nullptr, std::move(fake));
        QVERIFY(fakeObserver->loaded);
        fakeObserver->states.clear();
        const QStringList states {QStringLiteral("idle"), QStringLiteral("walk_left"), QStringLiteral("drag"),
                                  QStringLiteral("fall"), QStringLiteral("sleep"), QStringLiteral("think"),
                                  QStringLiteral("talk")};
        for (const QString &state : states) {
            QCOMPARE(window.switchActionForTesting(state).status, RuntimeActionDispatchStatus::Executed);
        }
        QCOMPARE(fakeObserver->states, states);
        QCOMPARE(window.renderBackendIdForTesting(), QStringLiteral("fake-live2d"));
        Live2DRenderBackend live2d;
        QCOMPARE(live2d.availability(), RenderBackendAvailability::Disabled);
        QVERIFY(Live2DRenderBackend::availabilityText().contains(QStringLiteral("适配骨架")));
    }

    void unavailableLive2dProjectFallsBackToSpriteWithReason()
    {
        QTemporaryDir dir;
        PetProject project = makeProject(dir.path());
        project.renderBackend = QStringLiteral("live2d");
        RuntimePetWindow window(project);
        QCOMPARE(window.renderBackendIdForTesting(), QStringLiteral("sprite"));
        QVERIFY(!window.renderBackendFallbackForTesting().isEmpty());
    }

    void savedPlacementClampsAcrossScreenLayoutsAndDpiChanges()
    {
        const QSize petSize(275, 275);
        QCOMPARE(RuntimePetWindow::clampTopLeftForTesting(
                     QPoint(935, 637), petSize, {QRect(0, 0, 1920, 1040)}),
                 QPoint(935, 637));
        QCOMPARE(RuntimePetWindow::clampTopLeftForTesting(
                     QPoint(935, 912), petSize, {QRect(0, 0, 1366, 728)}),
                 QPoint(935, 453));
        QCOMPARE(RuntimePetWindow::clampTopLeftForTesting(
                     QPoint(935, 912), petSize, {QRect(0, 0, 1280, 680)}),
                 QPoint(935, 405));
        QCOMPARE(RuntimePetWindow::clampTopLeftForTesting(
                     QPoint(2300, 900), petSize, {QRect(0, 0, 1920, 1040)}),
                 QPoint(1645, 765));
        QCOMPARE(RuntimePetWindow::clampTopLeftForTesting(
                     QPoint(-1700, 700), petSize,
                     {QRect(-1920, 0, 1920, 1040), QRect(0, 0, 1920, 1040)}, 1),
                 QPoint(-1700, 700));
        QCOMPARE(RuntimePetWindow::clampTopLeftForTesting(
                     QPoint(1500, 800), QSize(413, 413), {QRect(0, 0, 1920, 1040)}),
                 QPoint(1500, 627));
    }

    void immediateExecutionUsesRenderableFrameAndParameters()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PetProject project = makeProject(dir.path());
        RuntimePetWindow window(project);
        const QPixmap idlePixmap = window.renderedPixmapForTesting();
        const RuntimeActionDispatchResult result = window.dispatchAiActionForTesting(
            QStringLiteral("wave"),
            {{QStringLiteral("playbackSpeed"), 2.0}, {QStringLiteral("mirror"), true},
             {QStringLiteral("scale"), 0.8}, {QStringLiteral("offsetX"), 2}, {QStringLiteral("offsetY"), -1}});
        QCOMPARE(result.status, RuntimeActionDispatchStatus::Executed);
        QCOMPARE(window.currentActionForTesting(), QStringLiteral("wave"));
        QCOMPARE(window.frameIntervalForTesting(), 50);
        QVERIFY(!window.renderedPixmapForTesting().isNull());
        QVERIFY(window.renderedPixmapForTesting().cacheKey() != idlePixmap.cacheKey());
    }

    void invalidAndUnreadableActionsAreRejected()
    {
        QTemporaryDir dir;
        PetProject project = makeProject(dir.path());
        RuntimePetWindow window(project);
        QCOMPARE(window.dispatchAiActionForTesting(QStringLiteral("missing")).status,
                 RuntimeActionDispatchStatus::Rejected);
        QFile::remove(QDir(dir.path()).filePath("assets/wave/0001.png"));
        QCOMPARE(window.dispatchAiActionForTesting(QStringLiteral("wave")).status,
                 RuntimeActionDispatchStatus::Rejected);
    }

    void queuedActionRevalidatesAndReportsSuperseded()
    {
        QTemporaryDir dir;
        PetProject project = makeProject(dir.path());
        RuntimePetWindow window(project);
        QSignalSpy spy(&window, &RuntimePetWindow::queuedAiActionFinished);
        window.setRuntimeStateForTesting(QStringLiteral("Dragging"));
        QCOMPARE(window.dispatchAiActionForTesting(QStringLiteral("wave")).status,
                 RuntimeActionDispatchStatus::Queued);
        QCOMPARE(window.dispatchAiActionForTesting(QStringLiteral("wave"), {{QStringLiteral("scale"), 1.1}}).status,
                 RuntimeActionDispatchStatus::Queued);
        QCOMPARE(spy.count(), 1);
        const RuntimeActionDispatchResult superseded = qvariant_cast<RuntimeActionDispatchResult>(spy.takeFirst().at(2));
        QCOMPARE(superseded.errorCode, QStringLiteral("QueuedActionSuperseded"));

        PetProject changed;
        QString error;
        QVERIFY(changed.load(project.petJsonPath(), &error));
        changed.actions[QStringLiteral("wave")].allowAiTrigger = false;
        QVERIFY(changed.save(&error));
        window.restoreNormalStateForTesting();
        QCOMPARE(spy.count(), 1);
        const RuntimeActionDispatchResult rejected = qvariant_cast<RuntimeActionDispatchResult>(spy.takeFirst().at(2));
        QCOMPARE(rejected.status, RuntimeActionDispatchStatus::Rejected);
    }

    void journalReminderYesOpensJournalAfterModalUnwinds()
    {
        QTemporaryDir dir;
        PetProject project = makeProject(dir.path());
        RuntimePetWindow window(project);
        window.show();
        QTimer::singleShot(0, []() {
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                if (auto *box = qobject_cast<QMessageBox *>(widget)) {
                    box->done(QMessageBox::Yes);
                    return;
                }
            }
        });

        window.showJournalReminderForTesting();
        QTRY_VERIFY(window.journalWindowForTesting() != nullptr);
        QTRY_VERIFY(window.journalWindowForTesting()->isVisible());
        QVERIFY(window.overlayActiveForTesting());
        window.journalWindowForTesting()->close();
        QTRY_VERIFY(window.journalWindowForTesting() == nullptr);
    }
};

QTEST_MAIN(RuntimeActionTests)
#include "runtime_action_tests.moc"

#include <QtTest>

#include "actionmaterialwindow.h"
#include "aiconversationconsolewindow.h"
#include "aiconversationroommanager.h"
#include "aidialogwindow.h"
#include "petcontrolcenterwindow.h"
#include "petspeechbubblewindow.h"
#include "runtimepetmanager.h"
#include "runtimepetwindow.h"
#include "spritesheetimportdialog.h"
#include "systemtraycontroller.h"
#include "test_support.h"
#include "ui/theme/apptheme.h"

#include <QInputDialog>
#include <QAction>
#include <QColor>
#include <QComboBox>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QLabel>
#include <QImage>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTextEdit>
#include <QWindow>

namespace {
PetProject makeGuiProject(const QString &root)
{
    PetProject project;
    project.name = QStringLiteral("GuiFixture");
    project.projectDir = root;
    project.canvasSize = QSize(24, 32);
    project.anchor = QPoint(12, 32);
    project.patrolEnabled = false;
    QDir().mkpath(QDir(root).filePath(QStringLiteral("assets/idle")));
    TestSupport::writePng(QDir(root).filePath(QStringLiteral("assets/idle/0001.png")), QSize(24, 32), Qt::blue);
    PetAction idle;
    idle.id = QStringLiteral("idle");
    idle.name = idle.id;
    idle.displayName = QStringLiteral("待机");
    idle.systemRole = QStringLiteral("Idle");
    idle.frames.append({QStringLiteral("assets/idle/0001.png"), {}, {}});
    project.actions.insert(idle.id, idle);
    PetAction wave = idle;
    wave.id = QStringLiteral("wave");
    wave.name = wave.id;
    wave.displayName = QStringLiteral("挥手");
    wave.systemRole = QStringLiteral("None");
    wave.allowAiTrigger = true;
    wave.aiAllowedStates = {QStringLiteral("Normal")};
    project.actions.insert(wave.id, wave);
    QString error;
    if (!project.save(&error)) qFatal("GUI fixture save failed: %s", qPrintable(error));
    return project;
}

void answerInputDialog(const QString &value)
{
    QTimer::singleShot(0, [value]() {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *dialog = qobject_cast<QInputDialog *>(widget)) {
                dialog->setTextValue(value);
                dialog->accept();
                return;
            }
        }
    });
}

void answerMessageBox(QMessageBox::StandardButton button)
{
    QTimer::singleShot(0, [button]() {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *box = qobject_cast<QMessageBox *>(widget)) {
                box->done(button);
                return;
            }
        }
    });
}

class IntegrationChatClient final : public IAIConversationChatClient
{
public:
    AIRequestHandle *sendMessage(const AIProviderProfile &,
                                 const AIChatRequest &,
                                 AIProvider::ResultCallback callback) override
    {
        callback({true, QStringLiteral("{\"reply\":\"集成回复\",\"action\":{\"id\":\"wave\",\"parameters\":{}}}"),
                  {}, false});
        return nullptr;
    }
};

class IntegrationParticipantResolver final : public IAIConversationParticipantResolver
{
public:
    bool resolve(AIConversationParticipant *participant, QString *) const override
    {
        return participant && !participant->participantId.isEmpty() && !participant->petProjectPath.isEmpty();
    }
};
}

class PlatformGuiTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        AppTheme::apply(*qApp);
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("DesktopPetGuiTests"));
        QCoreApplication::setApplicationName(QStringLiteral("PlatformGuiTests"));
        QSettings().clear();
    }

    void controlCenterUsesRealNavigationAndSettingsClicks()
    {
        RuntimePetManager manager;
        PetControlCenterWindow window(&manager);
        window.show();
        auto *nav = window.findChild<QListWidget *>(QStringLiteral("controlCenterNavList"));
        auto *pages = window.findChild<QStackedWidget *>(QStringLiteral("controlCenterPages"));
        QVERIFY(nav);
        QVERIFY(pages);
        const QModelIndex generalIndex = nav->model()->index(8, 0);
        QTest::mouseClick(nav->viewport(), Qt::LeftButton, Qt::NoModifier, nav->visualRect(generalIndex).center());
        QCOMPARE(pages->currentIndex(), 8);
        auto *seconds = window.findChild<QLineEdit *>(QStringLiteral("bubbleSecondsEdit"));
        auto *save = window.findChild<QPushButton *>(QStringLiteral("saveGeneralSettingsButton"));
        auto *live2d = window.findChild<QLabel *>(QStringLiteral("live2dAvailabilityLabel"));
        QVERIFY(seconds && save && live2d);
        seconds->selectAll();
        QTest::keyClicks(seconds, QStringLiteral("17"));
        QTest::mouseClick(save, Qt::LeftButton);
        QCOMPARE(QSettings().value(QStringLiteral("bubble/displaySeconds")).toInt(), 17);
        QVERIFY(live2d->text().contains(QStringLiteral("Sprite Runtime")));
    }

    void multiAiConsoleCreatesAndRenamesRoomThroughDialogs()
    {
        RuntimePetManager manager;
        AIConversationConsoleWindow window(&manager);
        window.show();
        auto *rooms = window.findChild<QListWidget *>(QStringLiteral("roomList"));
        auto *create = window.findChild<QPushButton *>(QStringLiteral("newRoomButton"));
        auto *rename = window.findChild<QPushButton *>(QStringLiteral("renameRoomButton"));
        auto *mode = window.findChild<QComboBox *>(QStringLiteral("roomModeCombo"));
        auto *minimum = window.findChild<QSpinBox *>(QStringLiteral("minRespondersSpin"));
        QVERIFY(rooms && create && rename && mode && minimum);
        const int initialCount = rooms->count();
        answerInputDialog(QStringLiteral("自动化房间"));
        QTest::mouseClick(create, Qt::LeftButton);
        QCOMPARE(rooms->count(), initialCount + 1);
        answerInputDialog(QStringLiteral("重命名房间"));
        QTest::mouseClick(rename, Qt::LeftButton);
        QVERIFY(rooms->currentItem()->text().contains(QStringLiteral("重命名房间")));
        mode->setFocus();
        QTest::keyClick(mode, Qt::Key_Down);
        minimum->setFocus();
        QTest::keyClick(minimum, Qt::Key_2);
        QVERIFY(minimum->value() >= 1);
    }

    void actionMaterialRejectsInvalidSchemaFromRealButton()
    {
        QTemporaryDir dir;
        PetProject project = makeGuiProject(dir.path());
        const QByteArray before = TestSupport::readBytes(project.petJsonPath());
        ActionMaterialWindow window;
        window.loadProject(project.petJsonPath());
        window.show();
        auto *actions = window.findChild<QListWidget *>(QStringLiteral("actionList"));
        auto *states = window.findChild<QLineEdit *>(QStringLiteral("aiAllowedStatesEdit"));
        auto *schema = window.findChild<QTextEdit *>(QStringLiteral("aiParameterSchemaEdit"));
        auto *save = window.findChild<QPushButton *>(QStringLiteral("saveActionButton"));
        QVERIFY(actions && states && schema && save);
        QVERIFY(states->isReadOnly());
        QCOMPARE(states->text(), QStringLiteral("Normal"));
        schema->setFocus();
        QTest::keyClick(schema, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClicks(schema, QStringLiteral("{\"type\":"));
        answerMessageBox(QMessageBox::Ok);
        QTest::mouseClick(save, Qt::LeftButton);
        QCOMPARE(TestSupport::readBytes(project.petJsonPath()), before);
    }

    void runtimeBubbleAndRuntimeOptionsUseRealWidgets()
    {
        QTemporaryDir dir;
        PetProject project = makeGuiProject(dir.path());
        RuntimePetWindow window(project);
        window.show();
        QSignalSpy stateSpy(&window, &RuntimePetWindow::runtimeStateChanged);
        window.setRuntimeLocked(true);
        window.setRuntimeMousePassthrough(true);
        QVERIFY(stateSpy.count() >= 2);
        PetSpeechBubbleWindow *bubble = window.speechBubble();
        QVERIFY(bubble);
        bubble->showMessage(QStringLiteral("点击气泡打开聊天"));
        QTest::mouseClick(bubble, Qt::LeftButton, Qt::NoModifier, bubble->rect().center());
        QTRY_VERIFY(window.findChild<AIDialogWindow *>() != nullptr);
        window.findChild<AIDialogWindow *>()->close();
    }

    void multiAiRuntimeIntegrationTests()
    {
        QTemporaryDir dir;
        PetProject project = makeGuiProject(QDir(dir.path()).filePath(QStringLiteral("pet")));
        RuntimePetManager runtimeManager;
        QVERIFY(runtimeManager.startPet(project.petJsonPath()));
        IntegrationChatClient client;
        IntegrationParticipantResolver resolver;
        AIConversationRoomManager roomManager(&client, 1, QDir(dir.path()).filePath(QStringLiteral("rooms.json")));
        roomManager.setParticipantResolverForTesting(&resolver);
        const QString roomId = roomManager.createRoom(QStringLiteral("Runtime integration"));
        AIConversationRoom *room = roomManager.room(roomId);
        QVERIFY(room);
        room->mode = AIConversationMode::FreeGroup;
        AIConversationParticipant participant;
        participant.participantId = QStringLiteral("pet-a");
        participant.projectId = project.effectiveProjectId();
        participant.petProjectPath = project.petJsonPath();
        participant.characterName = QStringLiteral("Pet A");
        participant.allowedActionIds = project.aiTriggerActionIds();
        participant.allowedActionDescriptors = project.aiTriggerActionDescriptors();
        room->participants.append(participant);
        connect(&roomManager, &AIConversationRoomManager::roomPetReply, &roomManager,
                [&](const QString &replyRoomId, const QString &, const QString &petPath,
                    const QString &reply, const QString &actionId, const QJsonObject &parameters,
                    const QString &messageId, const QString &roomName, const QString &userMessage) {
            const RuntimeReplyDeliveryResult delivery = runtimeManager.deliverRoomAiReply(
                petPath, reply, actionId, parameters, roomName, userMessage, replyRoomId, messageId);
            QVERIFY(roomManager.recordRuntimeDeliveryResult(replyRoomId, messageId, delivery));
        });
        QVERIFY(roomManager.submitUserMessage(roomId, QStringLiteral("执行动作")));
        room = roomManager.room(roomId);
        QVERIFY(room && !room->history.isEmpty());
        const ConversationMessage delivered = room->history.last();
        QCOMPARE(delivered.desktopDeliveryStatus, DesktopDeliveryStatus::Delivered);
        QCOMPARE(delivered.actionExecutionStatus, ActionExecutionStatus::Executed);

        runtimeManager.stopPet(project.petJsonPath());
        QTRY_VERIFY(!runtimeManager.isPetRunning(project.petJsonPath()));
        QVERIFY(roomManager.submitUserMessage(roomId, QStringLiteral("桌宠已关闭")));
        room = roomManager.room(roomId);
        QCOMPARE(room->history.last().desktopDeliveryStatus, DesktopDeliveryStatus::PetNotRunning);
    }

    void systemTrayMenuProvidesRecoveryActions()
    {
        RuntimePetManager manager;
        SystemTrayController tray(&manager);
        QMenu *menu = tray.menuForTesting();
        QVERIFY(menu);
        QVERIFY(menu->findChild<QAction *>(QStringLiteral("trayOpenControlCenterAction")));
        QVERIFY(menu->findChild<QAction *>(QStringLiteral("trayShowAllPetsAction")));
        QVERIFY(menu->findChild<QAction *>(QStringLiteral("trayPausePatrolAction")));
        QVERIFY(menu->findChild<QAction *>(QStringLiteral("trayDisablePassthroughAction")));
        QVERIFY(menu->findChild<QAction *>(QStringLiteral("trayAboutAction")));
        QVERIFY(menu->findChild<QAction *>(QStringLiteral("trayExitAction")));
        QVERIFY(SystemTrayController::controlCenterRequired(false, false, false));
        QVERIFY(SystemTrayController::controlCenterRequired(true, false, true));
        QVERIFY(SystemTrayController::controlCenterRequired(false, true, true));
        QVERIFY(!SystemTrayController::controlCenterRequired(false, false, true));
    }

    void uiVisualSnapshotsAndDpiChecks()
    {
        const QString snapshotRoot = qEnvironmentVariable("DESKTOP_PET_UI_SNAPSHOT_DIR");
        QVERIFY2(!snapshotRoot.isEmpty(), "DESKTOP_PET_UI_SNAPSHOT_DIR must be configured");
        QVERIFY(QDir().mkpath(snapshotRoot));

        RuntimePetManager manager;
        PetControlCenterWindow center(&manager);
        center.resize(1180, 760);
        center.show();
        QTest::qWait(150);
        QVERIFY(center.grab().save(QDir(snapshotRoot).filePath(QStringLiteral("control-center.png"))));
        QVERIFY(center.devicePixelRatioF() >= 1.0);
        QVERIFY(center.minimumWidth() <= center.width());
        center.close();

        AIConversationConsoleWindow console(&manager);
        console.resize(1180, 720);
        console.show();
        QTest::qWait(150);
        QVERIFY(console.grab().save(QDir(snapshotRoot).filePath(QStringLiteral("multi-ai-console.png"))));
        console.close();

        QTemporaryDir projectDir;
        QVERIFY(projectDir.isValid());
        const PetProject project = makeGuiProject(projectDir.path());
        ActionMaterialWindow actions(&manager);
        actions.loadProject(project.petJsonPath());
        actions.resize(1260, 780);
        actions.show();
        QTest::qWait(150);
        QVERIFY(actions.grab().save(QDir(snapshotRoot).filePath(QStringLiteral("action-material.png"))));
        actions.close();

        QImage sheet(256, 128, QImage::Format_ARGB32_Premultiplied);
        sheet.fill(QColor(QStringLiteral("#43BEB8")));
        const QString sheetPath = QDir(projectDir.path()).filePath(QStringLiteral("sheet.png"));
        QVERIFY(sheet.save(sheetPath));
        SpriteSheetImportDialog spriteDialog(sheetPath);
        spriteDialog.resize(980, 720);
        spriteDialog.show();
        QTest::qWait(150);
        QVERIFY(spriteDialog.grab().save(QDir(snapshotRoot).filePath(QStringLiteral("sprite-sheet.png"))));
        spriteDialog.close();

        for (QWidget *widget : QApplication::allWidgets()) {
            if (!widget->isVisible() || !widget->window()) continue;
            QVERIFY2(widget->width() >= 0 && widget->height() >= 0, "Visible widget has invalid geometry");
        }
    }
};

QTEST_MAIN(PlatformGuiTests)
#include "platform_gui_tests.moc"

#include "runtimepetwindow.h"
#include "ui/theme/iconprovider.h"
#include "renderbackend.h"
#include "screenplacementutil.h"

#include "aiactionvalidator.h"
#include "aiproviderprofileregistry.h"
#include "petairequestcoordinator.h"
#include "petspeechbubblewindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QPixmap>
#include <QRandomGenerator>
#include <QScreen>
#include <QResizeEvent>
#include <QTime>
#include <QTransform>
#include <QtMath>

#include "aiprovider.h"
#include "moodjournal.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
constexpr int kDragThreshold = 4;
constexpr int kSleepAfterMs = 4 * 60 * 1000;
constexpr int kPatrolPauseDelayMs = 3000;
constexpr int kPetChatMinDelayMs = 5 * 60 * 1000;
constexpr int kPetChatDelayRangeMs = 10 * 60 * 1000;
constexpr int kRandomIdleMinDelayMs = 12 * 1000;
constexpr int kRandomIdleDelayRangeMs = 18 * 1000;
constexpr int kRandomIdleTotalChance = 60;
constexpr int kRandomIdleSleepChance = 3;
constexpr int kSleepDurationMs = 60 * 1000;
constexpr int kSleepCooldownMs = 120 * 1000;

QList<QPointer<RuntimePetWindow>> g_reminderWindows;

RuntimePetWindow *firstLiveReminderWindow()
{
    for (auto it = g_reminderWindows.begin(); it != g_reminderWindows.end();) {
        if (it->isNull()) {
            it = g_reminderWindows.erase(it);
        } else {
            return it->data();
        }
    }
    return nullptr;
}
}

RuntimePetWindow::RuntimePetWindow(const PetProject &project,
                                   const QSharedPointer<PetConversationContext> &conversationContext,
                                   QWidget *parent,
                                   std::unique_ptr<IRenderBackend> renderBackend)
    : QMainWindow(parent)
    , m_project(project)
    , m_conversationContext(conversationContext ? conversationContext
                                                : QSharedPointer<PetConversationContext>::create())
{
    setWindowTitle(project.name);
        m_conversationContext->updateAiProfile(characterName(),
                                               m_project.aiSystemPrompt,
                                               m_project.aiProviderProfileId,
                                                m_project.aiTriggerActionIds(),
                                                m_project.aiTriggerActionDescriptors());
    m_conversationContext->effectiveProjectId = m_project.effectiveProjectId();
    m_conversationContext->petProjectPath = m_project.petJsonPath();
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_patrolEnabled = project.patrolEnabled;
    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setAttribute(Qt::WA_TranslucentBackground, true);
    setCentralWidget(m_label);
    m_label->installEventFilter(this);

    resize(scaledCanvasSize());
    if (m_project.hasRuntimeAnchorScreen) {
        move(m_project.runtimeAnchorScreen - scaledAnchor());
        ensureVisibleOnAvailableScreens(true);
    }

    applyWindowOptions();
    const RenderBackendType requestedBackend = RenderBackendFactory::typeFromString(m_project.renderBackend);
    m_renderBackend = renderBackend ? std::move(renderBackend) : RenderBackendFactory::create(requestedBackend);
    RenderBackendLoadResult backendLoad = m_renderBackend->load(m_project);
    if (!backendLoad.ok) {
        m_renderBackendFallbackReason = backendLoad.message;
        qWarning().noquote() << "Render backend fallback:" << backendLoad.errorCode << backendLoad.message;
        m_renderBackend->unload();
        m_renderBackend = RenderBackendFactory::create(RenderBackendType::Sprite);
        backendLoad = m_renderBackend->load(m_project);
    }
    preloadFrameCache();

    m_speechBubble = new PetSpeechBubbleWindow();
    connect(m_speechBubble, &PetSpeechBubbleWindow::bubbleClicked, this, [this]() {
        if (m_speechBubble) {
            m_speechBubble->hideBubble();
        }
        openChatWindow();
    });
    connect(this, &RuntimePetWindow::petGeometryChanged,
            m_speechBubble, &PetSpeechBubbleWindow::updateBubblePosition);
    m_speechBubble->updateBubblePosition(frameGeometry());

    m_frameTimer = new QTimer(this);
    connect(m_frameTimer, &QTimer::timeout, this, &RuntimePetWindow::advanceFrame);

    m_idleTimer = new QTimer(this);
    m_idleTimer->setInterval(1000);
    connect(m_idleTimer, &QTimer::timeout, this, &RuntimePetWindow::checkIdleSleep);
    m_idleTimer->start();

    m_patrolTimer = new QTimer(this);
    m_patrolTimer->setInterval(16);
    connect(m_patrolTimer, &QTimer::timeout, this, &RuntimePetWindow::onPatrolTimer);

    m_patrolStateTimer = new QTimer(this);
    m_patrolStateTimer->setSingleShot(true);
    connect(m_patrolStateTimer, &QTimer::timeout, this, [this]() {
        if (m_patrolState == PatrolState::PatrolPause && !m_dragging) {
            switchToPatrolIdle();
            m_patrolTimer->start();
            scheduleNextWalk();
        }
    });

    m_physicsTimer = new QTimer(this);
    m_physicsTimer->setInterval(16);
    connect(m_physicsTimer, &QTimer::timeout, this, &RuntimePetWindow::onPhysicsTimer);

    m_followClickTimer = new QTimer(this);
    m_followClickTimer->setInterval(30);
    connect(m_followClickTimer, &QTimer::timeout, this, &RuntimePetWindow::pollFollowClick);

    m_followMoveTimer = new QTimer(this);
    m_followMoveTimer->setInterval(16);
    connect(m_followMoveTimer, &QTimer::timeout, this, &RuntimePetWindow::onFollowMoveTimer);

    m_petPrompts = {
        QString::fromUtf8("\xE5\x88\x9A\xE5\x88\x9A\xE6\x83\xB3\xE5\x88\xB0\xE4\xBD\xA0\xE5\x95\xA6\xEF\xBC\x8C\xE4\xBB\x8A\xE5\xA4\xA9\xE8\xBF\x87\xE5\xBE\x97\xE6\x80\x8E\xE4\xB9\x88\xE6\xA0\xB7\xEF\xBC\x9F"),
        QString::fromUtf8("\xE5\xB0\x8F\xE5\xB0\x8F\xE6\x89\x93\xE5\x8D\xA1\xEF\xBC\x9A\xE4\xBB\x8A\xE5\xA4\xA9\xE6\x9C\x89\xE4\xBB\x80\xE4\xB9\x88\xE5\xBC\x80\xE5\xBF\x83\xE7\x9A\x84\xE4\xBA\x8B\xE5\x90\x97\xEF\xBC\x9F"),
        QString::fromUtf8("\xE8\xA6\x81\xE4\xB8\x8D\xE8\xA6\x81\xE8\xB6\x81\xE8\xBF\x98\xE8\xAE\xB0\xE5\xBE\x97\xEF\xBC\x8C\xE5\x86\x99\xE4\xB8\x8B\xE4\xB8\x80\xE5\x8F\xA5\xE8\xAF\x9D\xEF\xBC\x9F"),
        QString::fromUtf8("\xE6\x88\x91\xE6\x9C\x89\xE7\x82\xB9\xE5\xA5\xBD\xE5\xA5\x87\xEF\xBC\x8C\xE4\xBB\x8A\xE5\xA4\xA9\xE7\x9A\x84\xE5\xBF\x83\xE6\x83\x85\xE6\x98\xAF\xE4\xBB\x80\xE4\xB9\x88\xE9\xA2\x9C\xE8\x89\xB2\xEF\xBC\x9F"),
        QString::fromUtf8("\xE8\xAE\xB0\xE5\xBE\x97\xE7\xA8\x8D\xE5\xBE\xAE\xE6\xB4\xBB\xE5\x8A\xA8\xE4\xB8\x80\xE4\xB8\x8B\xE8\x82\xA9\xE8\x86\x80\xE5\x93\xA6\xE3\x80\x82")
    };
    setupPetChat();
    setupJournalReminder();
    setupRandomIdleActions();

    m_sleepTimer = new QTimer(this);
    m_sleepTimer->setSingleShot(true);
    connect(m_sleepTimer, &QTimer::timeout, this, &RuntimePetWindow::finishSleep);

    m_lastInteraction.start();
    switchState(actionIdForRole(SystemActionRole::Idle, QStringLiteral("idle")));
    QTimer::singleShot(5000, this, [this]() {
        if (m_physicsState == PhysicsState::Normal && !m_dragging && !m_chatWindow && !m_journalWindow) {
            startPatrol();
        }
    });
}

bool RuntimePetWindow::randomIdleRuleMatchesForTest(const PetBehaviorRule &rule,
                                                    bool actionHasFrames,
                                                    bool onGround,
                                                    bool idle,
                                                    bool aiBusy,
                                                    qint64 elapsedMs,
                                                    qint64 lastTriggeredMs)
{
    return randomIdleRuleMatchesConditions(rule, actionHasFrames, onGround, idle, aiBusy, elapsedMs, lastTriggeredMs);
}

RuntimePetWindow::~RuntimePetWindow()
{
    m_runtimeClosing = true;
    cancelProactiveRequest(QStringLiteral("RuntimeWindowDestroyed"));
}

PetSpeechBubbleWindow *RuntimePetWindow::speechBubble() const
{
    return m_speechBubble.data();
}

void RuntimePetWindow::closeEvent(QCloseEvent *event)
{
    m_runtimeClosing = true;
    cancelProactiveRequest(QStringLiteral("RuntimeWindowClosed"));
    if (m_speechBubble) {
        m_speechBubble->hideBubble();
        m_speechBubble->deleteLater();
        m_speechBubble = nullptr;
    }
    g_reminderWindows.removeAll(QPointer<RuntimePetWindow>(this));
    saveRuntimePlacement();
    QMainWindow::closeEvent(event);
}

void RuntimePetWindow::placeOnGroundAtX(int x)
{
    const QRect screen = usableScreenRect();
    move(qBound(minAnchorBoundX(screen), x, maxAnchorBoundX(screen)), groundWindowY(screen));
    emit petGeometryChanged(frameGeometry());
}

bool RuntimePetWindow::ensureVisibleOnAvailableScreens(bool persistCorrection)
{
    QVector<QRect> availableScreens;
    int primaryIndex = 0;
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (int index = 0; index < screens.size(); ++index) {
        QScreen *screen = screens.at(index);
        availableScreens.append(screen->availableGeometry());
        if (screen == QGuiApplication::primaryScreen()) {
            primaryIndex = index;
        }
    }
    if (availableScreens.isEmpty()) {
        availableScreens.append(QRect(0, 0, 1920, 1080));
    }

    const QPoint corrected = ScreenPlacementUtil::clampTopLeft(
        frameGeometry().topLeft(), size(), availableScreens, primaryIndex);
    if (corrected == frameGeometry().topLeft()) {
        return false;
    }

    move(corrected);
    emit petGeometryChanged(frameGeometry());
    if (persistCorrection) {
        m_project.runtimeAnchorScreen = corrected + scaledAnchor();
        m_project.hasRuntimeAnchorScreen = true;
        RuntimeStatePatch patch;
        patch.hasAnchorScreen = true;
        patch.runtimeAnchorScreen = m_project.runtimeAnchorScreen;
        saveRuntimeStatePatch(patch);
    }
    return true;
}

#ifdef DESKTOP_PET_TESTING
QPoint RuntimePetWindow::clampTopLeftForTesting(const QPoint &desiredTopLeft,
                                                const QSize &windowSize,
                                                const QVector<QRect> &availableScreens,
                                                int primaryScreenIndex)
{
    return ScreenPlacementUtil::clampTopLeft(
        desiredTopLeft, windowSize, availableScreens, primaryScreenIndex);
}
#endif

void RuntimePetWindow::setPetSwitchOptions(const QVector<QPair<QString, QString>> &options)
{
    m_petSwitchOptions = options;
}

void RuntimePetWindow::setRuntimeLocked(bool locked)
{
    if (m_project.locked == locked) {
        return;
    }
    const bool wasLocked = m_project.locked;
    m_project.locked = locked;
    RuntimeStatePatch patch;
    patch.hasLocked = true;
    patch.locked = locked;
    saveRuntimeStatePatch(patch);

    if (locked && !wasLocked) {
        stopPatrol();
        cancelClickFollow();
        if (m_followMoveTimer && m_followMoveTimer->isActive()) {
            m_followMoveTimer->stop();
            m_isWalking = false;
        }
        if (m_physicsState == PhysicsState::Normal && !m_dragging) {
            restorePassiveStateIfSafe();
        }
    } else if (!locked && wasLocked) {
        if (m_physicsState == PhysicsState::Normal && !m_dragging && !m_overlayActive
            && !m_chatWindow && !m_journalWindow) {
            resumePatrolLater();
        }
    }
}

void RuntimePetWindow::setRuntimeMousePassthrough(bool mousePassthrough)
{
    if (m_project.mousePassthrough == mousePassthrough) {
        return;
    }
    m_project.mousePassthrough = mousePassthrough;
    applyMousePassthrough();
    RuntimeStatePatch patch;
    patch.hasMousePassthrough = true;
    patch.mousePassthrough = mousePassthrough;
    saveRuntimeStatePatch(patch);
}

void RuntimePetWindow::setRuntimeTopMost(bool topMost)
{
    if (m_project.topMost == topMost) {
        return;
    }
    m_project.topMost = topMost;
    applyWindowOptions();
    RuntimeStatePatch patch;
    patch.hasTopMost = true;
    patch.topMost = topMost;
    saveRuntimeStatePatch(patch);
}

void RuntimePetWindow::setRuntimePatrolEnabled(bool enabled)
{
    if (m_patrolEnabled == enabled && m_project.patrolEnabled == enabled) {
        return;
    }
    m_patrolEnabled = enabled;
    m_project.patrolEnabled = enabled;
    RuntimeStatePatch patch;
    patch.hasPatrolEnabled = true;
    patch.patrolEnabled = enabled;
    saveRuntimeStatePatch(patch);

    if (!enabled) {
        stopPatrol();
        return;
    }
    if (m_physicsState == PhysicsState::Normal && !m_dragging && !m_overlayActive
        && !m_chatWindow && !m_journalWindow) {
        resumePatrolLater();
    }
}

void RuntimePetWindow::reloadAiRoleFromProject()
{
    PetProject latest;
    if (!latest.load(m_project.petJsonPath())) {
        return;
    }
    m_project.aiCharacterName = latest.aiCharacterName;
    m_project.aiSystemPrompt = latest.aiSystemPrompt;
    m_project.aiProviderProfileId = latest.aiProviderProfileId;
    m_conversationContext->updateAiProfile(characterName(),
                                           m_project.aiSystemPrompt,
                                           m_project.aiProviderProfileId,
                                           m_project.aiTriggerActionIds(),
                                           m_project.aiTriggerActionDescriptors());
}

void RuntimePetWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    QAction *lockAction = menu.addAction(QString::fromUtf8("\xE9\x94\x81\xE5\xAE\x9A\xE4\xBD\x8D\xE7\xBD\xAE"));
    lockAction->setIcon(IconProvider::icon(AppIcon::Runtime));
    lockAction->setCheckable(true);
    lockAction->setChecked(m_project.locked);

    QAction *passthroughAction = menu.addAction(QString::fromUtf8("\xE9\xBC\xA0\xE6\xA0\x87\xE7\xA9\xBF\xE9\x80\x8F"));
    passthroughAction->setIcon(IconProvider::icon(AppIcon::Locate));
    passthroughAction->setCheckable(true);
    passthroughAction->setChecked(m_project.mousePassthrough);

    QAction *topMostAction = menu.addAction(QString::fromUtf8("\xE7\xAA\x97\xE5\x8F\xA3\xE7\xBD\xAE\xE9\xA1\xB6"));
    topMostAction->setIcon(IconProvider::icon(AppIcon::Home));
    topMostAction->setCheckable(true);
    topMostAction->setChecked(m_project.topMost);

    menu.addSeparator();
    QAction *aiSettingsAction = menu.addAction(QString::fromUtf8("\x41\x49\x20\xE8\xAE\xBE\xE7\xBD\xAE"));
    QAction *chatAction = menu.addAction(QString::fromUtf8("\xE4\xB8\x8E\xE6\xA1\x8C\xE5\xAE\xA0\xE8\x81\x8A\xE5\xA4\xA9"));
    QAction *journalAction = menu.addAction(QString::fromUtf8("\xE5\xBF\x83\xE6\x83\x85\xE6\x97\xA5\xE8\xAE\xB0"));
    QAction *controlCenterAction = menu.addAction(QStringLiteral("打开桌宠管理中心"));
    QAction *stopThisPetAction = menu.addAction(QStringLiteral("停止此桌宠"));
    aiSettingsAction->setIcon(IconProvider::icon(AppIcon::Cloud));
    chatAction->setIcon(IconProvider::icon(AppIcon::Chat));
    journalAction->setIcon(IconProvider::icon(AppIcon::Edit));
    controlCenterAction->setIcon(IconProvider::icon(AppIcon::Home));
    stopThisPetAction->setIcon(IconProvider::icon(AppIcon::Stop));

    menu.addSeparator();
    QHash<QAction *, QString> switchActions;
    if (!m_petSwitchOptions.isEmpty()) {
        QMenu *switchMenu = menu.addMenu(QStringLiteral("切换桌宠"));
        switchMenu->setIcon(IconProvider::icon(AppIcon::Pet));
        for (const auto &option : m_petSwitchOptions) {
            QAction *switchAction = switchMenu->addAction(option.first);
            switchAction->setCheckable(true);
            switchAction->setChecked(QDir::cleanPath(option.second) == QDir::cleanPath(m_project.petJsonPath()));
            switchActions.insert(switchAction, option.second);
        }
        menu.addSeparator();
    }

    QMenu *actionMenu = menu.addMenu(QString::fromUtf8("\xE5\x8A\xA8\xE4\xBD\x9C"));
    actionMenu->setIcon(IconProvider::icon(AppIcon::Motion));
    QAction *idleAction = actionMenu->addAction(QString::fromUtf8("\xE6\x81\xA2\xE5\xA4\x8D\xE5\xBE\x85\xE6\x9C\xBA"));
    QAction *sleepAction = hasStateFrames(actionIdForRole(SystemActionRole::Sleeping, QStringLiteral("sleep")))
        ? actionMenu->addAction(QString::fromUtf8("\xE7\x9D\xA1\xE7\x9C\xA0\x20\x31\x20\xE5\x88\x86\xE9\x92\x9F"))
        : nullptr;
    QHash<QAction *, QString> manualActions;
    QHash<QString, QMenu *> groupMenus;
    for (auto it = m_project.actions.constBegin(); it != m_project.actions.constEnd(); ++it) {
        const QString actionId = it.key();
        const PetAction &action = it.value();
        if (!action.showInContextMenu || !hasStateFrames(actionId)) {
            continue;
        }
        QMenu *targetMenu = actionMenu;
        if (!action.menuGroup.trimmed().isEmpty()) {
            if (!groupMenus.contains(action.menuGroup)) {
                groupMenus.insert(action.menuGroup, actionMenu->addMenu(action.menuGroup));
            }
            targetMenu = groupMenus.value(action.menuGroup);
        }
        const QString label = action.displayName.trimmed().isEmpty() ? actionId : action.displayName.trimmed();
        manualActions.insert(targetMenu->addAction(label), actionId);
    }

    menu.addSeparator();
    QAction *exitAction = menu.addAction(QStringLiteral("退出程序"));
    exitAction->setIcon(IconProvider::icon(AppIcon::Stop));

    QAction *chosen = menu.exec(event->globalPos());
    if (!chosen) {
        return;
    }

    if (chosen == lockAction) {
        setRuntimeLocked(lockAction->isChecked());
    } else if (chosen == passthroughAction) {
        setRuntimeMousePassthrough(passthroughAction->isChecked());
    } else if (chosen == topMostAction) {
        setRuntimeTopMost(topMostAction->isChecked());
    } else if (chosen == aiSettingsAction) {
        emit controlCenterRequested();
    } else if (chosen == chatAction) {
        openChatWindow();
    } else if (chosen == journalAction) {
        openJournalWindow();
    } else if (chosen == controlCenterAction) {
        emit controlCenterRequested();
    } else if (chosen == stopThisPetAction) {
        close();
    } else if (switchActions.contains(chosen)) {
        emit switchPetRequested(switchActions.value(chosen));
    } else if (chosen == idleAction) {
        wakeFromSleep();
        stopPatrol();
        cancelClickFollow();
        switchState(actionIdForRole(SystemActionRole::Idle, QStringLiteral("idle")));
        resumePatrolLater();
    } else if (sleepAction && chosen == sleepAction) {
        rememberInteraction();
        enterSleepState(true);
    } else if (manualActions.contains(chosen)) {
        rememberInteraction();
        wakeFromSleep();
        stopPatrol();
        cancelClickFollow();
        if (m_followMoveTimer->isActive()) {
            m_followMoveTimer->stop();
            m_isWalking = false;
        }
        switchState(manualActions.value(chosen));
    } else if (chosen == exitAction) {
        emit exitApplicationRequested();
    }

    event->accept();
}


bool RuntimePetWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_label) {
        if (event->type() == QEvent::Enter) {
            if (m_patrolActive && !m_dragging && m_physicsState == PhysicsState::Normal) {
                m_patrolStateTimer->stop();
                if (m_isWalking) {
                    switchState(actionIdForRole(SystemActionRole::Idle, QStringLiteral("idle")));
                }
                m_patrolState = PatrolState::PatrolPause;
                m_patrolTimer->stop();
            }
        } else if (event->type() == QEvent::Leave) {
            if (m_patrolActive && m_patrolState == PatrolState::PatrolPause && !m_dragging) {
                m_patrolStateTimer->start(kPatrolPauseDelayMs);
            }
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void RuntimePetWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QMainWindow::mousePressEvent(event);
        return;
    }

    rememberInteraction();
    wakeFromSleep();
    MoodJournal::instance().addTodayEvent(QStringLiteral("点击了桌宠"));
    cancelClickFollow();
    if (m_followMoveTimer->isActive()) {
        m_followMoveTimer->stop();
        m_isWalking = false;
    }
    if (m_physicsState != PhysicsState::Normal && m_physicsState != PhysicsState::Dragging) {
        m_physicsTimer->stop();
        m_physicsState = PhysicsState::Normal;
        m_bounceCount = 0;
        m_velocityX = 0.0;
        m_velocityY = 0.0;
    }
    if (m_patrolActive) {
        stopPatrol();
    }
    m_dragging = !m_project.locked;
    m_dragStarted = false;
    m_pressGlobal = event->globalPosition().toPoint();
    m_dragOffset = m_pressGlobal - frameGeometry().topLeft();
    if (m_dragging) {
        m_physicsState = PhysicsState::Dragging;
    }
    switchState(actionIdForRole(SystemActionRole::ClickReaction, QStringLiteral("click")));
    event->accept();
}

void RuntimePetWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging || !(event->buttons() & Qt::LeftButton)) {
        QMainWindow::mouseMoveEvent(event);
        return;
    }

    rememberInteraction();
    const QPoint globalPos = event->globalPosition().toPoint();
    if (!m_dragStarted && (globalPos - m_pressGlobal).manhattanLength() >= kDragThreshold) {
        m_dragStarted = true;
        switchState(actionIdForRole(SystemActionRole::Dragging, QStringLiteral("drag")));
    }

    if (m_dragStarted) {
        move(globalPos - m_dragOffset);
    }
    event->accept();
}

void RuntimePetWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QMainWindow::mouseReleaseEvent(event);
        return;
    }

    rememberInteraction();
    MoodJournal::instance().addTodayEvent(m_dragStarted ? QStringLiteral("拖拽了桌宠")
                                                        : QStringLiteral("轻点了桌宠"));
    if (m_dragStarted) {
        calculateReleaseVelocity();
        startFalling();
    } else {
        switchState(actionIdForRole(SystemActionRole::ClickReaction, QStringLiteral("click")));
        m_physicsState = PhysicsState::Normal;
    }

    if (m_patrolActive) {
        stopPatrol();
    }
    const bool armFollow = !m_dragStarted && !m_project.locked;
    m_dragging = false;
    m_dragStarted = false;
    if (!m_physicsTimer->isActive()) {
        saveRuntimePlacement();
    }
    if (armFollow) {
        armClickFollow();
    }
    event->accept();
}

void RuntimePetWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    emit petGeometryChanged(frameGeometry());
}

void RuntimePetWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    emit petGeometryChanged(frameGeometry());
}

void RuntimePetWindow::applyWindowOptions()
{
    const bool visible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, m_project.topMost);
    if (visible) {
        show();
    }
    applyMousePassthrough();
}

void RuntimePetWindow::applyMousePassthrough()
{
#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(winId());
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (m_project.mousePassthrough) {
        exStyle |= WS_EX_TRANSPARENT;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
#endif
}

RuntimeActionDispatchResult RuntimePetWindow::switchState(const QString &stateName)
{
    m_state = fallbackState(stateName);
    if (m_renderBackend) m_renderBackend->setBehaviorState(m_state);
    if (!actionHasRenderableFrame(m_state)) {
        return {RuntimeActionDispatchStatus::Failed,
                QStringLiteral("ActionFrameUnavailable"),
                QStringLiteral("The action could not enter a state with a renderable first frame.")};
    }
    if (!m_activeAiActionId.isEmpty() && m_state != m_activeAiActionId) {
        clearAiActionParameters();
    }
    m_frameIndex = 0;

    const PetAction *action = currentAction();
    Q_UNUSED(action);
    m_frameTimer->start(currentFrameDurationMs());
    renderFrame();
    return {RuntimeActionDispatchStatus::Executed, {}, QStringLiteral("The action entered a renderable runtime state.")};
}

bool RuntimePetWindow::actionHasRenderableFrame(const QString &stateName) const
{
    const auto it = m_frameCache.constFind(stateName);
    if (it == m_frameCache.constEnd() || it.value().isEmpty()) return false;
    return std::all_of(it.value().cbegin(), it.value().cend(), [](const QPixmap &frame) { return !frame.isNull(); });
}

void RuntimePetWindow::advanceFrame()
{
    if (m_renderBackend) m_renderBackend->update(qMax(1, currentFrameDurationMs()) / 1000.0);
    const PetAction *action = currentAction();
    if (!action || action->frames.isEmpty()) {
        return;
    }

    ++m_frameIndex;
    const int frameCount = cachedFrameCount(m_state);
    if (m_frameIndex >= frameCount) {
        const QString idleState = idleActionId();
        const QString sleepState = m_project.actionForRole(SystemActionRole::Sleeping, QStringLiteral("sleep"));
        if (m_state == sleepState) {
            m_frameIndex = qMax(0, frameCount - 1);
            renderFrame();
            m_frameTimer->start(currentFrameDurationMs());
            return;
        }
        if (action->loop) {
            m_frameIndex = 0;
        } else {
            if (m_aiGenerating && !m_aiAnimationState.isEmpty() && m_state == m_aiAnimationState) {
                m_frameIndex = 0;
                renderFrame();
                m_frameTimer->start(currentFrameDurationMs());
                return;
            }
            const QString nextState = action->nextActionId.isEmpty() ? idleState : action->nextActionId;
            if (m_physicsState != PhysicsState::Normal && nextState == idleState) {
                m_frameIndex = action->frames.size() - 1;
                renderFrame();
                m_frameTimer->start(currentFrameDurationMs());
                return;
            }

            switchState(nextState);
            if (nextState == idleState && m_patrolEnabled && !m_patrolActive
                && m_physicsState == PhysicsState::Normal && !m_dragging) {
                startPatrol();
            }
            return;
        }
    }

    renderFrame();
    m_frameTimer->start(currentFrameDurationMs());
}

int RuntimePetWindow::currentFrameDurationMs() const
{
    const PetAction *action = currentAction();
    if (!action) {
        return qMax(1, 1000 / 8);
    }
    const auto cachedDurations = m_frameDurationCache.constFind(m_state);
    if (cachedDurations != m_frameDurationCache.constEnd()
        && m_frameIndex >= 0 && m_frameIndex < cachedDurations.value().size()) {
        const int base = qMax(1, cachedDurations.value().value(m_frameIndex));
        const double speed = m_state == m_activeAiActionId
            ? m_activeAiActionParameters.value(QStringLiteral("playbackSpeed")).toDouble(1.0)
            : 1.0;
        return qMax(1, qRound(base / qBound(0.25, speed, 4.0)));
    }
    if (action->frameDurationsMs.size() == action->frames.size()
        && m_frameIndex >= 0 && m_frameIndex < action->frameDurationsMs.size()) {
        const int base = qMax(1, action->frameDurationsMs.value(m_frameIndex));
        const double speed = m_state == m_activeAiActionId
            ? m_activeAiActionParameters.value(QStringLiteral("playbackSpeed")).toDouble(1.0)
            : 1.0;
        return qMax(1, qRound(base / qBound(0.25, speed, 4.0)));
    }
    const int base = qMax(1, 1000 / qMax(1, action->fps));
    const double speed = m_state == m_activeAiActionId
        ? m_activeAiActionParameters.value(QStringLiteral("playbackSpeed")).toDouble(1.0)
        : 1.0;
    return qMax(1, qRound(base / qBound(0.25, speed, 4.0)));
}

void RuntimePetWindow::renderFrame()
{
    const PetAction *action = currentAction();
    if (!action || action->frames.isEmpty()) {
        m_label->setText(QStringLiteral("No frames\n%1").arg(m_state));
        m_label->setPixmap(QPixmap());
        return;
    }

    const QPixmap *pixmap = cachedFrame(m_state, m_frameIndex);
    if (!pixmap || pixmap->isNull()) {
        m_label->setText(QStringLiteral("Missing frame\n%1").arg(m_state));
        m_label->setPixmap(QPixmap());
        return;
    }

    m_label->setText(QString());
    const QJsonObject parameters = m_state == m_activeAiActionId ? m_activeAiActionParameters : QJsonObject();
    RenderFrameContext context;
    context.spriteFrame = *pixmap;
    context.actionParameters = parameters;
    context.targetSize = pixmap->size();
    m_label->setPixmap(m_renderBackend ? m_renderBackend->render(context) : *pixmap);
}

void RuntimePetWindow::preloadFrameCache()
{
    m_frameCache.clear();
    m_frameDurationCache.clear();

    for (auto it = m_project.actions.constBegin(); it != m_project.actions.constEnd(); ++it) {
        const QString actionName = it.key();
        const PetAction &action = it.value();
        QVector<QPixmap> frames;
        QVector<int> durations = action.frameDurationsMs.size() == action.frames.size()
            ? action.frameDurationsMs
            : QVector<int>();
        frames.reserve(action.frames.size());

        for (const PetFrame &frame : action.frames) {
            QPixmap source(m_project.absolutePathFor(frame.path));
            if (source.isNull()) {
                frames.append(QPixmap());
                continue;
            }

            QPixmap composed(m_project.canvasSize);
            composed.fill(Qt::transparent);
            {
                QPainter painter(&composed);
                painter.drawPixmap(action.offset + frame.offset + frame.autoOffset, source);
            }

            frames.append(composed.scaled(scaledCanvasSize(),
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
        }

        if (isRandomIdleAction(actionName) && frames.size() > 2) {
            const int originalSize = frames.size();
            frames.reserve(originalSize * 2 - 2);
            if (!durations.isEmpty()) {
                durations.reserve(originalSize * 2 - 2);
            }
            for (int i = originalSize - 2; i > 0; --i) {
                frames.append(frames.at(i));
                if (!durations.isEmpty()) {
                    durations.append(durations.at(i));
                }
            }
        }

        m_frameCache.insert(actionName, frames);
        if (!durations.isEmpty()) {
            m_frameDurationCache.insert(actionName, durations);
        }
    }
}

int RuntimePetWindow::cachedFrameCount(const QString &stateName) const
{
    const auto it = m_frameCache.constFind(stateName);
    if (it != m_frameCache.constEnd() && !it.value().isEmpty()) {
        return it.value().size();
    }

    const auto actionIt = m_project.actions.constFind(stateName);
    return actionIt != m_project.actions.constEnd() ? actionIt.value().frames.size() : 0;
}

const QPixmap *RuntimePetWindow::cachedFrame(const QString &stateName, int frameIndex) const
{
    const auto it = m_frameCache.constFind(stateName);
    if (it == m_frameCache.constEnd() || it.value().isEmpty()) {
        return nullptr;
    }

    return &it.value().at(qBound(0, frameIndex, it.value().size() - 1));
}

void RuntimePetWindow::checkIdleSleep()
{
    if (m_dragging || m_overlayActive || currentStateHasRole(SystemActionRole::Sleeping) || m_chatWindow || m_journalWindow
        || m_physicsState != PhysicsState::Normal) {
        return;
    }

    if (m_patrolActive && m_patrolState != PatrolState::PatrolIdle) {
        return;
    }

    if (m_lastInteraction.elapsed() >= kSleepAfterMs) {
        enterSleepState();
    }
}

void RuntimePetWindow::pauseForOverlay()
{
    m_overlayActive = true;
    stopPatrol();
    cancelClickFollow();
    if (m_randomIdleTimer) {
        m_randomIdleTimer->stop();
    }
    if (m_petChatTimer) {
        m_petChatTimer->stop();
    }
    if (m_followMoveTimer->isActive()) {
        m_followMoveTimer->stop();
        m_isWalking = false;
    }
}

void RuntimePetWindow::resumePatrolLater(int delayMs)
{
    m_overlayActive = false;
    scheduleNextPetChat();
    QTimer::singleShot(delayMs, this, [this]() {
        if (!m_chatWindow && !m_journalWindow && !m_overlayActive && m_physicsState == PhysicsState::Normal && !m_dragging) {
            startPatrol();
            scheduleNextRandomIdleAction();
        }
    });
}

void RuntimePetWindow::openChatWindow()
{
    if (m_speechBubble) {
        m_speechBubble->hideBubble();
    }
    pauseForOverlay();
    switchState(actionIdForRole(SystemActionRole::Idle, QStringLiteral("idle")));

    const QString characterName = m_project.aiCharacterName.trimmed().isEmpty()
        ? m_project.name
        : m_project.aiCharacterName.trimmed();
    if (!m_chatWindow) {
        m_chatWindow = new AIDialogWindow(characterName, m_project.aiSystemPrompt, m_conversationContext, this);
        connect(m_chatWindow, &AIDialogWindow::messageSent, this, [this](const QString &) {
            rememberInteraction();
            MoodJournal::instance().incrementChatCount();
        });
        connect(m_chatWindow, &AIDialogWindow::aiGenerationStarted, this, [this]() {
            beginAiRequestAnimation();
        });
        connect(m_chatWindow, &AIDialogWindow::aiGenerationFinished, this, [this]() {
            endAiRequestAnimationIfIdle();
        });
        connect(m_chatWindow, &AIDialogWindow::aiActionRequested, this, [this](const QString &actionId, const QJsonObject &parameters) {
            const RuntimeActionDispatchResult result = queueOrRunAiAction(actionId, parameters);
            showActionDispatchFeedback(actionId, result);
        });
        connect(m_chatWindow, &QObject::destroyed, this, [this]() {
            m_chatWindow = nullptr;
            if (m_runtimeClosing) return;
            rememberInteraction();
            endAiRequestAnimationIfIdle();
            restorePassiveStateIfSafe();
            resumePatrolLater();
        });
    }

    m_chatWindow->setWindowTitle(tr("%1").arg(characterName));
    m_chatWindow->show();
    m_chatWindow->raise();
    m_chatWindow->activateWindow();
}

void RuntimePetWindow::openJournalWindow()
{
    rememberInteraction();
    pauseForOverlay();

    if (!m_journalWindow) {
        m_journalWindow = new JournalWindow(this);
        m_journalWindow->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_journalWindow, &QObject::destroyed, this, [this]() {
            m_journalWindow = nullptr;
            if (m_runtimeClosing) return;
            resumePatrolLater();
        });
    }

    m_journalWindow->show();
    m_journalWindow->raise();
    m_journalWindow->activateWindow();
}

void RuntimePetWindow::setupPetChat()
{
    m_petChatTimer = new QTimer(this);
    m_petChatTimer->setSingleShot(true);
    connect(m_petChatTimer, &QTimer::timeout, this, &RuntimePetWindow::onPetChatTimer);
    scheduleNextPetChat();
}

void RuntimePetWindow::scheduleNextPetChat()
{
    if (!m_petChatTimer) {
        return;
    }

    const int delay = kPetChatMinDelayMs + QRandomGenerator::global()->bounded(kPetChatDelayRangeMs + 1);
    m_petChatTimer->start(delay);
}

void RuntimePetWindow::onPetChatTimer()
{
    if (!m_dragging && m_physicsState == PhysicsState::Normal && !m_overlayActive
        && !m_chatWindow && !m_journalWindow && !m_petPrompts.isEmpty()
        && !m_aiGenerating && !m_conversationContext->busy() && !m_followMoveTimer->isActive()
        && !currentStateHasRole(SystemActionRole::Sleeping)
        && !(m_patrolActive && m_patrolState == PatrolState::PatrolWalk)) {
        const QString prompt = m_petPrompts.at(QRandomGenerator::global()->bounded(m_petPrompts.size()));
        requestProactivePetMessage(prompt);
        MoodJournal::instance().addTodayEvent(QStringLiteral("桌宠发起了聊天"));
    }

    scheduleNextPetChat();
}

void RuntimePetWindow::setupJournalReminder()
{
    const QPointer<RuntimePetWindow> self(this);
    if (!g_reminderWindows.contains(self)) {
        g_reminderWindows.append(self);
    }
    connect(&MoodJournal::instance(), &MoodJournal::journalReminderDue, this, [this]() {
        if (firstLiveReminderWindow() == this && MoodJournal::instance().claimReminderForToday()) {
            showJournalReminder();
        }
    });
    MoodJournal::instance().startJournalReminder();
}

void RuntimePetWindow::showJournalReminder()
{
    pauseForOverlay();
    QMessageBox messageBox(this);
    messageBox.setWindowTitle(QStringLiteral("心情日记"));
    messageBox.setText(QStringLiteral("现在要写一条心情日记吗？"));
    messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    messageBox.setDefaultButton(QMessageBox::Yes);

    if (messageBox.exec() == QMessageBox::Yes) {
        // Create the journal after the modal reminder has fully unwound.
        QTimer::singleShot(0, this, [this]() { openJournalWindow(); });
    } else {
        resumePatrolLater();
    }
}

QString RuntimePetWindow::characterName() const
{
    const QString configuredName = m_project.aiCharacterName.trimmed();
    return configuredName.isEmpty() ? m_project.name : configuredName;
}

void RuntimePetWindow::requestProactivePetMessage(const QString &prompt)
{
    if (m_conversationContext->busy()) {
        return;
    }

    AIProviderProfileRegistry profiles;
    const ProviderLookupResult resolved = profiles.resolveStrict(m_project.aiProviderProfileId);
    if (!resolved.ok) {
        if (m_chatWindow) {
            m_chatWindow->displaySystemMessage(QStringLiteral("AI 主动消息未发送：%1").arg(resolved.message));
        }
        return;
    }

    QString leaseError;
    m_proactiveAiLease = PetAiRequestCoordinator::instance().acquire(m_project.effectiveProjectId(),
                                                                       m_project.petJsonPath(),
                                                                       PetAiRequestSource::ProactiveChat,
                                                                       {},
                                                                       &leaseError);
    if (!m_proactiveAiLease) {
        if (m_chatWindow) {
            m_chatWindow->displaySystemMessage(QStringLiteral("AI 主动消息未发送：%1").arg(leaseError));
        }
        return;
    }

    m_conversationContext->beginRequest();
    m_proactiveRequestActive = true;
    if (m_chatWindow) {
        m_chatWindow->setExternalBusy(true);
    }
    beginAiRequestAnimation();

    QPointer<RuntimePetWindow> guard(this);
    AIChatRequest request;
    request.characterName = characterName();
    request.systemPrompt = m_project.aiSystemPrompt;
    request.history = m_conversationContext->history;
    request.userMessage = prompt;
    request.allowedActionIds = m_project.aiTriggerActionIds();
    request.allowedActionDescriptors = m_project.aiTriggerActionDescriptors();
    request.proactive = true;
    m_proactiveRequestHandle = AIProvider::instance().sendMessage(resolved.profile, request, [guard](const AIRequestResult &result) {
        if (!guard) {
            return;
        }

        guard->finishProactiveRequest();
        if (guard->m_runtimeClosing) {
            return;
        }
        if (!result.success) {
            if (guard->m_chatWindow) {
                guard->m_chatWindow->displaySystemMessage(
                    QStringLiteral("AI 主动消息失败 [%1]：%2").arg(result.errorCode, result.message));
            }
            return;
        }
        const AIChatReply parsed = AIProvider::parseStructuredReply(result.message, guard->m_project.aiTriggerActionIds());
        const QString message = parsed.reply.trimmed().isEmpty() ? result.message.trimmed() : parsed.reply.trimmed();
        if (!message.isEmpty()) {
            guard->showProactivePetMessage(message);
            const RuntimeActionDispatchResult actionResult = guard->queueOrRunAiAction(parsed.actionId, parsed.actionParameters);
            guard->showActionDispatchFeedback(parsed.actionId, actionResult);
        }
    });
    m_proactiveAiLease->bindHandle(m_proactiveRequestHandle);
}

void RuntimePetWindow::cancelProactiveRequest(const QString &reason)
{
    if (m_proactiveRequestHandle && !m_proactiveRequestHandle->isFinished()) {
        m_proactiveRequestHandle->cancel(reason);
    }
    finishProactiveRequest();
}

void RuntimePetWindow::finishProactiveRequest()
{
    if (m_proactiveRequestActive && m_conversationContext) {
        m_proactiveRequestActive = false;
        m_conversationContext->endRequest();
    }
    m_proactiveRequestHandle.clear();
    m_proactiveAiLease.reset();
    if (m_chatWindow) {
        m_chatWindow->setExternalBusy(false);
    }
    endAiRequestAnimationIfIdle();
}

QPair<QString, QString> RuntimePetWindow::parseAiReplyAction(const QString &raw) const
{
    const AIChatReply parsed = AIProvider::parseStructuredReply(raw, m_project.aiTriggerActionIds());
    return {parsed.reply, parsed.actionId};
}

RuntimeActionDispatchResult RuntimePetWindow::queueOrRunAiAction(const QString &actionId,
                                                                 const QJsonObject &parameters,
                                                                 const QString &roomId,
                                                                 const QString &messageId)
{
    if (actionId.trimmed().isEmpty()) {
        return {RuntimeActionDispatchStatus::NotRequested, {}, {}};
    }
    const QString currentRuntimeState = currentAiActionRuntimeState();
    const bool mustQueue = currentRuntimeState != QStringLiteral("Normal");
    const QString validationState = mustQueue ? QStringLiteral("Normal") : currentRuntimeState;
    const AiActionValidationResult validation = validateAiActionRequest(m_project, actionId, parameters, validationState);
    if (!validation.ok) {
        return {RuntimeActionDispatchStatus::Rejected, validation.errorCode, validation.message};
    }
    if (mustQueue) {
        if (!m_queuedAiActionId.isEmpty()) {
            const RuntimeActionDispatchResult replaced {
                RuntimeActionDispatchStatus::Failed,
                QStringLiteral("QueuedActionSuperseded"),
                QStringLiteral("A newer AI action replaced the queued action.")
            };
            emit queuedAiActionFinished(m_queuedAiActionRoomId, m_queuedAiActionMessageId, replaced);
        }
        m_queuedAiActionId = actionId;
        m_queuedAiActionParameters = validation.parameters;
        m_queuedAiActionRoomId = roomId;
        m_queuedAiActionMessageId = messageId;
        return {RuntimeActionDispatchStatus::Queued,
                QStringLiteral("ActionQueued"),
                QStringLiteral("The action is queued until the pet returns to the Normal runtime state.")};
    }
    stopPatrol();
    cancelClickFollow();
    applyAiActionParameters(actionId, validation.parameters);
    const RuntimeActionDispatchResult switched = switchState(actionId);
    if (switched.status != RuntimeActionDispatchStatus::Executed || m_state != actionId) {
        clearAiActionParameters();
        return {RuntimeActionDispatchStatus::Failed,
                switched.errorCode.isEmpty() ? QStringLiteral("ActionStateEntryFailed") : switched.errorCode,
                switched.message};
    }
    return switched;
}

bool RuntimePetWindow::executeQueuedAiActionIfReady()
{
    if (m_queuedAiActionId.isEmpty() || currentAiActionRuntimeState() != QStringLiteral("Normal")) {
        return false;
    }

    const QString actionId = m_queuedAiActionId;
    const QJsonObject parameters = m_queuedAiActionParameters;
    const QString roomId = m_queuedAiActionRoomId;
    const QString messageId = m_queuedAiActionMessageId;
    m_queuedAiActionId.clear();
    m_queuedAiActionParameters = {};
    m_queuedAiActionRoomId.clear();
    m_queuedAiActionMessageId.clear();

    PetProject latestProject;
    QString loadError;
    if (!latestProject.load(m_project.petJsonPath(), &loadError)) {
        const RuntimeActionDispatchResult failed {
            RuntimeActionDispatchStatus::Failed,
            QStringLiteral("ProjectReloadFailed"),
            loadError.isEmpty() ? QStringLiteral("The pet project could not be reloaded before queued execution.") : loadError
        };
        emit queuedAiActionFinished(roomId, messageId, failed);
        showActionDispatchFeedback(actionId, failed);
        return false;
    }

    const AiActionValidationResult validation = validateAiActionRequest(
        latestProject, actionId, parameters, QStringLiteral("Normal"));
    if (!validation.ok) {
        const RuntimeActionDispatchResult rejected {
            RuntimeActionDispatchStatus::Rejected,
            validation.errorCode,
            validation.message
        };
        emit queuedAiActionFinished(roomId, messageId, rejected);
        showActionDispatchFeedback(actionId, rejected);
        return false;
    }

    m_project = latestProject;
    preloadFrameCache();
    stopPatrol();
    cancelClickFollow();
    applyAiActionParameters(actionId, validation.parameters);
    RuntimeActionDispatchResult executed = switchState(actionId);
    if (executed.status != RuntimeActionDispatchStatus::Executed || m_state != actionId) {
        clearAiActionParameters();
        executed = {RuntimeActionDispatchStatus::Failed,
                    executed.errorCode.isEmpty() ? QStringLiteral("ActionStateEntryFailed") : executed.errorCode,
                    executed.message};
    }
    emit queuedAiActionFinished(roomId, messageId, executed);
    showActionDispatchFeedback(actionId, executed);
    return true;
}

void RuntimePetWindow::applyAiActionParameters(const QString &actionId, const QJsonObject &parameters)
{
    m_activeAiActionId = actionId;
    m_activeAiActionParameters = parameters;
}

void RuntimePetWindow::clearAiActionParameters()
{
    m_activeAiActionId.clear();
    m_activeAiActionParameters = {};
}

void RuntimePetWindow::showActionDispatchFeedback(const QString &actionId,
                                                  const RuntimeActionDispatchResult &result)
{
    if (actionId.isEmpty() || result.status == RuntimeActionDispatchStatus::NotRequested
        || result.status == RuntimeActionDispatchStatus::Executed) {
        return;
    }
    QString statusText;
    switch (result.status) {
    case RuntimeActionDispatchStatus::Queued: statusText = QStringLiteral("已排队"); break;
    case RuntimeActionDispatchStatus::Rejected: statusText = QStringLiteral("已拒绝"); break;
    case RuntimeActionDispatchStatus::Failed: statusText = QStringLiteral("执行失败"); break;
    case RuntimeActionDispatchStatus::Executed: statusText = QStringLiteral("已执行"); break;
    case RuntimeActionDispatchStatus::NotRequested: statusText = QStringLiteral("未请求"); break;
    }
    const QString detail = QStringLiteral("动作 %1 %2 [%3]：%4")
                               .arg(actionId,
                                    statusText,
                                    result.errorCode.isEmpty() ? QStringLiteral("NoErrorCode") : result.errorCode,
                                    result.message);
    if (m_chatWindow) {
        m_chatWindow->displaySystemMessage(detail);
    } else if (m_speechBubble) {
        m_speechBubble->updateBubblePosition(frameGeometry());
        m_speechBubble->showMessage(detail);
    }
}

QString RuntimePetWindow::currentAiActionRuntimeState() const
{
    if (m_dragging || m_physicsState == PhysicsState::Dragging) return QStringLiteral("Dragging");
    if (m_physicsState == PhysicsState::Falling) return QStringLiteral("Falling");
    if (m_physicsState == PhysicsState::Bouncing) return QStringLiteral("Bouncing");
    if (m_physicsState == PhysicsState::Settled) return QStringLiteral("Settled");
    if (m_state == actionIdForRole(SystemActionRole::Sleeping, QStringLiteral("sleep"))) return QStringLiteral("Sleep");
    if (m_followMoveTimer && m_followMoveTimer->isActive()) return QStringLiteral("Follow");
    if (m_patrolActive && m_isWalking) return QStringLiteral("Patrol");
    return QStringLiteral("Normal");
}

#ifdef DESKTOP_PET_TESTING
RuntimeActionDispatchResult RuntimePetWindow::dispatchAiActionForTesting(const QString &actionId,
                                                                         const QJsonObject &parameters)
{
    return queueOrRunAiAction(actionId, parameters, QStringLiteral("test-room"), QStringLiteral("test-message"));
}

void RuntimePetWindow::setRuntimeStateForTesting(const QString &state)
{
    m_dragging = state == QStringLiteral("Dragging");
    if (state == QStringLiteral("Dragging")) m_physicsState = PhysicsState::Dragging;
    else if (state == QStringLiteral("Falling")) m_physicsState = PhysicsState::Falling;
    else if (state == QStringLiteral("Bouncing")) m_physicsState = PhysicsState::Bouncing;
    else if (state == QStringLiteral("Settled")) m_physicsState = PhysicsState::Settled;
    else m_physicsState = PhysicsState::Normal;
}

void RuntimePetWindow::restoreNormalStateForTesting()
{
    m_dragging = false;
    m_physicsState = PhysicsState::Normal;
    executeQueuedAiActionIfReady();
}
#endif

void RuntimePetWindow::showProactivePetMessage(const QString &message)
{
    const QString clean = message.trimmed();
    if (clean.isEmpty()) {
        return;
    }

    m_conversationContext->history.append({ChatHistoryRole::Assistant, clean});
    if (m_chatWindow) {
        m_chatWindow->displayExternalPetMessage(clean);
        return;
    }
    if (m_speechBubble) {
        m_speechBubble->updateBubblePosition(frameGeometry());
        m_speechBubble->showMessage(clean);
    }
}

RuntimeReplyDeliveryResult RuntimePetWindow::deliverRoomAiReply(const QString &reply,
                                                                const QString &actionId,
                                                                const QJsonObject &actionParameters,
                                                                const QString &roomName,
                                                                const QString &userMessage,
                                                                const QString &roomId,
                                                                const QString &messageId)
{
    RuntimeReplyDeliveryResult delivery;
    const QString clean = reply.trimmed();
    if (!clean.isEmpty() && m_conversationContext) {
        if (!userMessage.trimmed().isEmpty()) {
            const QString prefix = roomName.trimmed().isEmpty()
                ? QStringLiteral("[群聊]")
                : QStringLiteral("[%1]").arg(roomName.trimmed());
            m_conversationContext->history.append({ChatHistoryRole::User,
                                                   QStringLiteral("%1 %2").arg(prefix, userMessage.trimmed())});
        }
        m_conversationContext->history.append({ChatHistoryRole::Assistant, clean});
    }
    if (!clean.isEmpty() && m_speechBubble) {
        m_speechBubble->updateBubblePosition(frameGeometry());
        m_speechBubble->showMessage(clean);
        delivery.textDelivered = true;
    }
    delivery.action = queueOrRunAiAction(actionId, actionParameters, roomId, messageId);
    return delivery;
}

void RuntimePetWindow::beginAiRequestAnimation()
{
    if (m_aiGenerating) {
        return;
    }
    m_aiGenerating = true;
    m_aiAnimationState = selectAiAnimationState();
    restorePassiveStateIfSafe();
}

void RuntimePetWindow::endAiRequestAnimationIfIdle()
{
    if (m_conversationContext && m_conversationContext->busy()) {
        return;
    }
    m_aiGenerating = false;
    m_aiAnimationState.clear();
    restorePassiveStateIfSafe();
}

void RuntimePetWindow::setupRandomIdleActions()
{
    m_randomIdleActionNames = {
        QStringLiteral("look_around"),
        QStringLiteral("wave"),
        QStringLiteral("nod"),
        QStringLiteral("shake_head"),
        QStringLiteral("stretch")
    };

    m_randomIdleTimer = new QTimer(this);
    m_randomIdleTimer->setSingleShot(true);
    m_randomActionClock.start();
    connect(m_randomIdleTimer, &QTimer::timeout, this, &RuntimePetWindow::onRandomIdleActionTimer);
    scheduleNextRandomIdleAction();
}

void RuntimePetWindow::scheduleNextRandomIdleAction()
{
    if (!m_randomIdleTimer) {
        return;
    }

    const int delay = kRandomIdleMinDelayMs + QRandomGenerator::global()->bounded(kRandomIdleDelayRangeMs + 1);
    m_randomIdleTimer->start(delay);
}

void RuntimePetWindow::onRandomIdleActionTimer()
{
    if (canRunRandomIdleAction()) {
        triggerRandomIdleAction();
    }

    scheduleNextRandomIdleAction();
}

bool RuntimePetWindow::canRunRandomIdleAction() const
{
    if (m_dragging || m_overlayActive || m_chatWindow || m_journalWindow) {
        return false;
    }
    if (m_followMoveTimer->isActive()) {
        return false;
    }
    if (currentStateHasRole(SystemActionRole::Sleeping) || currentStateHasRole(SystemActionRole::Dragging)
        || currentStateHasRole(SystemActionRole::Landing)) {
        return false;
    }
    if (m_patrolActive && m_patrolState == PatrolState::PatrolWalk) {
        return false;
    }

    return true;
}

void RuntimePetWindow::triggerRandomIdleAction()
{
    const int roll = QRandomGenerator::global()->bounded(100);
    if (roll >= kRandomIdleTotalChance) {
        return;
    }

    if (roll < kRandomIdleSleepChance && hasStateFrames(actionIdForRole(SystemActionRole::Sleeping, QStringLiteral("sleep")))
        && !sleepCooldownActive()) {
        enterSleepState();
        return;
    }

    const QStringList candidates = availableRandomIdleActions();
    if (candidates.isEmpty()) {
        return;
    }

    double totalWeight = 0.0;
    for (const QString &candidate : candidates) {
        totalWeight += qMax(0.0, randomIdleWeight(candidate));
    }
    double pick = totalWeight > 0.0 ? QRandomGenerator::global()->generateDouble() * totalWeight : 0.0;
    QString chosen = candidates.constFirst();
    for (const QString &candidate : candidates) {
        pick -= qMax(0.0, randomIdleWeight(candidate));
        if (pick <= 0.0) {
            chosen = candidate;
            break;
        }
    }

    stopPatrol();
    cancelClickFollow();
    m_randomActionLastTriggeredMs.insert(chosen, m_randomActionClock.elapsed());
    switchState(chosen);
}

void RuntimePetWindow::enterSleepState(bool force)
{
    if (!force && m_overlayActive) {
        return;
    }
    if (!hasStateFrames(actionIdForRole(SystemActionRole::Sleeping, QStringLiteral("sleep"))) || m_dragging || m_chatWindow || m_journalWindow
        || m_physicsState != PhysicsState::Normal || (!force && sleepCooldownActive())) {
        return;
    }

    stopPatrol();
    cancelClickFollow();
    if (m_randomIdleTimer) {
        m_randomIdleTimer->stop();
    }
    switchState(actionIdForRole(SystemActionRole::Sleeping, QStringLiteral("sleep")));
    if (m_sleepTimer) {
        m_sleepTimer->start(kSleepDurationMs);
    }
}

void RuntimePetWindow::wakeFromSleep()
{
    if (currentStateHasRole(SystemActionRole::Sleeping)) {
        finishSleep();
        return;
    }
    if (m_randomIdleTimer && !m_randomIdleTimer->isActive()) {
        scheduleNextRandomIdleAction();
    }
}

void RuntimePetWindow::finishSleep()
{
    if (m_sleepTimer) {
        m_sleepTimer->stop();
    }
    if (currentStateHasRole(SystemActionRole::Sleeping)) {
        switchState(actionIdForRole(SystemActionRole::Idle, QStringLiteral("idle")));
    }
    m_lastInteraction.restart();
    m_sleepCooldownActive = true;
    m_sleepCooldown.restart();
    if (m_randomIdleTimer && !m_randomIdleTimer->isActive()) {
        scheduleNextRandomIdleAction();
    }
    if (m_patrolEnabled && !m_patrolActive && m_physicsState == PhysicsState::Normal && !m_dragging
        && !m_chatWindow && !m_journalWindow) {
        startPatrol();
    }
}

bool RuntimePetWindow::sleepCooldownActive() const
{
    return m_sleepCooldownActive && m_sleepCooldown.isValid()
        && m_sleepCooldown.elapsed() < kSleepCooldownMs;
}

bool RuntimePetWindow::isRandomIdleAction(const QString &stateName) const
{
    return stateName == QStringLiteral("look_around")
        || stateName == QStringLiteral("wave")
        || stateName == QStringLiteral("nod")
        || stateName == QStringLiteral("shake_head")
        || stateName == QStringLiteral("stretch");
}

QStringList RuntimePetWindow::availableRandomIdleActions() const
{
    QStringList result;
    bool hasExplicitRules = false;
    for (const PetBehaviorRule &rule : m_project.behaviorRules) {
        if (rule.triggerType != BehaviorTriggerType::RandomIdle) {
            continue;
        }
        hasExplicitRules = true;
        const qint64 last = m_randomActionLastTriggeredMs.value(rule.actionId, -1);
        const qint64 elapsed = m_randomActionClock.isValid() ? m_randomActionClock.elapsed() : 0;
        const bool idle = currentStateHasRole(SystemActionRole::Idle)
            || (m_patrolActive && m_patrolState == PatrolState::PatrolIdle);
        if (!randomIdleRuleMatchesConditions(rule,
                                             hasStateFrames(rule.actionId),
                                             m_physicsState == PhysicsState::Normal,
                                             idle,
                                             m_aiGenerating,
                                             elapsed,
                                             last)) {
            continue;
        }
        result.append(rule.actionId);
    }
    result.removeDuplicates();
    if (hasExplicitRules) {
        return result;
    }

    bool hasConfiguredRandom = false;
    for (auto it = m_project.actions.constBegin(); it != m_project.actions.constEnd(); ++it) {
        const PetAction &action = it.value();
        if (!action.allowRandomTrigger || !hasStateFrames(it.key()) || action.randomWeight <= 0.0) {
            continue;
        }
        hasConfiguredRandom = true;
        const qint64 last = m_randomActionLastTriggeredMs.value(it.key(), -1);
        if (last >= 0 && randomIdleCooldownMs(it.key()) > 0
            && m_randomActionClock.isValid()
            && m_randomActionClock.elapsed() - last < randomIdleCooldownMs(it.key())) {
            continue;
        }
        result.append(it.key());
    }
    if (hasConfiguredRandom) {
        return result;
    }

    for (const QString &stateName : m_randomIdleActionNames) {
        if (hasStateFrames(stateName)) {
            result.append(stateName);
        }
    }
    return result;
}

double RuntimePetWindow::randomIdleWeight(const QString &actionId) const
{
    double ruleWeight = 0.0;
    bool hasRule = false;
    for (const PetBehaviorRule &rule : m_project.behaviorRules) {
        if (rule.triggerType == BehaviorTriggerType::RandomIdle && rule.actionId == actionId) {
            hasRule = true;
            ruleWeight += qMax(0.0, rule.weight);
        }
    }
    if (hasRule) {
        return ruleWeight;
    }
    return m_project.actions.value(actionId).randomWeight;
}

int RuntimePetWindow::randomIdleCooldownMs(const QString &actionId) const
{
    int cooldown = 0;
    bool hasRule = false;
    for (const PetBehaviorRule &rule : m_project.behaviorRules) {
        if (rule.triggerType == BehaviorTriggerType::RandomIdle && rule.actionId == actionId) {
            hasRule = true;
            cooldown = qMax(cooldown, rule.cooldownMs);
        }
    }
    if (hasRule) {
        return cooldown;
    }
    return m_project.actions.value(actionId).randomCooldownMs;
}

void RuntimePetWindow::rememberInteraction()
{
    m_lastInteraction.restart();
}

void RuntimePetWindow::restorePassiveStateIfSafe()
{
    if (m_physicsState != PhysicsState::Normal || m_dragging
        || (m_followMoveTimer && m_followMoveTimer->isActive())
        || currentStateHasRole(SystemActionRole::Sleeping)
        || currentStateHasRole(SystemActionRole::Dragging)
        || currentStateHasRole(SystemActionRole::Landing)) {
        return;
    }

    if (!m_journalWindow) {
        if (!m_queuedAiActionId.isEmpty()) {
            if (executeQueuedAiActionIfReady()) {
                return;
            }
        }
        if (m_aiGenerating && !m_aiAnimationState.isEmpty() && hasStateFrames(m_aiAnimationState)) {
            switchState(m_aiAnimationState);
        } else {
            switchState(actionIdForRole(SystemActionRole::Idle, QStringLiteral("idle")));
        }
    }
}

QString RuntimePetWindow::selectAiAnimationState() const
{
    return aiTalkingActionId();
}

void RuntimePetWindow::saveRuntimePlacement()
{
    m_project.runtimeAnchorScreen = frameGeometry().topLeft() + scaledAnchor();
    m_project.hasRuntimeAnchorScreen = true;
    RuntimeStatePatch patch;
    patch.hasAnchorScreen = true;
    patch.runtimeAnchorScreen = m_project.runtimeAnchorScreen;
    saveRuntimeStatePatch(patch);
}

void RuntimePetWindow::saveRuntimeStatePatch(const RuntimeStatePatch &patch)
{
    QString error;
    if (!m_project.saveRuntimeStatePatch(toProjectRuntimePatch(patch), &error)) {
        qWarning() << error;
        return;
    }
    emit runtimeStateChanged(patch);
}

PetProject::RuntimeStatePatch RuntimePetWindow::toProjectRuntimePatch(const RuntimeStatePatch &patch) const
{
    PetProject::RuntimeStatePatch projectPatch;
    projectPatch.hasAnchorScreen = patch.hasAnchorScreen;
    projectPatch.anchorScreen = patch.runtimeAnchorScreen;
    projectPatch.clearAnchorScreen = patch.clearAnchorScreen;
    projectPatch.hasLocked = patch.hasLocked;
    projectPatch.locked = patch.locked;
    projectPatch.hasMousePassthrough = patch.hasMousePassthrough;
    projectPatch.mousePassthrough = patch.mousePassthrough;
    projectPatch.hasTopMost = patch.hasTopMost;
    projectPatch.topMost = patch.topMost;
    projectPatch.hasPatrolEnabled = patch.hasPatrolEnabled;
    projectPatch.patrolEnabled = patch.patrolEnabled;
    return projectPatch;
}

QRect RuntimePetWindow::usableScreenRect() const
{
    const QPoint center = frameGeometry().center();
    QScreen *screen = QGuiApplication::screenAt(center);
    if (!screen) {
        screen = QApplication::primaryScreen();
    }
    if (screen) {
        return screen->geometry();
    }

    return QRect(0, 0, 1920, 1080);
}

int RuntimePetWindow::physicsGroundY(const QRect &screen) const
{
    const QPoint center = frameGeometry().center();
    QScreen *currentScreen = QGuiApplication::screenAt(center);
    if (!currentScreen) {
        currentScreen = QApplication::primaryScreen();
    }

    int groundY = screen.bottom() + 1;
    if (currentScreen) {
        const QRect available = currentScreen->availableGeometry();
        const bool bottomReserved = available.top() <= screen.top()
            && available.left() <= screen.left()
            && available.right() >= screen.right()
            && available.bottom() < screen.bottom();
        if (bottomReserved) {
            groundY = available.bottom() + 1;
        }
    }

    const QRect taskbar = getTaskbarRect();
    if (taskbar.isValid() && taskbar.intersects(screen)
        && taskbar.width() >= taskbar.height()
        && taskbar.top() > screen.center().y()
        && taskbar.top() <= screen.bottom() + 1) {
        groundY = qMin(groundY, taskbar.top());
    }

    return qBound(screen.top() + 1, groundY, screen.bottom() + 1);
}

int RuntimePetWindow::groundWindowY(const QRect &screen) const
{
    return qMax(screen.top(), physicsGroundY(screen) - scaledAnchor().y());
}

int RuntimePetWindow::currentAnchorY() const
{
    return frameGeometry().topLeft().y() + scaledAnchor().y();
}

int RuntimePetWindow::minAnchorBoundX(const QRect &screen) const
{
    return screen.left() - scaledAnchor().x();
}

int RuntimePetWindow::maxAnchorBoundX(const QRect &screen) const
{
    return qMax(minAnchorBoundX(screen), screen.right() + 1 - scaledAnchor().x());
}

void RuntimePetWindow::startPatrol()
{
    if (!canStartPatrol()) {
        return;
    }

    calculatePatrolZone();
    m_patrolActive = true;
    switchToPatrolIdle();
    m_patrolTimer->start();
    scheduleNextWalk();
}

bool RuntimePetWindow::canStartPatrol() const
{
    return m_patrolEnabled
        && !m_project.locked
        && !m_patrolActive
        && m_physicsState == PhysicsState::Normal
        && !m_dragging
        && !m_overlayActive
        && !m_chatWindow
        && !m_journalWindow
        && !currentStateHasRole(SystemActionRole::Sleeping)
        && !m_aiGenerating
        && (!m_followMoveTimer || !m_followMoveTimer->isActive());
}

void RuntimePetWindow::stopPatrol()
{
    m_patrolActive = false;
    m_patrolTimer->stop();
    m_patrolStateTimer->stop();
    m_patrolState = PatrolState::Inactive;
    if (m_isWalking && m_physicsState == PhysicsState::Normal) {
        switchState(actionIdForRole(SystemActionRole::Idle, QStringLiteral("idle")));
    }
    m_isWalking = false;
}

void RuntimePetWindow::armClickFollow()
{
    if (m_physicsState != PhysicsState::Normal || m_dragging) {
        return;
    }

    stopPatrol();
    m_followArmed = true;
#ifdef _WIN32
    m_followClickWasDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
#else
    m_followClickWasDown = (QApplication::mouseButtons() & Qt::LeftButton) != 0;
#endif
    m_followClickTimer->start();
}

void RuntimePetWindow::cancelClickFollow()
{
    m_followArmed = false;
    if (m_followClickTimer) {
        m_followClickTimer->stop();
    }
}

void RuntimePetWindow::pollFollowClick()
{
    if (!m_followArmed || m_dragging || m_physicsState != PhysicsState::Normal) {
        cancelClickFollow();
        return;
    }

#ifdef _WIN32
    const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const QPoint globalPos = QCursor::pos();
#else
    const bool down = (QApplication::mouseButtons() & Qt::LeftButton) != 0;
    const QPoint globalPos = QCursor::pos();
#endif

    if (down && !m_followClickWasDown) {
        if (!frameGeometry().contains(globalPos)) {
            startFollowTo(globalPos);
            return;
        }
    }

    m_followClickWasDown = down;
}

void RuntimePetWindow::startFollowTo(const QPoint &globalPos)
{
    cancelClickFollow();
    stopPatrol();
    m_physicsTimer->stop();
    m_physicsState = PhysicsState::Normal;

    QScreen *targetScreen = QGuiApplication::screenAt(globalPos);
    if (!targetScreen) {
        targetScreen = QApplication::primaryScreen();
    }
    const QRect screen = targetScreen ? targetScreen->geometry() : usableScreenRect();
    const QPoint anchor = scaledAnchor();
    const int minX = minAnchorBoundX(screen);
    const int maxX = maxAnchorBoundX(screen);
    const int targetX = qBound(minX, globalPos.x() - anchor.x(), maxX);
    const int targetY = groundWindowY(screen);

    m_followTarget = QPoint(targetX, targetY);
    m_followMoveTimer->start();
    m_isWalking = true;
    const QString walkState = walkStateForDelta(m_followTarget.x() - frameGeometry().topLeft().x());
    if (!walkState.isEmpty()) {
        switchState(walkState);
    } else {
        switchState(actionIdForRole(SystemActionRole::Idle, QStringLiteral("idle")));
    }
}

void RuntimePetWindow::onFollowMoveTimer()
{
    if (m_dragging || m_physicsState != PhysicsState::Normal) {
        m_followMoveTimer->stop();
        m_isWalking = false;
        return;
    }

    QPoint current = frameGeometry().topLeft();
    const int deltaX = m_followTarget.x() - current.x();
    if (qAbs(deltaX) <= m_followSpeed) {
        move(m_followTarget);
        m_followMoveTimer->stop();
        m_isWalking = false;
        saveRuntimePlacement();
        restorePassiveStateIfSafe();
        QTimer::singleShot(1500, this, [this]() {
            if (m_physicsState == PhysicsState::Normal && !m_dragging && !m_followMoveTimer->isActive()) {
                startPatrol();
            }
        });
        return;
    }

    current.setX(current.x() + qBound(-m_followSpeed, deltaX, m_followSpeed));
    current.setY(m_followTarget.y());
    move(current);
}

void RuntimePetWindow::onPatrolTimer()
{
    if (m_patrolState != PatrolState::PatrolWalk || m_physicsState != PhysicsState::Normal
        || m_dragging || !m_patrolActive) {
        return;
    }

    QPoint current = frameGeometry().topLeft();
    const QPoint delta = m_patrolTarget - current;

    if (qAbs(delta.x()) <= m_patrolSpeed && qAbs(delta.y()) <= m_patrolSpeed) {
        switchToPatrolIdle();
        scheduleNextWalk();
        return;
    }

    current.setX(current.x() + qBound(-m_patrolSpeed, delta.x(), m_patrolSpeed));
    current.setY(m_patrolTarget.y());
    const QRect screen = usableScreenRect();
    current.setX(qBound(minAnchorBoundX(screen), current.x(), maxAnchorBoundX(screen)));
    move(current);
}

void RuntimePetWindow::switchToPatrolIdle()
{
    m_patrolState = PatrolState::PatrolIdle;
    m_isWalking = false;
    switchState(actionIdForRole(SystemActionRole::Idle, QStringLiteral("idle")));
}

void RuntimePetWindow::switchToPatrolWalk()
{
    if (!m_patrolActive || m_physicsState != PhysicsState::Normal || m_dragging) {
        return;
    }

    m_patrolState = PatrolState::PatrolWalk;
    m_isWalking = true;
    selectNewPatrolTarget();
    const QString walkState = walkStateForDelta(m_patrolTarget.x() - frameGeometry().topLeft().x());
    if (!walkState.isEmpty()) {
        switchState(walkState);
    } else {
        switchState(actionIdForRole(SystemActionRole::Idle, QStringLiteral("idle")));
    }
}

void RuntimePetWindow::scheduleNextWalk()
{
    if (!m_patrolActive || m_physicsState != PhysicsState::Normal || m_dragging) {
        return;
    }

    const int delay = 2000 + QRandomGenerator::global()->bounded(3000);
    QTimer::singleShot(delay, this, [this]() {
        if (m_patrolState == PatrolState::PatrolIdle && !m_dragging && m_patrolActive
            && m_physicsState == PhysicsState::Normal) {
            switchToPatrolWalk();
        }
    });
}

void RuntimePetWindow::selectNewPatrolTarget()
{
    const QRect screen = usableScreenRect();
    const int left = minAnchorBoundX(screen);
    const int right = maxAnchorBoundX(screen);
    const int range = qMax(0, right - left);
    int x = left + (range > 0 ? QRandomGenerator::global()->bounded(range + 1) : 0);

    const int y = groundWindowY(screen);
    x = qBound(left, x, right);
    m_patrolTarget = QPoint(x, y);
}

void RuntimePetWindow::calculatePatrolZone()
{
    const QRect screen = usableScreenRect();
    const int y = groundWindowY(screen);
    m_patrolZone = QRect(screen.left(), y, screen.width(), qMax(1, height()));
}

QRect RuntimePetWindow::getTaskbarRect() const
{
#ifdef _WIN32
    HWND shellWindow = FindWindow(L"Shell_TrayWnd", nullptr);
    if (shellWindow) {
        RECT rect;
        GetWindowRect(shellWindow, &rect);
        return QRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    }
#endif
    const QRect screen = QApplication::primaryScreen() ? QApplication::primaryScreen()->geometry()
                                                       : QRect(0, 0, 1920, 1080);
    return QRect(screen.left(), screen.bottom() - 50, screen.width(), 50);
}

bool RuntimePetWindow::hasWalkState() const
{
    return hasStateFrames(actionIdForRole(SystemActionRole::WalkLeft, QStringLiteral("walk_left")))
        || hasStateFrames(actionIdForRole(SystemActionRole::WalkRight, QStringLiteral("walk_right")))
        || hasStateFrames(QStringLiteral("walk"));
}

bool RuntimePetWindow::hasStateFrames(const QString &stateName) const
{
    auto it = m_project.actions.constFind(stateName);
    return it != m_project.actions.constEnd() && !it.value().frames.isEmpty();
}

QString RuntimePetWindow::walkStateForDelta(int deltaX) const
{
    const QString roleLeft = actionIdForRole(SystemActionRole::WalkLeft, QStringLiteral("walk_left"));
    const QString roleRight = actionIdForRole(SystemActionRole::WalkRight, QStringLiteral("walk_right"));
    const QString leftState = m_project.invertWalkDirection ? roleRight : roleLeft;
    const QString rightState = m_project.invertWalkDirection ? roleLeft : roleRight;

    if (deltaX < 0 && hasStateFrames(leftState)) {
        return leftState;
    }
    if (deltaX > 0 && hasStateFrames(rightState)) {
        return rightState;
    }
    if (hasStateFrames(QStringLiteral("walk"))) {
        return QStringLiteral("walk");
    }
    if (hasStateFrames(roleRight)) {
        return roleRight;
    }
    if (hasStateFrames(roleLeft)) {
        return roleLeft;
    }
    return QString();
}

void RuntimePetWindow::startFalling()
{
    m_physicsState = PhysicsState::Falling;
    stopPatrol();
    switchState(actionIdForRole(SystemActionRole::Falling, QStringLiteral("drop")));
    const QRect screen = usableScreenRect();
    const int groundY = physicsGroundY(screen);
    QPoint pos = frameGeometry().topLeft();
    pos.setX(qBound(minAnchorBoundX(screen), pos.x(), maxAnchorBoundX(screen)));
    pos.setY(qMax(screen.top(), pos.y()));
    if (pos.y() + scaledAnchor().y() > groundY) {
        pos.setY(groundWindowY(screen));
    }
    move(pos);

    m_bounceCount = 0;
    m_physicsTimer->start();
}

void RuntimePetWindow::onPhysicsTimer()
{
    switch (m_physicsState) {
    case PhysicsState::Falling:
        updateFalling();
        break;
    case PhysicsState::Bouncing:
        updateBouncing();
        break;
    case PhysicsState::Settled:
        m_physicsTimer->stop();
        onSettled();
        break;
    default:
        m_physicsTimer->stop();
        break;
    }
}

void RuntimePetWindow::updateFalling()
{
    m_velocityY += m_gravity;
    m_velocityX *= m_friction;

    QPoint pos = frameGeometry().topLeft();
    pos.setX(pos.x() + qRound(m_velocityX));
    pos.setY(pos.y() + qRound(m_velocityY));

    const QRect screen = usableScreenRect();
    const int groundY = physicsGroundY(screen);
    const int maxY = groundWindowY(screen);
    pos.setX(qBound(minAnchorBoundX(screen), pos.x(), maxAnchorBoundX(screen)));
    pos.setY(qMax(screen.top(), pos.y()));

    if (pos.y() + scaledAnchor().y() >= groundY) {
        pos.setY(maxY);
        move(pos);

        if (qAbs(m_velocityY) > m_minBounceVelocity && m_bounceCount < 3) {
            m_velocityY = -m_velocityY * m_bounceFactor;
            m_bounceCount++;
            m_physicsState = PhysicsState::Bouncing;
        } else {
            m_velocityY = 0.0;
            m_velocityX = 0.0;
            m_physicsState = PhysicsState::Settled;
        }
        return;
    }

    move(pos);
}

void RuntimePetWindow::updateBouncing()
{
    m_velocityY += m_gravity * 0.5;
    m_velocityX *= m_friction;

    QPoint pos = frameGeometry().topLeft();
    pos.setX(pos.x() + qRound(m_velocityX));
    pos.setY(pos.y() + qRound(m_velocityY));

    const QRect screen = usableScreenRect();
    const int groundY = physicsGroundY(screen);
    const int maxY = groundWindowY(screen);
    pos.setX(qBound(minAnchorBoundX(screen), pos.x(), maxAnchorBoundX(screen)));
    pos.setY(qMax(screen.top(), pos.y()));

    if (pos.y() + scaledAnchor().y() >= groundY) {
        pos.setY(maxY);
        move(pos);

        if (qAbs(m_velocityY) > m_minBounceVelocity && m_bounceCount < 3) {
            m_velocityY = -m_velocityY * m_bounceFactor;
            m_bounceCount++;
        } else {
            m_velocityY = 0.0;
            m_velocityX = 0.0;
            m_physicsState = PhysicsState::Settled;
        }
        return;
    }

    move(pos);
}

void RuntimePetWindow::onSettled()
{
    m_physicsState = PhysicsState::Normal;
    m_bounceCount = 0;
    saveRuntimePlacement();

    if (!m_dragging) {
        const QString landing = actionIdForRole(SystemActionRole::Landing, QString());
        if (hasStateFrames(landing)) {
            switchState(landing);
        } else {
            restorePassiveStateIfSafe();
        }
    }

    QTimer::singleShot(2000, this, [this]() {
        if (m_physicsState == PhysicsState::Normal && !m_dragging
            && !currentStateHasRole(SystemActionRole::Landing)
            && canStartPatrol()) {
            startPatrol();
        }
    });
}

void RuntimePetWindow::calculateReleaseVelocity()
{
    const QPoint releasePos = frameGeometry().topLeft();
    const QPoint dragStart = m_pressGlobal - m_dragOffset;
    const QPoint dragDelta = releasePos - dragStart;

    if (dragDelta.manhattanLength() > 100) {
        m_velocityY = qBound(-12.0, dragDelta.y() * 0.12, 12.0);
        if (m_velocityY >= 0.0 && m_velocityY < 4.0) {
            m_velocityY = 4.0;
        } else if (m_velocityY < 0.0 && qAbs(m_velocityY) < 2.0) {
            m_velocityY = -6.0;
        }
        m_velocityX = qBound(-10.0, dragDelta.x() * 0.1, 10.0);
    } else {
        m_velocityY = 4.0;
        m_velocityX = 0.0;
    }
}

QSize RuntimePetWindow::scaledCanvasSize() const
{
    const QSize canvas = m_project.canvasSize.isValid() ? m_project.canvasSize : QSize(180, 180);
    return QSize(qMax(1, qRound(canvas.width() * m_project.scale)),
                 qMax(1, qRound(canvas.height() * m_project.scale)));
}

QPoint RuntimePetWindow::scaledAnchor() const
{
    return QPoint(qRound(m_project.anchor.x() * m_project.scale),
                  qRound(m_project.anchor.y() * m_project.scale));
}

const PetAction *RuntimePetWindow::currentAction() const
{
    auto it = m_project.actions.constFind(m_state);
    if (it == m_project.actions.constEnd()) {
        return nullptr;
    }
    return &it.value();
}

QString RuntimePetWindow::actionIdForRole(SystemActionRole role, const QString &legacyFallback) const
{
    return m_project.actionForRole(role, legacyFallback);
}

QString RuntimePetWindow::idleActionId() const
{
    const QString roleIdle = actionIdForRole(SystemActionRole::Idle, QStringLiteral("idle"));
    if (hasStateFrames(roleIdle)) {
        return roleIdle;
    }
    for (auto it = m_project.actions.constBegin(); it != m_project.actions.constEnd(); ++it) {
        if (!it.value().frames.isEmpty()) {
            qWarning() << "Idle role is missing; using first readable action as emergency fallback:" << it.key();
            return it.key();
        }
    }
    return QString();
}

QString RuntimePetWindow::aiThinkingActionId() const
{
    const QString thinking = actionIdForRole(SystemActionRole::AIThinking, QStringLiteral("nod"));
    if (hasStateFrames(thinking)) {
        return thinking;
    }
    return idleActionId();
}

QString RuntimePetWindow::aiTalkingActionId() const
{
    const QString talking = actionIdForRole(SystemActionRole::AITalking, QStringLiteral("talk"));
    if (hasStateFrames(talking)) {
        return talking;
    }
    return aiThinkingActionId();
}

bool RuntimePetWindow::currentStateHasRole(SystemActionRole role) const
{
    const PetAction action = m_project.actions.value(m_state);
    return action.systemRole == PetProject::systemRoleName(role);
}

QString RuntimePetWindow::fallbackState(const QString &stateName) const
{
    const auto hasFrames = [this](const QString &name) {
        const auto it = m_project.actions.constFind(name);
        return it != m_project.actions.constEnd() && !it.value().frames.isEmpty();
    };

    if (hasFrames(stateName)) {
        return stateName;
    }
    const QString idle = idleActionId();
    if (hasFrames(idle)) {
        return idle;
    }
    return stateName;
}

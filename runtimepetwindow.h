#ifndef RUNTIMEPETWINDOW_H
#define RUNTIMEPETWINDOW_H

#include <QDate>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QPoint>
#include <QPixmap>
#include <QRect>
#include <QPointer>
#include <QSharedPointer>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <memory>

#include "aidialogwindow.h"
#include "journalwindow.h"
#include "petconversationcontext.h"
#include "petproject.h"
#include "renderbackend.h"
#include "runtimeactionresult.h"

class QMouseEvent;
class QCloseEvent;
class QContextMenuEvent;
class QEvent;
class QMessageBox;
class PetSpeechBubbleWindow;
class AIRequestHandle;
class PetAiRequestLease;

class RuntimePetWindow : public QMainWindow
{
    Q_OBJECT

public:
    struct RuntimeStatePatch {
        bool hasAnchorScreen {false};
        QPoint runtimeAnchorScreen;
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

    explicit RuntimePetWindow(const PetProject &project,
                              const QSharedPointer<PetConversationContext> &conversationContext = {},
                              QWidget *parent = nullptr,
                              std::unique_ptr<IRenderBackend> renderBackend = {});
    ~RuntimePetWindow() override;
    static bool randomIdleRuleMatchesForTest(const PetBehaviorRule &rule,
                                             bool actionHasFrames,
                                             bool onGround,
                                             bool idle,
                                             bool aiBusy,
                                             qint64 elapsedMs,
                                             qint64 lastTriggeredMs);
    void placeOnGroundAtX(int x);
    bool ensureVisibleOnAvailableScreens(bool persistCorrection = true);
    void setPetSwitchOptions(const QVector<QPair<QString, QString>> &options);
    QString petJsonPath() const { return m_project.petJsonPath(); }
    bool runtimeLocked() const { return m_project.locked; }
    bool runtimeMousePassthrough() const { return m_project.mousePassthrough; }
    bool runtimeTopMost() const { return m_project.topMost; }
    bool runtimePatrolEnabled() const { return m_patrolEnabled; }
    PetSpeechBubbleWindow *speechBubble() const;
    QSharedPointer<PetConversationContext> conversationContext() const { return m_conversationContext; }

public slots:
    void setRuntimeLocked(bool locked);
    void setRuntimeMousePassthrough(bool mousePassthrough);
    void setRuntimeTopMost(bool topMost);
    void setRuntimePatrolEnabled(bool enabled);
    void reloadAiRoleFromProject();
    void openChatWindow();
    RuntimeReplyDeliveryResult deliverRoomAiReply(const QString &reply,
                                                  const QString &actionId = QString(),
                                                  const QJsonObject &actionParameters = {},
                                                  const QString &roomName = QString(),
                                                  const QString &userMessage = QString(),
                                                  const QString &roomId = QString(),
                                                  const QString &messageId = QString());
#ifdef DESKTOP_PET_TESTING
    RuntimeActionDispatchResult dispatchAiActionForTesting(const QString &actionId, const QJsonObject &parameters = {});
    RuntimeActionDispatchResult switchActionForTesting(const QString &actionId) { return switchState(actionId); }
    void setRuntimeStateForTesting(const QString &state);
    void restoreNormalStateForTesting();
    QString currentActionForTesting() const { return m_state; }
    QString queuedActionForTesting() const { return m_queuedAiActionId; }
    int frameIntervalForTesting() const { return m_frameTimer ? m_frameTimer->interval() : -1; }
    QPixmap renderedPixmapForTesting() const { return m_label ? m_label->pixmap() : QPixmap(); }
    void showJournalReminderForTesting() { showJournalReminder(); }
    JournalWindow *journalWindowForTesting() const { return m_journalWindow; }
    bool overlayActiveForTesting() const { return m_overlayActive; }
    QString renderBackendIdForTesting() const { return m_renderBackend ? m_renderBackend->backendId() : QString(); }
    QString renderBackendFallbackForTesting() const { return m_renderBackendFallbackReason; }
    static QPoint clampTopLeftForTesting(const QPoint &desiredTopLeft,
                                         const QSize &windowSize,
                                         const QVector<QRect> &availableScreens,
                                         int primaryScreenIndex = 0);
#endif

signals:
    void runtimeStateChanged(const RuntimePetWindow::RuntimeStatePatch &patch);
    void switchPetRequested(const QString &petJsonPath);
    void controlCenterRequested();
    void exitApplicationRequested();
    void petGeometryChanged(const QRect &geometry);
    void queuedAiActionFinished(const QString &roomId,
                                const QString &messageId,
                                const RuntimeActionDispatchResult &result);

protected:
    void closeEvent(QCloseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class PatrolState {
        Inactive,
        PatrolIdle,
        PatrolWalk,
        PatrolPause
    };

    enum class PhysicsState {
        Normal,
        Dragging,
        Falling,
        Bouncing,
        Settled
    };

    void applyWindowOptions();
    void applyMousePassthrough();
    RuntimeActionDispatchResult switchState(const QString &stateName);
    bool actionHasRenderableFrame(const QString &stateName) const;
    void advanceFrame();
    int currentFrameDurationMs() const;
    void renderFrame();
    void preloadFrameCache();
    int cachedFrameCount(const QString &stateName) const;
    const QPixmap *cachedFrame(const QString &stateName, int frameIndex) const;
    void checkIdleSleep();
    void pauseForOverlay();
    void resumePatrolLater(int delayMs = 1000);
    void openJournalWindow();
    void setupPetChat();
    void scheduleNextPetChat();
    void onPetChatTimer();
    void setupJournalReminder();
    void showJournalReminder();
    QString characterName() const;
    void requestProactivePetMessage(const QString &prompt);
    void cancelProactiveRequest(const QString &reason);
    void finishProactiveRequest();
    void showProactivePetMessage(const QString &message);
    QPair<QString, QString> parseAiReplyAction(const QString &raw) const;
    RuntimeActionDispatchResult queueOrRunAiAction(const QString &actionId,
                                                   const QJsonObject &parameters = {},
                                                   const QString &roomId = QString(),
                                                   const QString &messageId = QString());
    bool executeQueuedAiActionIfReady();
    void applyAiActionParameters(const QString &actionId, const QJsonObject &parameters);
    void clearAiActionParameters();
    void showActionDispatchFeedback(const QString &actionId, const RuntimeActionDispatchResult &result);
    QString currentAiActionRuntimeState() const;
    void beginAiRequestAnimation();
    void endAiRequestAnimationIfIdle();
    void rememberInteraction();
    void restorePassiveStateIfSafe();
    QString selectAiAnimationState() const;
    void saveRuntimePlacement();
    void saveRuntimeStatePatch(const RuntimeStatePatch &patch);
    PetProject::RuntimeStatePatch toProjectRuntimePatch(const RuntimeStatePatch &patch) const;
    QRect usableScreenRect() const;
    int physicsGroundY(const QRect &screen) const;
    int groundWindowY(const QRect &screen) const;
    int currentAnchorY() const;
    int minAnchorBoundX(const QRect &screen) const;
    int maxAnchorBoundX(const QRect &screen) const;
    void setupRandomIdleActions();
    void scheduleNextRandomIdleAction();
    void onRandomIdleActionTimer();
    bool canRunRandomIdleAction() const;
    void triggerRandomIdleAction();
    void enterSleepState(bool force = false);
    void wakeFromSleep();
    void finishSleep();
    bool sleepCooldownActive() const;
    bool isRandomIdleAction(const QString &stateName) const;
    QStringList availableRandomIdleActions() const;
    double randomIdleWeight(const QString &actionId) const;
    int randomIdleCooldownMs(const QString &actionId) const;
    void startPatrol();
    bool canStartPatrol() const;
    void stopPatrol();
    void onPatrolTimer();
    void switchToPatrolIdle();
    void switchToPatrolWalk();
    void scheduleNextWalk();
    void selectNewPatrolTarget();
    void calculatePatrolZone();
    QRect getTaskbarRect() const;
    bool hasWalkState() const;
    bool hasStateFrames(const QString &stateName) const;
    QString walkStateForDelta(int deltaX) const;
    void armClickFollow();
    void cancelClickFollow();
    void pollFollowClick();
    void startFollowTo(const QPoint &globalPos);
    void onFollowMoveTimer();
    void startFalling();
    void onPhysicsTimer();
    void updateFalling();
    void updateBouncing();
    void onSettled();
    void calculateReleaseVelocity();
    QSize scaledCanvasSize() const;
    QPoint scaledAnchor() const;
    const PetAction *currentAction() const;
    QString fallbackState(const QString &stateName) const;
    QString actionIdForRole(SystemActionRole role, const QString &legacyFallback) const;
    QString idleActionId() const;
    QString aiThinkingActionId() const;
    QString aiTalkingActionId() const;
    bool currentStateHasRole(SystemActionRole role) const;

    PetProject m_project;
    QLabel *m_label {nullptr};
    QTimer *m_frameTimer {nullptr};
    QTimer *m_idleTimer {nullptr};
    QTimer *m_patrolTimer {nullptr};
    QTimer *m_patrolStateTimer {nullptr};
    QTimer *m_physicsTimer {nullptr};
    QTimer *m_followClickTimer {nullptr};
    QTimer *m_followMoveTimer {nullptr};
    QTimer *m_petChatTimer {nullptr};
    QTimer *m_randomIdleTimer {nullptr};
    QTimer *m_sleepTimer {nullptr};
    QElapsedTimer m_lastInteraction;
    QElapsedTimer m_sleepCooldown;
    bool m_sleepCooldownActive {false};
    bool m_overlayActive {false};
    QString m_state {"idle"};
    int m_frameIndex {0};
    bool m_dragging {false};
    bool m_dragStarted {false};
    QPoint m_pressGlobal;
    QPoint m_dragOffset;
    PatrolState m_patrolState {PatrolState::Inactive};
    bool m_patrolEnabled {true};
    bool m_patrolActive {false};
    QPoint m_patrolTarget;
    int m_patrolSpeed {1};
    bool m_isWalking {false};
    QRect m_patrolZone;
    bool m_followArmed {false};
    bool m_followClickWasDown {false};
    QPoint m_followTarget;
    int m_followSpeed {3};
    PhysicsState m_physicsState {PhysicsState::Normal};
    double m_velocityX {0.0};
    double m_velocityY {0.0};
    double m_gravity {0.6};
    double m_bounceFactor {0.5};
    double m_friction {0.98};
    int m_minBounceVelocity {3};
    int m_bounceCount {0};
    QPointer<AIDialogWindow> m_chatWindow;
    QPointer<JournalWindow> m_journalWindow;
    QPointer<PetSpeechBubbleWindow> m_speechBubble;
    QStringList m_petPrompts;
    QStringList m_randomIdleActionNames;
    QElapsedTimer m_randomActionClock;
    QHash<QString, qint64> m_randomActionLastTriggeredMs;
    QHash<QString, QVector<QPixmap>> m_frameCache;
    QHash<QString, QVector<int>> m_frameDurationCache;
    QSharedPointer<PetConversationContext> m_conversationContext;
    QSharedPointer<PetAiRequestLease> m_proactiveAiLease;
    QPointer<AIRequestHandle> m_proactiveRequestHandle;
    bool m_proactiveRequestActive {false};
    bool m_runtimeClosing {false};
    QVector<QPair<QString, QString>> m_petSwitchOptions;
    bool m_aiGenerating {false};
    QString m_aiAnimationState;
    QString m_queuedAiActionId;
    QJsonObject m_queuedAiActionParameters;
    QString m_queuedAiActionRoomId;
    QString m_queuedAiActionMessageId;
    QString m_activeAiActionId;
    QJsonObject m_activeAiActionParameters;
    std::unique_ptr<IRenderBackend> m_renderBackend;
    QString m_renderBackendFallbackReason;
};

#endif // RUNTIMEPETWINDOW_H

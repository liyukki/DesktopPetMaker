#include "aiconversationconsolewindow.h"

#include "petprojectregistry.h"
#include "runtimepetmanager.h"
#include "ui/theme/apptheme.h"
#include "ui/theme/iconprovider.h"
#include "ui/theme/themeconstants.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>

namespace {
QString messageStatusText(ConversationMessageStatus status)
{
    switch (status) {
    case ConversationMessageStatus::Pending: return QStringLiteral("\u7b49\u5f85\u4e2d");
    case ConversationMessageStatus::Completed: return QStringLiteral("\u5df2\u5b8c\u6210");
    case ConversationMessageStatus::Failed: return QStringLiteral("\u5931\u8d25");
    case ConversationMessageStatus::Cancelled: return QStringLiteral("\u5df2\u53d6\u6d88");
    case ConversationMessageStatus::TimedOut: return QStringLiteral("\u5df2\u8d85\u65f6");
    case ConversationMessageStatus::InvalidAction: return QStringLiteral("\u52a8\u4f5c\u65e0\u6548");
    case ConversationMessageStatus::RuntimeDispatchFailed: return QStringLiteral("\u65e7\u7248\u6295\u9012\u5931\u8d25");
    case ConversationMessageStatus::Interrupted: return QStringLiteral("\u5df2\u4e2d\u65ad");
    }
    return QStringLiteral("\u672a\u77e5");
}

QString desktopStatusText(DesktopDeliveryStatus status)
{
    switch (status) {
    case DesktopDeliveryStatus::NotRequested: return QStringLiteral("\u672a\u8bf7\u6c42");
    case DesktopDeliveryStatus::Pending: return QStringLiteral("\u5f85\u6295\u9012");
    case DesktopDeliveryStatus::Delivered: return QStringLiteral("\u5df2\u6295\u9012");
    case DesktopDeliveryStatus::PetNotRunning: return QStringLiteral("\u684c\u5ba0\u672a\u8fd0\u884c");
    case DesktopDeliveryStatus::Failed: return QStringLiteral("\u6295\u9012\u5931\u8d25");
    }
    return QStringLiteral("\u672a\u77e5");
}

QString actionStatusText(ActionExecutionStatus status)
{
    switch (status) {
    case ActionExecutionStatus::NotRequested: return QStringLiteral("\u672a\u8bf7\u6c42");
    case ActionExecutionStatus::Pending: return QStringLiteral("\u5f85\u6267\u884c");
    case ActionExecutionStatus::Executed: return QStringLiteral("\u5df2\u6267\u884c");
    case ActionExecutionStatus::Queued: return QStringLiteral("\u5df2\u6392\u961f");
    case ActionExecutionStatus::Rejected: return QStringLiteral("\u5df2\u62d2\u7edd");
    case ActionExecutionStatus::Failed: return QStringLiteral("\u6267\u884c\u5931\u8d25");
    }
    return QStringLiteral("\u672a\u77e5");
}
}

AIConversationConsoleWindow::AIConversationConsoleWindow(RuntimePetManager *runtimeManager, QWidget *parent)
    : QMainWindow(parent)
    , m_runtimeManager(runtimeManager)
{
    setWindowTitle(QStringLiteral("AI \u591a\u89d2\u8272\u5bf9\u8bdd\u53f0"));
    setupUi();
    if (m_roomManager.hasLoadFailure()) {
        if (m_roomManager.isFutureSchemaProtected()) {
            QMessageBox::information(this,
                                     QStringLiteral("\u623f\u95f4\u6570\u636e\u6765\u81ea\u65b0\u7248\u7a0b\u5e8f"),
                                     m_roomManager.loadFailureMessage()
                                         + QStringLiteral("\n\n\u5f53\u524d\u7248\u672c\u5df2\u542f\u7528\u53ea\u8bfb\u4fdd\u62a4\uff0c\u4e0d\u4f1a\u5907\u4efd\u3001\u8986\u76d6\u6216\u91cd\u7f6e\u8be5\u6587\u4ef6\u3002"));
            refreshRoomList();
            resize(1080, 640);
            return;
        }
        const QString backup = m_roomManager.corruptBackupPath().isEmpty()
            ? QStringLiteral("\u672a\u80fd\u521b\u5efa\u5907\u4efd")
            : m_roomManager.corruptBackupPath();
        const auto choice = QMessageBox::warning(
            this,
            QStringLiteral("\u623f\u95f4\u6570\u636e\u65e0\u6cd5\u8bfb\u53d6"),
            QStringLiteral("\u672a\u5bf9\u539f\u6570\u636e\u8fdb\u884c\u8986\u76d6\u3002\n\n%1\n\n\u5907\u4efd\uff1a%2\n\n\u662f\u5426\u786e\u8ba4\u521b\u5efa\u4e00\u4e2a\u7a7a\u623f\u95f4\u4ed3\u5e93\uff1f")
                .arg(m_roomManager.loadFailureMessage(), backup),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (choice == QMessageBox::Yes && m_roomManager.recoverFromLoadFailure()) {
            m_roomManager.createRoom(QStringLiteral("\u9ed8\u8ba4\u623f\u95f4"));
        }
    } else if (m_roomManager.rooms().isEmpty()) {
        m_roomManager.createRoom(QStringLiteral("\u9ed8\u8ba4\u623f\u95f4"));
    }
    refreshRoomList();
    resize(1080, 640);
}

void AIConversationConsoleWindow::setupUi()
{
    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("appRoot"));
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(20, 18, 20, 18);
    rootLayout->setSpacing(14);
    auto *header = new QWidget(root);
    header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    auto *headerText = new QWidget(header);
    auto *headerTextLayout = new QVBoxLayout(headerText);
    headerTextLayout->setContentsMargins(0, 0, 0, 0);
    headerTextLayout->setSpacing(2);
    auto *title = new QLabel(QStringLiteral("AI 多角色对话台"), headerText);
    auto *subtitle = new QLabel(QStringLiteral("编排房间、角色关系和桌面动作投递"), headerText);
    AppTheme::setRole(title, QStringLiteral("pageTitle"));
    AppTheme::setRole(subtitle, QStringLiteral("muted"));
    headerTextLayout->addWidget(title);
    headerTextLayout->addWidget(subtitle);
    auto *modeBadge = new QLabel(QStringLiteral("Multi-AI"), header);
    AppTheme::setRole(modeBadge, QStringLiteral("status"));
    modeBadge->setProperty("status", QStringLiteral("info"));
    headerLayout->addWidget(headerText);
    headerLayout->addStretch(1);
    headerLayout->addWidget(modeBadge);
    rootLayout->addWidget(header);
    auto *splitter = new QSplitter(root);

    auto *left = new QWidget(splitter);
    AppTheme::setRole(left, QStringLiteral("card"));
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    auto *roomsTitle = new QLabel(QStringLiteral("会话房间"), left);
    AppTheme::setRole(roomsTitle, QStringLiteral("sectionTitle"));
    m_roomList = new QListWidget(left);
    m_roomList->setObjectName(QStringLiteral("roomList"));
    auto *newRoom = new QPushButton(QStringLiteral("\u65b0\u5efa\u623f\u95f4"), left);
    auto *renameRoom = new QPushButton(QStringLiteral("\u91cd\u547d\u540d\u623f\u95f4"), left);
    auto *deleteRoom = new QPushButton(QStringLiteral("\u5220\u9664\u623f\u95f4"), left);
    newRoom->setObjectName(QStringLiteral("newRoomButton"));
    renameRoom->setObjectName(QStringLiteral("renameRoomButton"));
    deleteRoom->setObjectName(QStringLiteral("deleteRoomButton"));
    IconProvider::apply(newRoom, AppIcon::Add);
    IconProvider::apply(renameRoom, AppIcon::Edit);
    IconProvider::apply(deleteRoom, AppIcon::Delete);
    AppTheme::setButtonRole(newRoom, QStringLiteral("primary"));
    AppTheme::setButtonRole(deleteRoom, QStringLiteral("danger"));
    leftLayout->addWidget(roomsTitle);
    leftLayout->addWidget(m_roomList, 1);
    leftLayout->addWidget(newRoom);
    leftLayout->addWidget(renameRoom);
    leftLayout->addWidget(deleteRoom);

    auto *center = new QWidget(splitter);
    AppTheme::setRole(center, QStringLiteral("card"));
    auto *centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(14, 12, 14, 12);
    auto *conversationTitle = new QLabel(QStringLiteral("对话内容"), center);
    AppTheme::setRole(conversationTitle, QStringLiteral("sectionTitle"));
    m_modeCombo = new QComboBox(center);
    m_modeCombo->setObjectName(QStringLiteral("roomModeCombo"));
    m_modeCombo->addItem(QStringLiteral("\u5b9a\u5411\u5bf9\u8bdd"), QVariant::fromValue(static_cast<int>(AIConversationMode::Directed)));
    m_modeCombo->addItem(QStringLiteral("\u81ea\u7531\u7fa4\u804a"), QVariant::fromValue(static_cast<int>(AIConversationMode::FreeGroup)));
    m_modeCombo->addItem(QStringLiteral("\u8f6e\u6d41\u53d1\u8a00"), QVariant::fromValue(static_cast<int>(AIConversationMode::RoundTable)));
    m_directedTargetCombo = new QComboBox(center);
    m_directedTargetCombo->setObjectName(QStringLiteral("directedTargetCombo"));
    m_directedTargetCombo->setPlaceholderText(QStringLiteral("\u5b9a\u5411\u76ee\u6807"));
    m_minRespondersSpin = new QSpinBox(center);
    m_minRespondersSpin->setObjectName(QStringLiteral("minRespondersSpin"));
    m_minRespondersSpin->setRange(1, 32);
    m_maxRespondersSpin = new QSpinBox(center);
    m_maxRespondersSpin->setObjectName(QStringLiteral("maxRespondersSpin"));
    m_maxRespondersSpin->setRange(1, 32);
    m_persistHistoryCheck = new QCheckBox(QStringLiteral("\u4fdd\u5b58\u5386\u53f2\u8bb0\u5f55"), center);
    m_historyLimitSpin = new QSpinBox(center);
    m_historyLimitSpin->setRange(1, 2000);
    m_clearHistoryButton = new QPushButton(QStringLiteral("\u6e05\u7a7a\u5386\u53f2"), center);
    m_transcript = new QTextEdit(center);
    m_transcript->setReadOnly(true);
    m_input = new QTextEdit(center);
    m_input->setObjectName(QStringLiteral("roomMessageInput"));
    m_input->setPlaceholderText(QStringLiteral("\u8f93\u5165\u7fa4\u804a\u6d88\u606f"));
    m_input->setMaximumHeight(90);
    m_sendButton = new QPushButton(QStringLiteral("\u53d1\u9001"), center);
    m_cancelButton = new QPushButton(QStringLiteral("\u53d6\u6d88"), center);
    m_sendButton->setObjectName(QStringLiteral("roomSendButton"));
    m_cancelButton->setObjectName(QStringLiteral("roomCancelButton"));
    m_cancelButton->setEnabled(false);
    IconProvider::apply(m_sendButton, AppIcon::Chat);
    IconProvider::apply(m_cancelButton, AppIcon::Stop);
    AppTheme::setButtonRole(m_sendButton, QStringLiteral("primary"));
    AppTheme::setButtonRole(m_cancelButton, QStringLiteral("danger"));
    centerLayout->addWidget(conversationTitle);
    centerLayout->addWidget(m_modeCombo);
    auto *configuration = new QHBoxLayout();
    configuration->addWidget(m_directedTargetCombo, 1);
    configuration->addWidget(m_minRespondersSpin);
    configuration->addWidget(m_maxRespondersSpin);
    configuration->addWidget(m_persistHistoryCheck);
    configuration->addWidget(m_historyLimitSpin);
    configuration->addWidget(m_clearHistoryButton);
    centerLayout->addLayout(configuration);
    centerLayout->addWidget(m_transcript, 1);
    centerLayout->addWidget(m_input);
    auto *sendButtons = new QHBoxLayout();
    sendButtons->addWidget(m_sendButton);
    sendButtons->addWidget(m_cancelButton);
    centerLayout->addLayout(sendButtons);

    auto *right = new QWidget(splitter);
    AppTheme::setRole(right, QStringLiteral("card"));
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(12, 12, 12, 12);
    auto *participantsTitle = new QLabel(QStringLiteral("参与角色"), right);
    AppTheme::setRole(participantsTitle, QStringLiteral("sectionTitle"));
    m_participantList = new QListWidget(right);
    m_participantList->setObjectName(QStringLiteral("participantList"));
    m_participantList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    auto *addPet = new QPushButton(QStringLiteral("\u6dfb\u52a0\u684c\u5ba0\u89d2\u8272"), right);
    auto *removePet = new QPushButton(QStringLiteral("\u79fb\u9664\u89d2\u8272"), right);
    addPet->setObjectName(QStringLiteral("addParticipantButton"));
    removePet->setObjectName(QStringLiteral("removeParticipantButton"));
    IconProvider::apply(addPet, AppIcon::Add);
    IconProvider::apply(removePet, AppIcon::Delete);
    AppTheme::setButtonRole(removePet, QStringLiteral("danger"));
    m_editWeightButton = new QPushButton(QStringLiteral("\u7f16\u8f91\u53d1\u8a00\u6743\u91cd"), right);
    m_editRelationshipButton = new QPushButton(QStringLiteral("\u7f16\u8f91\u5b9a\u5411\u5173\u7cfb"), right);
    rightLayout->addWidget(participantsTitle);
    rightLayout->addWidget(m_participantList, 1);
    rightLayout->addWidget(addPet);
    rightLayout->addWidget(removePet);
    rightLayout->addWidget(m_editWeightButton);
    rightLayout->addWidget(m_editRelationshipButton);

    splitter->addWidget(left);
    splitter->addWidget(center);
    splitter->addWidget(right);
    splitter->setStretchFactor(1, 1);
    rootLayout->addWidget(splitter, 1);
    setCentralWidget(root);

    connect(newRoom, &QPushButton::clicked, this, &AIConversationConsoleWindow::createRoom);
    connect(renameRoom, &QPushButton::clicked, this, &AIConversationConsoleWindow::renameCurrentRoom);
    connect(deleteRoom, &QPushButton::clicked, this, &AIConversationConsoleWindow::deleteCurrentRoom);
    connect(addPet, &QPushButton::clicked, this, &AIConversationConsoleWindow::addParticipant);
    connect(removePet, &QPushButton::clicked, this, &AIConversationConsoleWindow::removeParticipant);
    connect(m_editWeightButton, &QPushButton::clicked, this, &AIConversationConsoleWindow::editParticipantWeight);
    connect(m_editRelationshipButton, &QPushButton::clicked, this, &AIConversationConsoleWindow::editParticipantRelationship);
    connect(m_participantList, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        if (m_loadingRoom || !item) return;
        const QString roomId = currentRoomId();
        if (!roomId.isEmpty()) {
            if (!m_roomManager.setParticipantEnabled(roomId,
                                                     item->data(Qt::UserRole).toString(),
                                                     item->checkState() == Qt::Checked)) {
                showMutationErrorAndReload();
            }
        }
    });
    connect(m_roomList, &QListWidget::currentRowChanged, this, &AIConversationConsoleWindow::loadCurrentRoom);
    connect(m_sendButton, &QPushButton::clicked, this, &AIConversationConsoleWindow::sendMessage);
    connect(m_cancelButton, &QPushButton::clicked, this, &AIConversationConsoleWindow::cancelCurrentTurn);
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this]() {
        if (m_loadingRoom) return;
        saveCurrentRoomConfiguration();
    });
    connect(m_directedTargetCombo, &QComboBox::currentIndexChanged, this, [this]() { saveCurrentRoomConfiguration(); });
    connect(m_minRespondersSpin, &QSpinBox::valueChanged, this, [this]() { saveCurrentRoomConfiguration(); });
    connect(m_maxRespondersSpin, &QSpinBox::valueChanged, this, [this]() { saveCurrentRoomConfiguration(); });
    connect(m_persistHistoryCheck, &QCheckBox::toggled, this, [this]() { saveCurrentRoomConfiguration(); });
    connect(m_historyLimitSpin, &QSpinBox::valueChanged, this, [this]() { saveCurrentRoomConfiguration(); });
    connect(m_clearHistoryButton, &QPushButton::clicked, this, [this]() {
        const QString roomId = currentRoomId();
        if (!roomId.isEmpty() && !m_roomManager.clearHistory(roomId)) showMutationErrorAndReload();
    });
    connect(&m_roomManager, &AIConversationRoomManager::roomUpdated, this, [this](const QString &roomId) {
        if (roomId == currentRoomId()) {
            loadCurrentRoom();
        }
    });
    connect(&m_roomManager, &AIConversationRoomManager::turnStarted, this, [this](const QString &roomId) {
        if (roomId == currentRoomId()) {
            m_sendButton->setEnabled(false);
            m_sendButton->setText(QStringLiteral("\u601d\u8003\u4e2d"));
            m_cancelButton->setEnabled(true);
        }
    });
    connect(&m_roomManager, &AIConversationRoomManager::turnFinished, this, [this](const QString &roomId) {
        if (roomId == currentRoomId()) {
            m_sendButton->setEnabled(true);
            m_sendButton->setText(QStringLiteral("\u53d1\u9001"));
            m_cancelButton->setEnabled(false);
        }
    });
    connect(&m_roomManager, &AIConversationRoomManager::roomError, this, [this](const QString &roomId, const QString &message) {
        if (roomId == currentRoomId()) {
            statusBar()->showMessage(message, 5000);
        }
    });
    connect(&m_roomManager,
            &AIConversationRoomManager::roomPetReply,
            this,
            [this](const QString &roomId, const QString &, const QString &petProjectPath, const QString &reply,
                   const QString &actionId, const QJsonObject &actionParameters, const QString &messageId, const QString &roomName,
                   const QString &userMessage) {
                RuntimeReplyDeliveryResult delivery;
                if (m_runtimeManager) {
                    delivery = m_runtimeManager->deliverRoomAiReply(petProjectPath, reply, actionId, actionParameters,
                                                                    roomName, userMessage, roomId, messageId);
                } else {
                    delivery.errorCode = QStringLiteral("PetNotRunning");
                    delivery.message = QStringLiteral("The RuntimePetManager is unavailable.");
                    delivery.action = {actionId.isEmpty() ? RuntimeActionDispatchStatus::NotRequested
                                                          : RuntimeActionDispatchStatus::Failed,
                                       actionId.isEmpty() ? QString() : delivery.errorCode,
                                       actionId.isEmpty() ? QString() : delivery.message};
                }
                m_roomManager.recordRuntimeDeliveryResult(roomId, messageId, delivery);
            });
    if (m_runtimeManager) {
        connect(m_runtimeManager, &RuntimePetManager::queuedAiActionFinished, this,
                [this](const QString &, const QString &roomId, const QString &messageId,
                       const RuntimeActionDispatchResult &result) {
                    if (!roomId.isEmpty() && !messageId.isEmpty()) {
                        m_roomManager.recordRuntimeActionResult(roomId, messageId, result);
                    }
                });
    }
}

void AIConversationConsoleWindow::refreshRoomList()
{
    const QString currentId = currentRoomId();
    m_roomList->clear();
    int restoreRow = 0;
    const QVector<AIConversationRoom> rooms = m_roomManager.rooms();
    for (int i = 0; i < rooms.size(); ++i) {
        const AIConversationRoom &room = rooms.at(i);
        auto *item = new QListWidgetItem(room.roomName, m_roomList);
        item->setData(Qt::UserRole, room.roomId);
        if (room.roomId == currentId) {
            restoreRow = i;
        }
    }
    if (m_roomList->count() > 0) {
        m_roomList->setCurrentRow(qBound(0, restoreRow, m_roomList->count() - 1));
    }
}

void AIConversationConsoleWindow::createRoom()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               QStringLiteral("新建房间"),
                                               QStringLiteral("房间名称"),
                                               QLineEdit::Normal,
                                               QStringLiteral("新房间"),
                                               &ok);
    if (ok) {
        if (m_roomManager.createRoom(name).isEmpty()) showMutationErrorAndReload();
    }
    refreshRoomList();
}

void AIConversationConsoleWindow::renameCurrentRoom()
{
    AIConversationRoom *room = currentRoom();
    if (!room) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                                QStringLiteral("重命名房间"),
                                                QStringLiteral("房间名称"),
                                                QLineEdit::Normal,
                                                room->roomName,
                                                &ok);
    if (ok && m_roomManager.renameRoom(room->roomId, name)) {
        refreshRoomList();
    }
}

void AIConversationConsoleWindow::deleteCurrentRoom()
{
    const QString roomId = currentRoomId();
    if (roomId.isEmpty()) {
        return;
    }
    if (!m_roomManager.deleteRoom(roomId)) {
        QMessageBox::warning(this, QStringLiteral("\u5220\u9664\u5931\u8d25"), m_roomManager.lastMutationResult().message);
    }
    refreshRoomList();
    loadCurrentRoom();
}

void AIConversationConsoleWindow::addParticipant()
{
    AIConversationRoom *room = currentRoom();
    if (!room) return;

    PetProjectRegistry registry;
    registry.scan();
    const QVector<PetProjectEntry> entries = registry.entries();
    QStringList labels;
    for (const PetProjectEntry &entry : entries) {
        labels.append(QStringLiteral("%1 | %2").arg(entry.displayName, entry.petJsonPath));
    }
    if (labels.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("\u6ca1\u6709\u684c\u5ba0"), QStringLiteral("\u5f53\u524d\u6ca1\u6709\u53ef\u52a0\u5165\u623f\u95f4\u7684\u684c\u5ba0\u9879\u76ee\u3002"));
        return;
    }

    bool ok = false;
    const QString selected = QInputDialog::getItem(this, QStringLiteral("\u6dfb\u52a0\u684c\u5ba0\u89d2\u8272"), QStringLiteral("\u684c\u5ba0"), labels, 0, false, &ok);
    if (!ok || selected.isEmpty()) return;
    const int index = labels.indexOf(selected);
    if (index < 0) return;
    QString error;
    if (!m_roomManager.addParticipantFromProject(room->roomId, entries.at(index).petJsonPath, &error)) {
        QMessageBox::warning(this, QStringLiteral("\u6dfb\u52a0\u5931\u8d25"), error);
    }
}

void AIConversationConsoleWindow::removeParticipant()
{
    AIConversationRoom *room = currentRoom();
    QListWidgetItem *item = m_participantList->currentItem();
    if (!room || !item) return;
    if (!m_roomManager.removeParticipant(room->roomId, item->data(Qt::UserRole).toString())) {
        showMutationErrorAndReload();
    }
}

void AIConversationConsoleWindow::sendMessage()
{
    AIConversationRoom *room = currentRoom();
    if (!room) return;
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) return;
    if (!m_roomManager.submitUserMessage(room->roomId, text, selectedParticipantIds())) {
        const QString reason = m_roomManager.lastMutationResult().message.trimmed();
        QMessageBox::information(this,
                                 QStringLiteral("\u6682\u65f6\u4e0d\u80fd\u53d1\u9001"),
                                 reason.isEmpty()
                                     ? QStringLiteral("\u5f53\u524d\u623f\u95f4\u6682\u65f6\u4e0d\u80fd\u53d1\u9001\u6d88\u606f\u3002")
                                     : reason);
        return;
    }
    m_input->clear();
}

void AIConversationConsoleWindow::cancelCurrentTurn()
{
    const QString roomId = currentRoomId();
    if (!roomId.isEmpty()) {
        m_roomManager.cancelTurn(roomId);
    }
}

void AIConversationConsoleWindow::loadCurrentRoom()
{
    AIConversationRoom *room = currentRoom();
    m_transcript->clear();
    m_participantList->clear();
    if (!room) {
        return;
    }
    m_loadingRoom = true;
    m_modeCombo->setCurrentIndex(m_modeCombo->findData(QVariant::fromValue(static_cast<int>(room->mode))));
    m_directedTargetCombo->clear();
    m_directedTargetCombo->addItem(QStringLiteral("\u672a\u9009\u62e9\u76ee\u6807"), QString());
    for (const AIConversationParticipant &participant : room->participants) {
        if (participant.enabled) m_directedTargetCombo->addItem(participant.characterName, participant.participantId);
    }
    const int targetIndex = m_directedTargetCombo->findData(room->directedTargetParticipantId);
    m_directedTargetCombo->setCurrentIndex(targetIndex >= 0 ? targetIndex : 0);
    m_directedTargetCombo->setVisible(room->mode == AIConversationMode::Directed);
    m_minRespondersSpin->setValue(room->minRespondersPerTurn);
    m_maxRespondersSpin->setValue(room->maxRespondersPerTurn);
    m_persistHistoryCheck->setChecked(room->persistHistory);
    m_historyLimitSpin->setValue(room->historyMaxMessages);
    for (const ConversationMessage &message : room->history) {
        appendMessage(message);
    }
    refreshParticipants(*room);
    const bool busy = m_roomManager.isRoomBusy(room->roomId);
    m_sendButton->setEnabled(!busy);
    m_sendButton->setText(busy ? QStringLiteral("\u601d\u8003\u4e2d") : QStringLiteral("\u53d1\u9001"));
    if (m_cancelButton) {
        m_cancelButton->setEnabled(busy);
    }
    m_loadingRoom = false;
}

void AIConversationConsoleWindow::refreshParticipants(const AIConversationRoom &room)
{
    for (const AIConversationParticipant &participant : room.participants) {
        auto *item = new QListWidgetItem(QStringLiteral("%1  [\u6743\u91cd %2 | \u4e0a\u6b21\u53d1\u8a00 %3]")
                                           .arg(participant.characterName)
                                           .arg(participant.speakingWeight, 0, 'f', 2)
                                           .arg(participant.lastSpokeTurn),
                                       m_participantList);
        item->setData(Qt::UserRole, participant.participantId);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(participant.enabled ? Qt::Checked : Qt::Unchecked);
    }
}

void AIConversationConsoleWindow::saveCurrentRoomConfiguration()
{
    if (m_loadingRoom) return;
    AIConversationRoom *room = currentRoom();
    if (!room) return;
    if (!m_roomManager.setRoomSettings(room->roomId,
                                       static_cast<AIConversationMode>(m_modeCombo->currentData().toInt()),
                                       m_minRespondersSpin->value(),
                                       m_maxRespondersSpin->value(),
                                       m_directedTargetCombo->currentData().toString(),
                                       m_persistHistoryCheck->isChecked(),
                                       m_historyLimitSpin->value())) {
        showMutationErrorAndReload();
    }
}

void AIConversationConsoleWindow::showMutationErrorAndReload()
{
    const QString message = m_roomManager.lastMutationResult().message;
    if (!message.isEmpty()) {
        statusBar()->showMessage(message, 5000);
    }
    loadCurrentRoom();
}

void AIConversationConsoleWindow::editParticipantWeight()
{
    AIConversationRoom *room = currentRoom();
    QListWidgetItem *item = m_participantList->currentItem();
    if (!room || !item) return;
    const QString participantId = item->data(Qt::UserRole).toString();
    const auto found = std::find_if(room->participants.cbegin(), room->participants.cend(), [&participantId](const AIConversationParticipant &participant) {
        return participant.participantId == participantId;
    });
    if (found == room->participants.cend()) return;
    bool accepted = false;
    const double weight = QInputDialog::getDouble(this, QStringLiteral("\u53d1\u8a00\u6743\u91cd"), QStringLiteral("\u6743\u91cd"),
                                                   found->speakingWeight, 0.0, 100.0, 2, &accepted);
    if (accepted && !m_roomManager.setParticipantWeight(room->roomId, participantId, weight)) {
        showMutationErrorAndReload();
    }
}

void AIConversationConsoleWindow::editParticipantRelationship()
{
    AIConversationRoom *room = currentRoom();
    if (!room || room->participants.size() < 2) return;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("编辑定向关系"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *fromBox = new QComboBox(&dialog);
    auto *toBox = new QComboBox(&dialog);
    auto *addressEdit = new QLineEdit(&dialog);
    auto *descriptionEdit = new QTextEdit(&dialog);
    auto *bidirectional = new QCheckBox(QStringLiteral("同时保存反向关系"), &dialog);
    auto *existingList = new QListWidget(&dialog);
    for (const AIConversationParticipant &participant : room->participants) {
        const QString label = participant.characterName.isEmpty() ? participant.participantId : participant.characterName;
        fromBox->addItem(label, participant.participantId);
        toBox->addItem(label, participant.participantId);
    }
    if (toBox->count() > 1) toBox->setCurrentIndex(1);
    for (const ParticipantRelationship &relationship : room->relationships) {
        const QString fromName = fromBox->itemText(fromBox->findData(relationship.fromParticipantId));
        const QString toName = toBox->itemText(toBox->findData(relationship.toParticipantId));
        auto *item = new QListWidgetItem(QStringLiteral("%1 -> %2：%3")
                                             .arg(fromName, toName, relationship.description), existingList);
        item->setData(Qt::UserRole, relationship.fromParticipantId);
        item->setData(Qt::UserRole + 1, relationship.toParticipantId);
    }
    form->addRow(QStringLiteral("发起方"), fromBox);
    form->addRow(QStringLiteral("接收方"), toBox);
    form->addRow(QStringLiteral("偏好称呼"), addressEdit);
    form->addRow(QStringLiteral("关系说明"), descriptionEdit);
    form->addRow(QString(), bidirectional);
    layout->addLayout(form);
    layout->addWidget(new QLabel(QStringLiteral("当前关系"), &dialog));
    layout->addWidget(existingList);
    auto *deleteButton = new QPushButton(QStringLiteral("删除选中关系"), &dialog);
    layout->addWidget(deleteButton);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    const auto loadRelationship = [&]() {
        addressEdit->clear();
        descriptionEdit->clear();
        const QString fromId = fromBox->currentData().toString();
        const QString toId = toBox->currentData().toString();
        for (const ParticipantRelationship &relationship : room->relationships) {
            if (relationship.fromParticipantId == fromId && relationship.toParticipantId == toId) {
                addressEdit->setText(relationship.preferredAddress);
                descriptionEdit->setPlainText(relationship.description);
                break;
            }
        }
    };
    connect(fromBox, &QComboBox::currentIndexChanged, &dialog, loadRelationship);
    connect(toBox, &QComboBox::currentIndexChanged, &dialog, loadRelationship);
    connect(existingList, &QListWidget::itemClicked, &dialog, [=](QListWidgetItem *item) {
        fromBox->setCurrentIndex(fromBox->findData(item->data(Qt::UserRole)));
        toBox->setCurrentIndex(toBox->findData(item->data(Qt::UserRole + 1)));
        loadRelationship();
    });
    bool deletedRelationship = false;
    connect(deleteButton, &QPushButton::clicked, &dialog, [&]() {
        QListWidgetItem *item = existingList->currentItem();
        if (!item) return;
        if (!m_roomManager.removeParticipantRelationship(room->roomId,
                                                          item->data(Qt::UserRole).toString(),
                                                          item->data(Qt::UserRole + 1).toString())) {
            showMutationErrorAndReload();
        } else {
            deletedRelationship = true;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    loadRelationship();
    if (dialog.exec() != QDialog::Accepted || deletedRelationship) return;

    const QString fromId = fromBox->currentData().toString();
    const QString toId = toBox->currentData().toString();
    if (fromId == toId) {
        QMessageBox::warning(this, QStringLiteral("编辑定向关系"), QStringLiteral("发起方和接收方不能相同。"));
        return;
    }
    ParticipantRelationship relationship {fromId, toId, addressEdit->text().trimmed(), descriptionEdit->toPlainText().trimmed()};
    if (!m_roomManager.setParticipantRelationship(room->roomId, relationship)) {
        showMutationErrorAndReload();
        return;
    }
    if (bidirectional->isChecked()) {
        ParticipantRelationship reverse {toId, fromId, addressEdit->text().trimmed(), descriptionEdit->toPlainText().trimmed()};
        if (!m_roomManager.setParticipantRelationship(room->roomId, reverse)) showMutationErrorAndReload();
    }
}

AIConversationRoom *AIConversationConsoleWindow::currentRoom()
{
    const QString roomId = currentRoomId();
    return roomId.isEmpty() ? nullptr : m_roomManager.room(roomId);
}

QString AIConversationConsoleWindow::currentRoomId() const
{
    QListWidgetItem *item = m_roomList ? m_roomList->currentItem() : nullptr;
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QStringList AIConversationConsoleWindow::selectedParticipantIds() const
{
    QStringList ids;
    for (QListWidgetItem *item : m_participantList->selectedItems()) {
        ids.append(item->data(Qt::UserRole).toString());
    }
    return ids;
}

void AIConversationConsoleWindow::appendMessage(const ConversationMessage &message)
{
    const QString time = message.timestamp.toString(QStringLiteral("HH:mm:ss"));
    QStringList statuses;
    statuses.append(QStringLiteral("AI:%1").arg(messageStatusText(message.status)));
    if (message.senderType == ConversationSenderType::Pet) {
        statuses.append(QStringLiteral("\u684c\u9762:%1").arg(desktopStatusText(message.desktopDeliveryStatus)));
        statuses.append(QStringLiteral("\u52a8\u4f5c:%1").arg(actionStatusText(message.actionExecutionStatus)));
    }
    if (!message.errorCode.isEmpty()) statuses.append(QStringLiteral("\u539f\u56e0:%1").arg(message.errorCode));
    QString background = QString::fromLatin1(ThemeConstants::Surface);
    QString accent = QString::fromLatin1(ThemeConstants::Border);
    if (message.senderType == ConversationSenderType::User) {
        background = QString::fromLatin1(ThemeConstants::AccentSoft);
        accent = QString::fromLatin1(ThemeConstants::Accent);
    } else if (message.senderType == ConversationSenderType::System
               || message.senderType == ConversationSenderType::InternalControl) {
        background = QString::fromLatin1(ThemeConstants::InfoSoft);
        accent = QString::fromLatin1(ThemeConstants::Info);
    } else if (message.status == ConversationMessageStatus::Failed
               || message.status == ConversationMessageStatus::TimedOut
               || message.status == ConversationMessageStatus::RuntimeDispatchFailed) {
        background = QString::fromLatin1(ThemeConstants::ErrorSoft);
        accent = QString::fromLatin1(ThemeConstants::Error);
    } else if (message.status == ConversationMessageStatus::Pending) {
        background = QString::fromLatin1(ThemeConstants::WarningSoft);
        accent = QString::fromLatin1(ThemeConstants::Warning);
    }

    const QString html = QStringLiteral(
        "<div style=\"margin:8px 2px;padding:9px 11px;background:%1;border-left:4px solid %2;\">"
        "<div><b>%3</b> <span style=\"color:%4;font-size:11px;\">%5 · %6</span></div>"
        "<div style=\"margin-top:5px;\">%7</div></div>")
        .arg(background,
             accent,
             message.senderName.toHtmlEscaped(),
             QString::fromLatin1(ThemeConstants::SecondaryText),
             time.toHtmlEscaped(),
             statuses.join(QStringLiteral(" · ")).toHtmlEscaped(),
             message.content.toHtmlEscaped());
    m_transcript->append(html);
}

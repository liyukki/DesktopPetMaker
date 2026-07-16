#ifndef AICONVERSATIONCONSOLEWINDOW_H
#define AICONVERSATIONCONSOLEWINDOW_H

#include "aiconversationroommanager.h"

#include <QMainWindow>

class QComboBox;
class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTextEdit;
class RuntimePetManager;

class AIConversationConsoleWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AIConversationConsoleWindow(RuntimePetManager *runtimeManager = nullptr, QWidget *parent = nullptr);

private:
    void setupUi();
    void refreshRoomList();
    void createRoom();
    void renameCurrentRoom();
    void deleteCurrentRoom();
    void addParticipant();
    void removeParticipant();
    void sendMessage();
    void cancelCurrentTurn();
    void loadCurrentRoom();
    void refreshParticipants(const AIConversationRoom &room);
    void saveCurrentRoomConfiguration();
    void showMutationErrorAndReload();
    void editParticipantWeight();
    void editParticipantRelationship();
    AIConversationRoom *currentRoom();
    QString currentRoomId() const;
    QStringList selectedParticipantIds() const;
    void appendMessage(const ConversationMessage &message);

    AIConversationRoomManager m_roomManager;
    RuntimePetManager *m_runtimeManager {nullptr};
    QListWidget *m_roomList {nullptr};
    QListWidget *m_participantList {nullptr};
    QComboBox *m_modeCombo {nullptr};
    QComboBox *m_directedTargetCombo {nullptr};
    QSpinBox *m_minRespondersSpin {nullptr};
    QSpinBox *m_maxRespondersSpin {nullptr};
    QCheckBox *m_persistHistoryCheck {nullptr};
    QSpinBox *m_historyLimitSpin {nullptr};
    QTextEdit *m_transcript {nullptr};
    QTextEdit *m_input {nullptr};
    QPushButton *m_sendButton {nullptr};
    QPushButton *m_cancelButton {nullptr};
    QPushButton *m_clearHistoryButton {nullptr};
    QPushButton *m_editWeightButton {nullptr};
    QPushButton *m_editRelationshipButton {nullptr};
    bool m_loadingRoom {false};
};

#endif // AICONVERSATIONCONSOLEWINDOW_H

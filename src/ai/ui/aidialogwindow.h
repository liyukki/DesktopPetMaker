#ifndef AIDIALOGWINDOW_H
#define AIDIALOGWINDOW_H

#include <QDialog>
#include <QJsonObject>
#include <QPointer>
#include <QSharedPointer>
#include <QStringList>
#include <QVector>

#include "petconversationcontext.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class AIRequestHandle;
class PetAiRequestLease;

class AIDialogWindow : public QDialog
{
    Q_OBJECT

public:
    explicit AIDialogWindow(const QString &petName,
                            const QString &systemPrompt = QString(),
                            const QSharedPointer<PetConversationContext> &conversationContext = {},
                            QWidget *parent = nullptr);
    ~AIDialogWindow() override;

    void displayExternalPetMessage(const QString &message);
    void displaySystemMessage(const QString &message);
    void setExternalBusy(bool busy);

signals:
    void messageSent(const QString &message);
    void aiGenerationStarted();
    void aiGenerationFinished();
    void aiActionRequested(const QString &actionId, const QJsonObject &parameters);

public slots:
    void onPetInitiate(const QString &message);

private:
    void setupUi();
    void applyStyle();
    void setSending(bool sending);
    void onSendClicked();
    void onInputReturn();
    void addMessage(const QString &speaker, const QString &message, bool isPet = false);
    void releaseOwnRequest();

    QTextEdit *m_chatDisplay {nullptr};
    QLineEdit *m_inputEdit {nullptr};
    QPushButton *m_sendButton {nullptr};
    QLabel *m_petNameLabel {nullptr};

    QString m_petName;
    QString m_systemPrompt;
    bool m_waitingForAi {false};
    bool m_externalBusy {false};
    bool m_ownRequestActive {false};
    QSharedPointer<PetConversationContext> m_conversationContext;
    QSharedPointer<PetAiRequestLease> m_ownLease;
    QPointer<AIRequestHandle> m_requestHandle;
};

#endif // AIDIALOGWINDOW_H

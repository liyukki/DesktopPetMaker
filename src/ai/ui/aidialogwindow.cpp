#include "aidialogwindow.h"
#include "ui/theme/apptheme.h"
#include "ui/theme/iconprovider.h"
#include "ui/theme/themeconstants.h"

#include "aiprovider.h"
#include "aiproviderprofileregistry.h"
#include "petairequestcoordinator.h"

#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

AIDialogWindow::AIDialogWindow(const QString &petName,
                               const QString &systemPrompt,
                               const QSharedPointer<PetConversationContext> &conversationContext,
                               QWidget *parent)
    : QDialog(parent)
    , m_petName(petName.trimmed().isEmpty() ? QStringLiteral("Pet") : petName.trimmed())
    , m_systemPrompt(systemPrompt)
    , m_conversationContext(conversationContext ? conversationContext
                                                : QSharedPointer<PetConversationContext>::create())
{
    setAttribute(Qt::WA_DeleteOnClose);
    setupUi();
    applyStyle();
    for (const auto &entry : m_conversationContext->history) {
        const bool isPet = entry.role != ChatHistoryRole::User;
        addMessage(isPet ? m_petName : QStringLiteral("你"), entry.content, isPet);
    }
    setExternalBusy(m_conversationContext->busy());
}
AIDialogWindow::~AIDialogWindow()
{
    if (m_requestHandle && !m_requestHandle->isFinished()) {
        m_requestHandle->cancel();
    }
    releaseOwnRequest();
}
void AIDialogWindow::setupUi()
{
    setWindowTitle(QStringLiteral("💬 %1").arg(m_petName));
    resize(460, 560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *titleRow = new QHBoxLayout();
    m_petNameLabel = new QLabel(QStringLiteral("  %1").arg(m_petName), this);
    QFont titleFont = m_petNameLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    m_petNameLabel->setFont(titleFont);
    titleRow->addWidget(m_petNameLabel);
    titleRow->addStretch(1);

    m_chatDisplay = new QTextEdit(this);
    m_chatDisplay->setReadOnly(true);
    m_chatDisplay->setAcceptRichText(true);
    m_chatDisplay->setUndoRedoEnabled(false);
    m_chatDisplay->setFrameShape(QFrame::NoFrame);

    auto *inputRow = new QHBoxLayout();
    m_inputEdit = new QLineEdit(this);
    m_inputEdit->setPlaceholderText(tr("输入文字..."));
    m_sendButton = new QPushButton(tr("发送"), this);
    inputRow->addWidget(m_inputEdit, 1);
    inputRow->addWidget(m_sendButton);

    root->addLayout(titleRow);
    root->addWidget(m_chatDisplay, 1);
    root->addLayout(inputRow);

    connect(m_sendButton, &QPushButton::clicked, this, &AIDialogWindow::onSendClicked);
    connect(m_inputEdit, &QLineEdit::returnPressed, this, &AIDialogWindow::onInputReturn);
}

void AIDialogWindow::applyStyle()
{
    setStyleSheet(QString());
    AppTheme::setButtonRole(m_sendButton, QStringLiteral("primary"));
    IconProvider::apply(m_sendButton, AppIcon::Chat);

    QString documentStyle = QStringLiteral(R"(
        p { margin: 0; }
        .row-left { text-align: left; margin: 6px 0; }
        .row-right { text-align: right; margin: 6px 0; }
        .bubble-left, .bubble-right {
            display: inline-block;
            max-width: 70%;
            padding: 8px 12px;
            border-radius: 12px;
            margin: 0;
            word-wrap: break-word;
        }
        .bubble-left {
            background: @SURFACE@;
            color: @PRIMARY@;
            border: 1px solid @BORDER@;
        }
        .bubble-right {
            background: @ACCENT_SOFT@;
            color: @PRIMARY@;
            border: 1px solid @FOCUS@;
        }
        .meta {
            color: @SECONDARY@;
            font-size: 11px;
        }
        .tail-left, .tail-right {
            font-size: 18px;
            line-height: 1;
            vertical-align: bottom;
        }
        .tail-left { color: @SURFACE@; }
        .tail-right { color: @ACCENT_SOFT@; }
    )");
    documentStyle.replace(QStringLiteral("@SURFACE@"), QString::fromLatin1(ThemeConstants::Surface));
    documentStyle.replace(QStringLiteral("@PRIMARY@"), QString::fromLatin1(ThemeConstants::PrimaryText));
    documentStyle.replace(QStringLiteral("@SECONDARY@"), QString::fromLatin1(ThemeConstants::SecondaryText));
    documentStyle.replace(QStringLiteral("@BORDER@"), QString::fromLatin1(ThemeConstants::Border));
    documentStyle.replace(QStringLiteral("@ACCENT_SOFT@"), QString::fromLatin1(ThemeConstants::AccentSoft));
    documentStyle.replace(QStringLiteral("@FOCUS@"), QString::fromLatin1(ThemeConstants::FocusRing));
    m_chatDisplay->document()->setDefaultStyleSheet(documentStyle);
}

void AIDialogWindow::onPetInitiate(const QString &message)
{
    addMessage(m_petName, message, true);
    m_conversationContext->history.append({ChatHistoryRole::Assistant, message});
}

void AIDialogWindow::setSending(bool sending)
{
    m_waitingForAi = sending;
    const bool locked = sending || m_externalBusy;
    m_inputEdit->setEnabled(!locked);
    m_sendButton->setEnabled(!locked);
    if (m_externalBusy && !sending) {
        m_sendButton->setText(QStringLiteral("思考中"));
    } else {
        m_sendButton->setText(sending ? tr("...") : QStringLiteral("发送"));
    }
}

void AIDialogWindow::setExternalBusy(bool busy)
{
    m_externalBusy = busy;
    m_inputEdit->setPlaceholderText(busy ? QStringLiteral("桌宠正在想事情……")
                                         : QStringLiteral("输入文字..."));
    setSending(m_waitingForAi);
}

void AIDialogWindow::displayExternalPetMessage(const QString &message)
{
    addMessage(m_petName, message, true);
}

void AIDialogWindow::displaySystemMessage(const QString &message)
{
    addMessage(QStringLiteral("系统"), message, true);
}
void AIDialogWindow::onSendClicked()
{
    if (m_waitingForAi || m_externalBusy || m_conversationContext->busy()) {
        setExternalBusy(true);
        return;
    }

    const QString message = m_inputEdit->text().trimmed();
    if (message.isEmpty()) {
        return;
    }

    m_inputEdit->clear();
    addMessage(QStringLiteral("你"), message, false);
    m_conversationContext->history.append({ChatHistoryRole::User, message});
    emit messageSent(message);

    AIProviderProfileRegistry profiles;
    const ProviderLookupResult resolved = profiles.resolveStrict(m_conversationContext->providerProfileId);
    if (!resolved.ok) {
        addMessage(QStringLiteral("系统"), QStringLiteral("AI 请求失败：%1").arg(resolved.message), true);
        return;
    }

    QString leaseError;
    m_ownLease = PetAiRequestCoordinator::instance().acquire(m_conversationContext->effectiveProjectId,
                                                              m_conversationContext->petProjectPath,
                                                              PetAiRequestSource::SingleChat,
                                                              {},
                                                              &leaseError);
    if (!m_ownLease) {
        addMessage(QStringLiteral("系统"), QStringLiteral("AI 请求未发送：%1").arg(leaseError), true);
        return;
    }

    setSending(true);
    emit aiGenerationStarted();
    m_conversationContext->beginRequest();
    m_ownRequestActive = true;
    QPointer<AIDialogWindow> guard(this);
    AIProvider &provider = AIProvider::instance();
    AIChatRequest request;
    request.characterName = m_conversationContext->characterName.trimmed().isEmpty() ? m_petName : m_conversationContext->characterName;
    request.systemPrompt = m_conversationContext->systemPrompt.trimmed().isEmpty() ? m_systemPrompt : m_conversationContext->systemPrompt;
    request.history = m_conversationContext->history;
    request.userMessage = message;
    request.allowedActionIds = m_conversationContext->allowedActionIds;
    request.allowedActionDescriptors = m_conversationContext->allowedActionDescriptors;
    m_requestHandle = provider.sendMessage(resolved.profile, request, [guard](const AIRequestResult &result) {
        if (!guard) {
            return;
        }

        guard->releaseOwnRequest();
        guard->setSending(false);
        if (result.success) {
            const AIChatReply parsed = AIProvider::parseStructuredReply(result.message, guard->m_conversationContext->allowedActionIds);
            const QString visibleReply = parsed.reply.trimmed().isEmpty() ? result.message : parsed.reply.trimmed();
            guard->addMessage(guard->m_petName, visibleReply, true);
            guard->m_conversationContext->history.append({ChatHistoryRole::Assistant, visibleReply});
            if (!parsed.actionId.isEmpty()) {
                emit guard->aiActionRequested(parsed.actionId, parsed.actionParameters);
            }
        } else {
            const QString detail = result.message.trimmed().isEmpty()
                ? QStringLiteral("Unknown request failure.")
                : result.message.trimmed();
            guard->addMessage(QStringLiteral("系统"),
                              QStringLiteral("AI 请求失败 [%1]：%2").arg(result.errorCode, detail),
                              true);
        }
        guard->aiGenerationFinished();
    });
    m_ownLease->bindHandle(m_requestHandle);
}

void AIDialogWindow::releaseOwnRequest()
{
    if (!m_ownRequestActive) {
        return;
    }
    m_ownRequestActive = false;
    m_requestHandle.clear();
    m_ownLease.reset();
    m_conversationContext->endRequest();
}
void AIDialogWindow::onInputReturn()
{
    onSendClicked();
}

void AIDialogWindow::addMessage(const QString &speaker, const QString &message, bool isPet)
{
    const QString time = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
    const QString escaped = message.toHtmlEscaped().replace('\n', QStringLiteral("<br>"));
    const QString rowClass = isPet ? QStringLiteral("row-left") : QStringLiteral("row-right");
    const QString bubbleClass = isPet ? QStringLiteral("bubble-left") : QStringLiteral("bubble-right");
    const QString tailClass = isPet ? QStringLiteral("tail-left") : QStringLiteral("tail-right");
    const QString tail = isPet ? QStringLiteral("◥") : QStringLiteral("◢");

    const QString html = QStringLiteral(
        "<p class='%1'>"
        "<span class='%2'>%3</span>"
        "<span class='%4'>%5</span>"
        "</p>"
        "<p class='meta %1'>%6  %7</p>")
                             .arg(rowClass,
                                  bubbleClass,
                                  escaped,
                                  tailClass,
                                  tail,
                                  speaker.toHtmlEscaped(),
                                  time.toHtmlEscaped());

    m_chatDisplay->append(html);
    QTextCursor cursor = m_chatDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_chatDisplay->setTextCursor(cursor);
    m_chatDisplay->verticalScrollBar()->setValue(m_chatDisplay->verticalScrollBar()->maximum());
}

#ifndef AIPROVIDER_H
#define AIPROVIDER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>
#include <functional>

#include "aiactiondescriptor.h"
#include "chathistoryentry.h"
#include "aiproviderprofileregistry.h"

class QNetworkAccessManager;
class QNetworkReply;

struct AIRequestResult
{
    bool success {false};
    QString message;
    QString errorCode;
    bool cancelled {false};
};

class AIRequestHandle : public QObject
{
    Q_OBJECT

public:
    explicit AIRequestHandle(QObject *parent = nullptr);

    QString requestId() const;
    bool isFinished() const { return m_finished; }
    bool isCancelled() const { return m_cancelled; }
    void cancel(const QString &reason = QStringLiteral("Cancelled"),
                const QString &errorCode = QStringLiteral("Cancelled"));

signals:
    void finished(const AIRequestResult &result);

private:
    friend class AIProvider;
    void setReply(QNetworkReply *reply);
    void complete(const AIRequestResult &result);

    QString m_requestId;
    QPointer<QNetworkReply> m_reply;
    bool m_finished {false};
    bool m_cancelled {false};
};

struct AIChatRequest
{
    QString characterName;
    QString systemPrompt;
    QVector<ChatHistoryEntry> history;
    QString userMessage;
    QStringList allowedActionIds;
    QVector<AIActionDescriptor> allowedActionDescriptors;
    bool proactive {false};
};

struct AIChatReply
{
    QString reply;
    QString actionId;
    QJsonObject actionParameters;
};

class AIProvider : public QObject
{
    Q_OBJECT

public:
    using ReplyCallback = std::function<void(bool success, const QString &message)>;
    using ResultCallback = std::function<void(const AIRequestResult &result)>;
    struct AIConfig {
        QString baseUrl;
        QString apiKey;
        QString model;
    };

    static AIProvider &instance();

    AIRequestHandle *sendMessage(const AIProviderProfile &profile,
                                 const AIChatRequest &request,
                                 ResultCallback callback = {});
    AIRequestHandle *sendMessage(const AIProviderProfile &profile,
                                 const AIChatRequest &request,
                                 ReplyCallback callback);
    static AIChatReply parseStructuredReply(const QString &raw, const QStringList &allowedActionIds);
    AIRequestHandle *testConnection(const AIConfig &config, ResultCallback callback = {});
    AIRequestHandle *testConnection(const AIConfig &config, ReplyCallback callback);
    QJsonObject buildProfileRequestBodyForTest(const AIChatRequest &request,
                                               const AIConfig &config,
                                               int maxTokens = 220) const;

private:
    explicit AIProvider(QObject *parent = nullptr);

    QString endpointUrl(const AIConfig &config) const;
    bool isConfigured(const AIConfig &config) const;
    bool isDeepSeekV4Model(const AIConfig &config) const;
    QNetworkRequest buildRequest(const AIConfig &config) const;
    QJsonObject buildProfileRequestBody(const AIChatRequest &request,
                                        int maxTokens,
                                        const AIConfig &config) const;
    AIRequestHandle *postChatRequest(const AIConfig &config, const QJsonObject &body, ResultCallback callback);
    QString parseChatResponse(const QByteArray &raw, bool *ok) const;
    QString resolvedProfileSystemPrompt(const QString &petName, const QString &projectSystemPrompt) const;
    QJsonArray buildMessagesWithSystemPrompt(const QString &systemPrompt,
                                             const QVector<ChatHistoryEntry> &history,
                                             const QString &userInput) const;

    QNetworkAccessManager *m_networkManager {nullptr};
};

#endif // AIPROVIDER_H

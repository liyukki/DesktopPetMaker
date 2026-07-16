#include "aiprovider.h"

#include "credentialstore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QUuid>

namespace {
constexpr int kRequestTimeoutMs = 30000;
constexpr int kHistoryLimit = 12;

QString stripCopiedFormatting(QString value)
{
    value = value.trimmed();
    value.remove(QRegularExpression(QStringLiteral("\\x1B\\[[0-9;]*[A-Za-z]")));
    value.remove(QRegularExpression(QStringLiteral("\\[(?:0|1|22|39)m\\]$")));
    return value.trimmed();
}

QString normalizeModelForBase(const QString &baseUrl, const QString &model)
{
    const QString clean = stripCopiedFormatting(model);
    if (!baseUrl.contains(QStringLiteral("deepseek"), Qt::CaseInsensitive)) {
        return clean;
    }

    if (clean.startsWith(QStringLiteral("deepseek-v4-flash"), Qt::CaseInsensitive)) {
        return QStringLiteral("deepseek-v4-flash");
    }
    if (clean.startsWith(QStringLiteral("deepseek-v4-pro"), Qt::CaseInsensitive)) {
        return QStringLiteral("deepseek-v4-pro");
    }
    if (clean == QStringLiteral("ds4") || clean.isEmpty()) {
        return QStringLiteral("deepseek-v4-flash");
    }
    return clean;
}
}

AIProvider &AIProvider::instance()
{
    static AIProvider provider;
    return provider;
}

AIProvider::AIProvider(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

AIRequestHandle::AIRequestHandle(QObject *parent)
    : QObject(parent)
    , m_requestId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

QString AIRequestHandle::requestId() const
{
    return m_requestId;
}

void AIRequestHandle::setReply(QNetworkReply *reply)
{
    m_reply = reply;
}

void AIRequestHandle::cancel(const QString &reason, const QString &errorCode)
{
    if (m_finished) {
        return;
    }
    m_cancelled = true;
    complete({false, reason, errorCode, true});
    if (m_reply && m_reply->isRunning()) {
        m_reply->abort();
    }
}

void AIRequestHandle::complete(const AIRequestResult &result)
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    emit finished(result);
}

AIRequestHandle *AIProvider::sendMessage(const AIProviderProfile &profile,
                                         const AIChatRequest &request,
                                         ResultCallback callback)
{
    AIConfig config;
    config.baseUrl = profile.baseUrl;
    config.model = profile.model;
    config.apiKey = CredentialStore::instance().readSecret(profile.credentialId);
    if (!isConfigured(config)) {
        auto *handle = new AIRequestHandle(this);
        if (callback) {
            QObject::connect(handle, &AIRequestHandle::finished, handle, [callback](const AIRequestResult &result) {
                callback(result);
            });
        }
        QTimer::singleShot(0, handle, [handle, config]() {
            const QString code = config.apiKey.trimmed().isEmpty()
                ? QStringLiteral("CredentialMissing")
                : QStringLiteral("InvalidConfiguration");
            handle->complete({false, QStringLiteral("%1: AI API is not configured.").arg(code), code, false});
        });
        connect(handle, &AIRequestHandle::finished, handle, &QObject::deleteLater);
        return handle;
    }

    return postChatRequest(config,
                           buildProfileRequestBody(request,
                                                   request.proactive ? 120 : 220,
                                                   config),
                           std::move(callback));
}

AIRequestHandle *AIProvider::sendMessage(const AIProviderProfile &profile,
                                         const AIChatRequest &request,
                                         ReplyCallback callback)
{
    return sendMessage(profile, request, [callback](const AIRequestResult &result) {
        if (callback) {
            callback(result.success, result.message);
        }
    });
}

AIChatReply AIProvider::parseStructuredReply(const QString &raw, const QStringList &allowedActionIds)
{
    QString text = raw.trimmed();
    if (text.startsWith(QStringLiteral("```"))) {
        const int firstNewline = text.indexOf('\n');
        const int lastFence = text.lastIndexOf(QStringLiteral("```"));
        if (firstNewline >= 0 && lastFence > firstNewline) {
            text = text.mid(firstNewline + 1, lastFence - firstNewline - 1).trimmed();
        }
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        AIChatReply plain;
        plain.reply = raw;
        return plain;
    }

    const QJsonObject obj = doc.object();
    AIChatReply parsed;
    parsed.reply = obj.value(QStringLiteral("reply")).toString(raw).trimmed();
    QString action = obj.value(QStringLiteral("action")).toString().trimmed();
    QJsonObject parameters = obj.value(QStringLiteral("parameters")).toObject();
    if (action.isEmpty() && obj.value(QStringLiteral("action")).isObject()) {
        const QJsonObject actionObject = obj.value(QStringLiteral("action")).toObject();
        action = actionObject.value(QStringLiteral("id")).toString().trimmed();
        if (parameters.isEmpty()) {
            parameters = actionObject.value(QStringLiteral("parameters")).toObject();
        }
    }
    if (!action.isEmpty() && allowedActionIds.contains(action)) {
        parsed.actionId = action;
        parsed.actionParameters = parameters;
    }
    if (parsed.reply.isEmpty()) {
        parsed.reply = raw;
    }
    return parsed;
}

AIRequestHandle *AIProvider::testConnection(const AIConfig &config, ResultCallback callback)
{
    if (!isConfigured(config)) {
        auto *handle = new AIRequestHandle(this);
        if (callback) {
            QObject::connect(handle, &AIRequestHandle::finished, handle, [callback](const AIRequestResult &result) {
                callback(result);
            });
        }
        QTimer::singleShot(0, handle, [handle, config]() {
            const QString code = config.apiKey.trimmed().isEmpty()
                ? QStringLiteral("CredentialMissing")
                : QStringLiteral("InvalidConfiguration");
            handle->complete({false, QStringLiteral("%1: AI API is not configured.").arg(code), code, false});
        });
        connect(handle, &AIRequestHandle::finished, handle, &QObject::deleteLater);
        return handle;
    }

    AIChatRequest request;
    request.characterName = QStringLiteral("Pet");
    request.userMessage = QStringLiteral("hi");
    return postChatRequest(config,
                           buildProfileRequestBody(request, 20, config),
                           std::move(callback));
}

AIRequestHandle *AIProvider::testConnection(const AIConfig &config, ReplyCallback callback)
{
    return testConnection(config, [callback](const AIRequestResult &result) {
        if (callback) {
            callback(result.success, result.message);
        }
    });
}

bool AIProvider::isConfigured(const AIConfig &config) const
{
    return !config.baseUrl.isEmpty() && !config.apiKey.isEmpty() && !config.model.isEmpty();
}

bool AIProvider::isDeepSeekV4Model(const AIConfig &config) const
{
    return config.baseUrl.contains(QStringLiteral("deepseek"), Qt::CaseInsensitive)
        && (config.model == QStringLiteral("deepseek-v4-flash")
            || config.model == QStringLiteral("deepseek-v4-pro"));
}

QNetworkRequest AIProvider::buildRequest(const AIConfig &config) const
{
    QNetworkRequest request{QUrl(endpointUrl(config))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(config.apiKey).toUtf8());
    return request;
}

QJsonObject AIProvider::buildProfileRequestBodyForTest(const AIChatRequest &request,
                                                       const AIConfig &config,
                                                       int maxTokens) const
{
    return buildProfileRequestBody(request, maxTokens, config);
}

QJsonObject AIProvider::buildProfileRequestBody(const AIChatRequest &request,
                                                int maxTokens,
                                                const AIConfig &config) const
{
    QString systemPrompt = resolvedProfileSystemPrompt(request.characterName, request.systemPrompt);
    if (!request.allowedActionDescriptors.isEmpty()) {
        QStringList lines;
        for (const auto &descriptor : request.allowedActionDescriptors) {
            lines.append(QStringLiteral("- id: %1 | name: %2 | category: %3 | description: %4 | states: %5 | parameters: %6")
                             .arg(descriptor.id,
                                  descriptor.displayName,
                                  descriptor.category,
                                  descriptor.description,
                                  descriptor.allowedStates.join(QStringLiteral(", ")),
                                  QString::fromUtf8(QJsonDocument(descriptor.parameterSchema).toJson(QJsonDocument::Compact))));
        }
        systemPrompt += QStringLiteral("\n\nYou may optionally reply as compact JSON: {\"reply\":\"text\",\"action\":{\"id\":\"action_id\",\"parameters\":{}}}. Allowed actions:\n%1\nUse only schema-valid parameters. If no action is needed, omit action or use null. Never output shell commands, file paths, or programs.")
                            .arg(lines.join('\n'));
    } else if (!request.allowedActionIds.isEmpty()) {
        systemPrompt += QStringLiteral("\n\nYou may optionally reply as compact JSON: {\"reply\":\"text\",\"action\":{\"id\":\"action_id\",\"parameters\":{}}}. Allowed action IDs: %1. If no action is needed, omit action or use null. Never output shell commands, file paths, or programs.")
                            .arg(request.allowedActionIds.join(QStringLiteral(", ")));
    }

    QJsonObject body;
    body.insert("model", normalizeModelForBase(config.baseUrl, config.model));
    body.insert("max_tokens", maxTokens);
    body.insert("temperature", 0.8);
    body.insert("messages", buildMessagesWithSystemPrompt(systemPrompt, request.history, request.userMessage));

    if (isDeepSeekV4Model(config)) {
        QJsonObject thinking;
        thinking.insert("type", QStringLiteral("disabled"));
        body.insert("thinking", thinking);
    }

    return body;
}

AIRequestHandle *AIProvider::postChatRequest(const AIConfig &config,
                                              const QJsonObject &body,
                                              ResultCallback callback)
{
    auto *handle = new AIRequestHandle(this);
    if (callback) {
        QObject::connect(handle, &AIRequestHandle::finished, handle, [callback](const AIRequestResult &result) {
            callback(result);
        });
    }
    QNetworkReply *reply = m_networkManager->post(buildRequest(config), QJsonDocument(body).toJson(QJsonDocument::Compact));
    handle->setReply(reply);
    QTimer *timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    QObject::connect(timeout, &QTimer::timeout, reply, [handle]() {
        handle->cancel(QStringLiteral("AI request timed out."), QStringLiteral("TimedOut"));
    });
    timeout->start(kRequestTimeoutMs);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, handle]() {
        if (handle->isFinished()) {
            reply->deleteLater();
            return;
        }
        const QByteArray raw = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            QString error = reply->errorString();
            if (!raw.isEmpty()) {
                error += QStringLiteral("\n%1").arg(QString::fromUtf8(raw));
            }
            handle->complete({false, error, QStringLiteral("NetworkError"), false});
            reply->deleteLater();
            return;
        }

        bool ok = false;
        const QString content = parseChatResponse(raw, &ok);
        handle->complete({ok,
                          content,
                          ok ? QString() : (content.contains(QStringLiteral("empty"), Qt::CaseInsensitive)
                                              ? QStringLiteral("EmptyResponse")
                                              : QStringLiteral("InvalidResponse")),
                          false});
        reply->deleteLater();
    });
    connect(handle, &AIRequestHandle::finished, handle, &QObject::deleteLater);
    return handle;
}

QString AIProvider::parseChatResponse(const QByteArray &raw, bool *ok) const
{
    if (ok) {
        *ok = false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return QStringLiteral("Invalid JSON response: %1").arg(parseError.errorString());
    }

    const QJsonArray choices = doc.object().value("choices").toArray();
    if (choices.isEmpty()) {
        return QStringLiteral("API response has no choices.");
    }

    const QJsonObject choice = choices.first().toObject();
    QString content = choice.value("message").toObject().value("content").toString().trimmed();
    if (content.isEmpty()) {
        content = choice.value("text").toString().trimmed();
    }
    if (content.isEmpty()) {
        return QStringLiteral("API response is empty.");
    }

    if (ok) {
        *ok = true;
    }
    return content;
}

QString AIProvider::endpointUrl(const AIConfig &config) const
{
    QString url = config.baseUrl;
    if (!url.endsWith(QStringLiteral("/chat/completions"))) {
        url += QStringLiteral("/chat/completions");
    }
    return url;
}

QString AIProvider::resolvedProfileSystemPrompt(const QString &petName, const QString &projectSystemPrompt) const
{
    QString prompt = projectSystemPrompt.trimmed();
    if (prompt.isEmpty()) {
        prompt = tr("You are a small desktop pet named %1. Reply briefly and stay in character.");
    }
    prompt.replace(QStringLiteral("%1"), petName.trimmed().isEmpty() ? QStringLiteral("Pet") : petName.trimmed());
    return prompt;
}

namespace {
QString chatRoleName(ChatHistoryRole role)
{
    switch (role) {
    case ChatHistoryRole::User:
        return QStringLiteral("user");
    case ChatHistoryRole::Assistant:
        return QStringLiteral("assistant");
    case ChatHistoryRole::System:
        return QStringLiteral("system");
    }
    return QStringLiteral("user");
}
}

QJsonArray AIProvider::buildMessagesWithSystemPrompt(const QString &systemPrompt,
                                                     const QVector<ChatHistoryEntry> &history,
                                                     const QString &userInput) const
{
    QJsonArray messages;

    QJsonObject system;
    system.insert("role", QStringLiteral("system"));
    system.insert("content", systemPrompt);
    messages.append(system);

    const int start = qMax(0, history.size() - kHistoryLimit);
    for (int i = start; i < history.size(); ++i) {
        const QString role = chatRoleName(history.at(i).role);
        const QString content = history.at(i).content.trimmed();
        if (content.isEmpty()) {
            continue;
        }

        QJsonObject msg;
        msg.insert("role", role);
        msg.insert("content", content);
        messages.append(msg);
    }

    if (!userInput.trimmed().isEmpty()
        && (history.isEmpty()
            || history.last().role != ChatHistoryRole::User
            || history.last().content != userInput)) {
        QJsonObject latest;
        latest.insert("role", QStringLiteral("user"));
        latest.insert("content", userInput.trimmed());
        messages.append(latest);
    }

    return messages;
}

#include "aiproviderprofileregistry.h"

#include "credentialstore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUuid>
#include <QUrl>
#include <algorithm>

namespace {
QJsonArray serializeProfiles(const QVector<AIProviderProfile> &profiles)
{
    QJsonArray array;
    for (const AIProviderProfile &item : profiles) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), item.id);
        obj.insert(QStringLiteral("displayName"), item.displayName);
        obj.insert(QStringLiteral("providerType"), item.providerType);
        obj.insert(QStringLiteral("baseUrl"), item.baseUrl);
        obj.insert(QStringLiteral("model"), item.model);
        obj.insert(QStringLiteral("credentialId"), item.credentialId);
        array.append(obj);
    }
    return array;
}

bool writeProfiles(QSettings &settings, const QString &key, const QVector<AIProviderProfile> &profiles)
{
    settings.setValue(key, QJsonDocument(serializeProfiles(profiles)).toJson(QJsonDocument::Compact));
    settings.sync();
    return settings.status() == QSettings::NoError;
}
}

QString AIProviderProfileRegistry::settingsKey() const
{
    return QStringLiteral("ai/providerProfiles");
}

QVector<AIProviderProfile> AIProviderProfileRegistry::profiles() const
{
    migrateLegacyAiSettingsOnce();
    QVector<AIProviderProfile> result;
    const QByteArray raw = QSettings().value(settingsKey()).toByteArray();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    for (const QJsonValue &value : doc.array()) {
        const QJsonObject obj = value.toObject();
        AIProviderProfile profile;
        profile.id = obj.value(QStringLiteral("id")).toString();
        profile.displayName = obj.value(QStringLiteral("displayName")).toString();
        profile.providerType = obj.value(QStringLiteral("providerType")).toString(QStringLiteral("OpenAICompatible"));
        profile.baseUrl = obj.value(QStringLiteral("baseUrl")).toString();
        profile.model = obj.value(QStringLiteral("model")).toString();
        profile.credentialId = obj.value(QStringLiteral("credentialId")).toString(profile.id);
        if (!profile.id.isEmpty()) {
            result.append(profile);
        }
    }
    if (result.isEmpty()) {
        result.append(defaultProfile());
    }
    return result;
}

void AIProviderProfileRegistry::migrateLegacyAiSettingsOnce() const
{
    QSettings settings;
    if (!settings.value(settingsKey()).toByteArray().isEmpty()) {
        return;
    }

    const QString legacyKey = settings.value(QStringLiteral("ai/apiKey")).toString();
    if (legacyKey.trimmed().isEmpty()) {
        return;
    }

    AIProviderProfile profile = defaultProfile();
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), profile.id);
    obj.insert(QStringLiteral("displayName"), profile.displayName);
    obj.insert(QStringLiteral("providerType"), profile.providerType);
    obj.insert(QStringLiteral("baseUrl"), profile.baseUrl);
    obj.insert(QStringLiteral("model"), profile.model);
    obj.insert(QStringLiteral("credentialId"), profile.credentialId);
    QJsonArray array;
    array.append(obj);

    if (CredentialStore::instance().writeSecret(profile.credentialId, legacyKey.trimmed())) {
        settings.setValue(settingsKey(), QJsonDocument(array).toJson(QJsonDocument::Compact));
        settings.remove(QStringLiteral("ai/apiKey"));
        settings.sync();
    }
}

AIProviderProfile AIProviderProfileRegistry::profile(const QString &id) const
{
    for (const AIProviderProfile &profile : profiles()) {
        if (profile.id == id) {
            return profile;
        }
    }
    return {};
}

ProviderLookupResult AIProviderProfileRegistry::resolveStrict(const QString &id) const
{
    ProviderLookupResult result;
    const QString cleanId = id.trimmed();
    if (cleanId.isEmpty()) {
        result.error = ProviderLookupError::EmptyId;
        result.message = QStringLiteral("AI service is not configured for this desktop pet.");
        return result;
    }

    const AIProviderProfile resolved = profile(cleanId);
    if (resolved.id.isEmpty()) {
        result.error = ProviderLookupError::NotFound;
        result.message = QStringLiteral("The selected AI service profile no longer exists.");
        return result;
    }

    const QUrl url(resolved.baseUrl.trimmed());
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()
        || (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http"))) {
        result.error = ProviderLookupError::InvalidBaseUrl;
        result.message = QStringLiteral("The selected AI service URL is invalid.");
        return result;
    }
    if (resolved.model.trimmed().isEmpty()) {
        result.error = ProviderLookupError::EmptyModel;
        result.message = QStringLiteral("The selected AI service has no model configured.");
        return result;
    }
    if (resolved.credentialId.trimmed().isEmpty()
        || CredentialStore::instance().readSecret(resolved.credentialId).trimmed().isEmpty()) {
        result.error = ProviderLookupError::MissingCredential;
        result.message = QStringLiteral("CredentialMissing: the selected AI service has no API credential.");
        return result;
    }

    result.ok = true;
    result.error = ProviderLookupError::None;
    result.profile = resolved;
    return result;
}

AIProviderProfile AIProviderProfileRegistry::defaultProfile() const
{
    QSettings settings;
    AIProviderProfile profile;
    profile.id = QStringLiteral("default");
    profile.displayName = QStringLiteral("默认 AI 服务");
    profile.providerType = QStringLiteral("OpenAICompatible");
    profile.baseUrl = settings.value(QStringLiteral("ai/baseUrl"), QStringLiteral("https://api.deepseek.com/v1")).toString();
    profile.model = settings.value(QStringLiteral("ai/model"), QStringLiteral("deepseek-v4-flash")).toString();
    profile.credentialId = QStringLiteral("default");
    return profile;
}

bool AIProviderProfileRegistry::saveProfile(const AIProviderProfile &profile)
{
    if (profile.id.trimmed().isEmpty()) {
        return false;
    }
    QVector<AIProviderProfile> all = profiles();
    bool replaced = false;
    for (AIProviderProfile &existing : all) {
        if (existing.id == profile.id) {
            existing = profile;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        all.append(profile);
    }
    QSettings settings;
    return writeProfiles(settings, settingsKey(), all);
}

bool AIProviderProfileRegistry::deleteProfile(const QString &id)
{
    if (id.trimmed().isEmpty() || id == QStringLiteral("default")) {
        return false;
    }
    QVector<AIProviderProfile> all = profiles();
    const qsizetype originalSize = all.size();
    all.erase(std::remove_if(all.begin(), all.end(), [&id](const AIProviderProfile &profile) {
                  return profile.id == id;
              }),
              all.end());
    if (all.size() == originalSize) {
        return false;
    }
    QSettings settings;
    return writeProfiles(settings, settingsKey(), all);
}

QString AIProviderProfileRegistry::duplicateProfile(const QString &id)
{
    const AIProviderProfile original = profile(id);
    if (original.id.isEmpty()) {
        return {};
    }
    AIProviderProfile copy = original;
    copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copy.displayName += QStringLiteral(" 副本");
    copy.credentialId = copy.id;
    return saveProfile(copy) ? copy.id : QString();
}

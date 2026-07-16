#ifndef AIPROVIDERPROFILEREGISTRY_H
#define AIPROVIDERPROFILEREGISTRY_H

#include <QString>
#include <QVector>

enum class ProviderLookupError
{
    None,
    EmptyId,
    NotFound,
    InvalidBaseUrl,
    EmptyModel,
    MissingCredential
};

struct AIProviderProfile
{
    QString id;
    QString displayName;
    QString providerType;
    QString baseUrl;
    QString model;
    QString credentialId;
};

struct ProviderLookupResult
{
    bool ok {false};
    AIProviderProfile profile;
    ProviderLookupError error {ProviderLookupError::NotFound};
    QString message;
};

class AIProviderProfileRegistry
{
public:
    QVector<AIProviderProfile> profiles() const;
    AIProviderProfile profile(const QString &id) const;
    ProviderLookupResult resolveStrict(const QString &id) const;
    AIProviderProfile defaultProfile() const;
    bool saveProfile(const AIProviderProfile &profile);
    bool deleteProfile(const QString &id);
    QString duplicateProfile(const QString &id);

private:
    QString settingsKey() const;
    void migrateLegacyAiSettingsOnce() const;
};

#endif // AIPROVIDERPROFILEREGISTRY_H

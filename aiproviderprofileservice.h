#ifndef AIPROVIDERPROFILESERVICE_H
#define AIPROVIDERPROFILESERVICE_H

#include "aiproviderprofileregistry.h"

enum class CredentialUpdateMode
{
    KeepExisting,
    Replace,
    Clear
};

struct ProfileMutationResult
{
    bool ok {false};
    QString message;
};

class ICredentialStore
{
public:
    virtual ~ICredentialStore() = default;
    virtual QString readSecret(const QString &credentialId) const = 0;
    virtual bool writeSecret(const QString &credentialId, const QString &secret) = 0;
    virtual bool deleteSecret(const QString &credentialId) = 0;
};

class IProviderProfileStore
{
public:
    virtual ~IProviderProfileStore() = default;
    virtual AIProviderProfile profile(const QString &profileId) const = 0;
    virtual bool saveProfile(const AIProviderProfile &profile) = 0;
};

class AIProviderProfileService
{
public:
    AIProviderProfileService(ICredentialStore &credentials, IProviderProfileStore &profiles);

    ProfileMutationResult saveProfile(const AIProviderProfile &profile,
                                      CredentialUpdateMode credentialMode,
                                      const QString &replacementSecret = {});

private:
    bool restoreSecret(const QString &credentialId, const QString &oldSecret) const;

    ICredentialStore &m_credentials;
    IProviderProfileStore &m_profiles;
};

class CredentialStoreAdapter final : public ICredentialStore
{
public:
    QString readSecret(const QString &credentialId) const override;
    bool writeSecret(const QString &credentialId, const QString &secret) override;
    bool deleteSecret(const QString &credentialId) override;
};

class ProviderProfileRegistryStoreAdapter final : public IProviderProfileStore
{
public:
    AIProviderProfile profile(const QString &profileId) const override;
    bool saveProfile(const AIProviderProfile &profile) override;
};

#endif // AIPROVIDERPROFILESERVICE_H

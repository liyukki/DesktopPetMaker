#include "aiproviderprofileservice.h"

#include "credentialstore.h"

#include <QUrl>

AIProviderProfileService::AIProviderProfileService(ICredentialStore &credentials,
                                                   IProviderProfileStore &profiles)
    : m_credentials(credentials)
    , m_profiles(profiles)
{
}

ProfileMutationResult AIProviderProfileService::saveProfile(const AIProviderProfile &profile,
                                                            CredentialUpdateMode credentialMode,
                                                            const QString &replacementSecret)
{
    const QUrl url(profile.baseUrl.trimmed());
    if (profile.id.trimmed().isEmpty() || profile.credentialId.trimmed().isEmpty()
        || !url.isValid() || url.host().isEmpty() || profile.model.trimmed().isEmpty()) {
        return {false, QStringLiteral("AI service configuration is incomplete.")};
    }
    if (credentialMode == CredentialUpdateMode::Replace && replacementSecret.trimmed().isEmpty()) {
        return {false, QStringLiteral("A replacement API key is required.")};
    }

    const AIProviderProfile oldProfile = m_profiles.profile(profile.id);
    const QString oldSecret = m_credentials.readSecret(profile.credentialId);
    bool credentialUpdated = false;
    if (credentialMode == CredentialUpdateMode::Replace) {
        credentialUpdated = m_credentials.writeSecret(profile.credentialId, replacementSecret.trimmed());
    } else if (credentialMode == CredentialUpdateMode::Clear) {
        credentialUpdated = m_credentials.deleteSecret(profile.credentialId);
    } else {
        credentialUpdated = true;
    }
    if (!credentialUpdated) {
        return {false, QStringLiteral("Unable to update the secure API credential.")};
    }

    if (m_profiles.saveProfile(profile)) {
        return {true, {}};
    }

    bool rollbackOk = true;
    if (credentialMode != CredentialUpdateMode::KeepExisting) {
        rollbackOk = restoreSecret(profile.credentialId, oldSecret);
    }
    if (!oldProfile.id.isEmpty()) {
        rollbackOk = m_profiles.saveProfile(oldProfile) && rollbackOk;
    }
    return {false, rollbackOk
                       ? QStringLiteral("The AI service profile was not saved; the previous credential was restored.")
                       : QStringLiteral("The AI service profile was not saved and the rollback needs attention.")};
}

bool AIProviderProfileService::restoreSecret(const QString &credentialId, const QString &oldSecret) const
{
    return oldSecret.isEmpty() ? m_credentials.deleteSecret(credentialId)
                               : m_credentials.writeSecret(credentialId, oldSecret);
}

QString CredentialStoreAdapter::readSecret(const QString &credentialId) const
{
    return CredentialStore::instance().readSecret(credentialId);
}

bool CredentialStoreAdapter::writeSecret(const QString &credentialId, const QString &secret)
{
    return CredentialStore::instance().writeSecret(credentialId, secret);
}

bool CredentialStoreAdapter::deleteSecret(const QString &credentialId)
{
    return CredentialStore::instance().deleteSecret(credentialId);
}

AIProviderProfile ProviderProfileRegistryStoreAdapter::profile(const QString &profileId) const
{
    return AIProviderProfileRegistry().profile(profileId);
}

bool ProviderProfileRegistryStoreAdapter::saveProfile(const AIProviderProfile &profile)
{
    return AIProviderProfileRegistry().saveProfile(profile);
}

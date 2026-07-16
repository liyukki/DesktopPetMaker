#include <QtTest>

#include "aiproviderprofileservice.h"

class FakeCredentialStore final : public ICredentialStore
{
public:
    QString readSecret(const QString &id) const override { return secrets.value(id); }
    bool writeSecret(const QString &id, const QString &secret) override
    {
        if (failWrite) return false;
        secrets.insert(id, secret);
        return true;
    }
    bool deleteSecret(const QString &id) override
    {
        if (failDelete) return false;
        secrets.remove(id);
        return true;
    }
    QHash<QString, QString> secrets;
    bool failWrite {false};
    bool failDelete {false};
};

class FakeProfileStore final : public IProviderProfileStore
{
public:
    AIProviderProfile profile(const QString &id) const override { return profiles.value(id); }
    bool saveProfile(const AIProviderProfile &profile) override
    {
        if (failSave) return false;
        profiles.insert(profile.id, profile);
        return true;
    }
    QHash<QString, AIProviderProfile> profiles;
    bool failSave {false};
};

class ProviderCredentialTests : public QObject
{
    Q_OBJECT
private slots:
    void profileAndCredentialCommitTogether()
    {
        FakeCredentialStore credentials;
        FakeProfileStore profiles;
        AIProviderProfile profile;
        profile.id = QStringLiteral("profile-a");
        profile.credentialId = QStringLiteral("credential-a");
        profile.baseUrl = QStringLiteral("https://example.invalid/v1");
        profile.model = QStringLiteral("test-model");
        AIProviderProfileService service(credentials, profiles);
        const ProfileMutationResult result = service.saveProfile(profile, CredentialUpdateMode::Replace,
                                                                 QStringLiteral("test-secret"));
        QVERIFY(result.ok);
        QCOMPARE(credentials.readSecret(profile.credentialId), QStringLiteral("test-secret"));
        QCOMPARE(profiles.profile(profile.id).id, profile.id);
    }

    void profileFailureRestoresPreviousCredential()
    {
        FakeCredentialStore credentials;
        FakeProfileStore profiles;
        AIProviderProfile profile;
        profile.id = QStringLiteral("profile-a");
        profile.credentialId = QStringLiteral("credential-a");
        profile.baseUrl = QStringLiteral("https://example.invalid/v1");
        profile.model = QStringLiteral("test-model");
        credentials.secrets.insert(profile.credentialId, QStringLiteral("old-secret"));
        profiles.failSave = true;
        AIProviderProfileService service(credentials, profiles);
        const ProfileMutationResult result = service.saveProfile(profile, CredentialUpdateMode::Replace,
                                                                 QStringLiteral("new-secret"));
        QVERIFY(!result.ok);
        QCOMPARE(credentials.readSecret(profile.credentialId), QStringLiteral("old-secret"));
    }
};

QTEST_MAIN(ProviderCredentialTests)
#include "provider_credential_tests.moc"

#include <QtTest>

#include "credentialstore.h"

class CredentialStoreWindowsIntegrationTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        CredentialStore::instance().deleteSecret(testCredentialId());
    }

    void cleanupTestCase()
    {
        QVERIFY(CredentialStore::instance().deleteSecret(testCredentialId()));
    }

    void windowsCredentialRoundTrip()
    {
#ifdef Q_OS_WIN
        CredentialStore &store = CredentialStore::instance();
        QVERIFY(store.writeSecret(testCredentialId(), QStringLiteral("temporary-integration-secret")));
        QCOMPARE(store.readSecret(testCredentialId()), QStringLiteral("temporary-integration-secret"));
        QVERIFY(store.deleteSecret(testCredentialId()));
#else
        QSKIP("Windows Credential Manager integration test");
#endif
    }

private:
    static QString testCredentialId()
    {
        return QStringLiteral("desktop-pet-tests/integration/credential-store-v1");
    }
};

QTEST_MAIN(CredentialStoreWindowsIntegrationTests)
#include "credential_store_windows_integration_tests.moc"

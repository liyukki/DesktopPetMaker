#ifndef CREDENTIALSTORE_H
#define CREDENTIALSTORE_H

#include <QString>

class CredentialStore
{
public:
    static CredentialStore &instance();

    bool writeSecret(const QString &credentialId, const QString &secret);
    QString readSecret(const QString &credentialId) const;
    bool deleteSecret(const QString &credentialId);

private:
    CredentialStore() = default;
    QString keyFor(const QString &credentialId) const;
};

#endif // CREDENTIALSTORE_H

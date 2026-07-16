#include "credentialstore.h"

#include <QByteArray>
#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#endif

namespace {
QString targetForCredential(const QString &credentialId)
{
    return QStringLiteral("AI Desktop Pet/%1").arg(credentialId);
}
}

CredentialStore &CredentialStore::instance()
{
    static CredentialStore store;
    return store;
}

QString CredentialStore::keyFor(const QString &credentialId) const
{
    return QStringLiteral("credentials/%1").arg(credentialId);
}

bool CredentialStore::writeSecret(const QString &credentialId, const QString &secret)
{
    if (credentialId.trimmed().isEmpty()) {
        return false;
    }
#ifdef Q_OS_WIN
    const QByteArray data = secret.toUtf8();
    const std::wstring target = targetForCredential(credentialId).toStdWString();
    CREDENTIALW credential;
    ZeroMemory(&credential, sizeof(credential));
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(target.c_str());
    credential.CredentialBlobSize = static_cast<DWORD>(data.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(data.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (!CredWriteW(&credential, 0)) {
        return false;
    }
    QSettings().remove(keyFor(credentialId));
    return true;
#else
    QSettings().setValue(keyFor(credentialId), secret);
    return true;
#endif
}

QString CredentialStore::readSecret(const QString &credentialId) const
{
#ifdef Q_OS_WIN
    const std::wstring target = targetForCredential(credentialId).toStdWString();
    PCREDENTIALW credential = nullptr;
    if (CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential) && credential) {
        const QByteArray data(reinterpret_cast<const char *>(credential->CredentialBlob),
                              static_cast<int>(credential->CredentialBlobSize));
        const QString secret = QString::fromUtf8(data);
        CredFree(credential);
        if (!secret.isEmpty()) {
            return secret;
        }
    }
    return QString();
#else
    QSettings settings;
    const QString secret = settings.value(keyFor(credentialId)).toString();
    return secret;
#endif
}

bool CredentialStore::deleteSecret(const QString &credentialId)
{
    if (credentialId.trimmed().isEmpty()) {
        return false;
    }
    bool ok = true;
#ifdef Q_OS_WIN
    const std::wstring target = targetForCredential(credentialId).toStdWString();
    if (!CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0)) {
        const DWORD error = GetLastError();
        ok = (error == ERROR_NOT_FOUND || error == ERROR_FILE_NOT_FOUND);
    }
#endif
    QSettings().remove(keyFor(credentialId));
    return ok;
}

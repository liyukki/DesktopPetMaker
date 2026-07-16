#include "archivecommandrunner.h"

#include <QProcess>
#include <QStandardPaths>

bool PowerShellArchiveCommandRunner::run(const QString &command, QString *errorMessage)
{
    QString shell = QStandardPaths::findExecutable(QStringLiteral("pwsh.exe"));
    const bool usingPwsh = !shell.isEmpty();
    if (shell.isEmpty()) shell = QStandardPaths::findExecutable(QStringLiteral("powershell.exe"));
    if (shell.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("No PowerShell executable found for petpack operation.");
        return false;
    }

    QString finalCommand = command;
    if (!usingPwsh) {
        finalCommand = QStringLiteral("[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false); "
                                      "$OutputEncoding=[System.Text.UTF8Encoding]::new($false); ") + command;
    }

    QProcess versionProcess;
    QStringList versionArgs {QStringLiteral("-NoLogo"), QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive")};
    if (!usingPwsh) versionArgs << QStringLiteral("-ExecutionPolicy") << QStringLiteral("Bypass");
    versionArgs << QStringLiteral("-Command") << QStringLiteral("$PSVersionTable.PSVersion.ToString()");
    versionProcess.start(shell, versionArgs);
    versionProcess.waitForFinished(5000);
    const QString shellVersion = QString::fromUtf8(versionProcess.readAllStandardOutput()).trimmed();

    QProcess process;
    QStringList args {QStringLiteral("-NoLogo"), QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive")};
    if (!usingPwsh) args << QStringLiteral("-ExecutionPolicy") << QStringLiteral("Bypass");
    args << QStringLiteral("-Command") << finalCommand;
    process.start(shell, args);
    if (!process.waitForStarted()) {
        if (errorMessage) *errorMessage = QStringLiteral("Failed to start PowerShell for petpack operation. Shell: %1").arg(shell);
        return false;
    }

    constexpr int kArchiveTimeoutMs = 60000;
    if (!process.waitForFinished(kArchiveTimeoutMs)) {
        process.terminate();
        if (!process.waitForFinished(3000)) {
            process.kill();
            process.waitForFinished(3000);
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("PowerShell archive command timed out after %1 ms. Shell: %2 Version: %3")
                .arg(kArchiveTimeoutMs).arg(shell, shellVersion);
        }
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
        const QString stdoutText = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (errorMessage) {
            const QString detail = stderrText.isEmpty() ? stdoutText : stderrText;
            *errorMessage = QStringLiteral("PowerShell archive command failed. Shell: %1 Version: %2 ExitCode: %3 Error: %4")
                .arg(shell, shellVersion).arg(process.exitCode()).arg(detail);
        }
        return false;
    }
    return true;
}

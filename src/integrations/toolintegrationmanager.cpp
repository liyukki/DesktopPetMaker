#include "toolintegrationmanager.h"

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>

bool ToolIntegrationManager::isToolAvailable(const QString &id) const
{
    return QFileInfo::exists(QSettings().value(QStringLiteral("tools/%1/path").arg(id)).toString());
}

bool ToolIntegrationManager::launchTool(const QString &id, const QStringList &args) const
{
    const QString path = QSettings().value(QStringLiteral("tools/%1/path").arg(id)).toString();
    if (!QFileInfo::exists(path)) {
        return false;
    }
    return QProcess::startDetached(path, args);
}

bool ToolIntegrationManager::launchTool(const QString &id, const QHash<QString, QString> &templateValues) const
{
    QSettings settings;
    const QString templateText = settings.value(QStringLiteral("tools/%1/argumentsTemplate").arg(id)).toString();
    return launchTool(id, expandArgumentsTemplate(templateText, templateValues));
}

void ToolIntegrationManager::setTool(const QString &id, const QString &displayName, const QString &executablePath, const QString &argumentsTemplate)
{
    QSettings settings;
    settings.setValue(QStringLiteral("tools/%1/displayName").arg(id), displayName);
    settings.setValue(QStringLiteral("tools/%1/path").arg(id), executablePath);
    settings.setValue(QStringLiteral("tools/%1/argumentsTemplate").arg(id), argumentsTemplate);
}

QStringList ToolIntegrationManager::expandArgumentsTemplate(const QString &argumentsTemplate,
                                                            const QHash<QString, QString> &templateValues)
{
    QStringList result;
    QString current;
    bool inQuote = false;
    QChar quoteChar;

    auto flush = [&]() {
        if (!current.isEmpty()) {
            result.append(current);
            current.clear();
        }
    };

    for (int i = 0; i < argumentsTemplate.size(); ++i) {
        const QChar ch = argumentsTemplate.at(i);
        if ((ch == '"' || ch == '\'') && (!inQuote || ch == quoteChar)) {
            if (inQuote) {
                inQuote = false;
                quoteChar = QChar();
            } else {
                inQuote = true;
                quoteChar = ch;
            }
            continue;
        }
        if (!inQuote && ch.isSpace()) {
            flush();
            continue;
        }
        current.append(ch);
    }
    flush();

    const QRegularExpression placeholder(QStringLiteral("\\{([A-Za-z0-9_]+)\\}"));
    for (QString &arg : result) {
        QRegularExpressionMatchIterator it = placeholder.globalMatch(arg);
        QString expanded;
        int last = 0;
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            expanded += arg.mid(last, match.capturedStart() - last);
            expanded += templateValues.value(match.captured(1));
            last = match.capturedEnd();
        }
        expanded += arg.mid(last);
        arg = expanded;
    }
    return result;
}

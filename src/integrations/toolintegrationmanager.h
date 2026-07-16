#ifndef TOOLINTEGRATIONMANAGER_H
#define TOOLINTEGRATIONMANAGER_H

#include <QHash>
#include <QString>
#include <QStringList>

class ToolIntegrationManager
{
public:
    bool isToolAvailable(const QString &id) const;
    bool launchTool(const QString &id, const QStringList &args = {}) const;
    bool launchTool(const QString &id, const QHash<QString, QString> &templateValues) const;
    void setTool(const QString &id, const QString &displayName, const QString &executablePath, const QString &argumentsTemplate = QString());
    static QStringList expandArgumentsTemplate(const QString &argumentsTemplate,
                                               const QHash<QString, QString> &templateValues);
};

#endif // TOOLINTEGRATIONMANAGER_H

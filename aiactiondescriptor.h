#ifndef AIACTIONDESCRIPTOR_H
#define AIACTIONDESCRIPTOR_H

#include <QJsonObject>
#include <QString>
#include <QStringList>

// Provider-facing action data. Runtime validation remains authoritative.
struct AIActionDescriptor
{
    QString id;
    QString displayName;
    QString description;
    QString category;
    bool allowAiTrigger {false};
    QStringList allowedStates;
    QJsonObject parameterSchema;
};

#endif // AIACTIONDESCRIPTOR_H

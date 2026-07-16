#ifndef AIACTIONVALIDATOR_H
#define AIACTIONVALIDATOR_H

#include <QJsonObject>
#include <QString>

#include "petproject.h"

struct AiActionValidationResult
{
    bool ok {false};
    QString errorCode;
    QString message;
    QJsonObject parameters;
};

AiActionValidationResult validateAiActionRequest(const PetProject &project,
                                                 const QString &actionId,
                                                 const QJsonObject &parameters,
                                                 const QString &runtimeState);

#endif // AIACTIONVALIDATOR_H

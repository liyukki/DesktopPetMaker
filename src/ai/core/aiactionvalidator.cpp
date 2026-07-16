#include "aiactionvalidator.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QImageReader>

namespace {
bool matchesType(const QJsonValue &value, const QString &type)
{
    if (type == QStringLiteral("string")) return value.isString();
    if (type == QStringLiteral("boolean")) return value.isBool();
    if (type == QStringLiteral("number")) return value.isDouble();
    if (type == QStringLiteral("integer")) return value.isDouble() && value.toDouble() == value.toInt();
    if (type == QStringLiteral("object")) return value.isObject();
    if (type == QStringLiteral("array")) return value.isArray();
    if (type == QStringLiteral("null")) return value.isNull();
    return false;
}

AiActionValidationResult failure(const QString &code, const QString &message)
{
    return {false, code, message, {}};
}
}

AiActionValidationResult validateAiActionRequest(const PetProject &project,
                                                 const QString &actionId,
                                                 const QJsonObject &parameters,
                                                 const QString &runtimeState)
{
    const auto actionIt = project.actions.constFind(actionId);
    if (actionId.trimmed().isEmpty() || actionIt == project.actions.constEnd()) {
        return failure(QStringLiteral("ActionNotFound"), QStringLiteral("The requested action does not exist for this pet."));
    }
    const PetAction &action = actionIt.value();
    if (!action.allowAiTrigger) {
        return failure(QStringLiteral("ActionNotAllowed"), QStringLiteral("The requested action is not enabled for AI use."));
    }
    if (action.frames.isEmpty()) {
        return failure(QStringLiteral("ActionHasNoFrames"), QStringLiteral("The requested action has no valid runtime frames."));
    }
    bool hasReadableFrame = false;
    for (const PetFrame &frame : action.frames) {
        QImageReader reader(project.absolutePathFor(frame.path));
        if (!reader.canRead()) {
            return failure(QStringLiteral("ActionFrameUnreadable"),
                           QStringLiteral("The requested action contains an unreadable frame: %1").arg(frame.path));
        }
        hasReadableFrame = true;
    }
    if (!hasReadableFrame) {
        return failure(QStringLiteral("ActionHasNoReadableFrames"), QStringLiteral("The requested action has no readable runtime frame."));
    }
    if (runtimeState != QStringLiteral("Normal")) {
        return failure(QStringLiteral("RuntimeStateUnsupported"),
                       QStringLiteral("AI actions currently execute only in the Normal runtime state."));
    }
    const QStringList allowedStates = action.aiAllowedStates.isEmpty()
        ? QStringList {QStringLiteral("Normal")}
        : action.aiAllowedStates;
    if (!allowedStates.contains(runtimeState)) {
        return failure(QStringLiteral("ActionStateRejected"),
                       QStringLiteral("The requested action is not allowed while the pet is in state %1.").arg(runtimeState));
    }

    const QJsonObject schema = action.aiParameterSchema;
    if (schema.isEmpty()) {
        return parameters.isEmpty()
            ? AiActionValidationResult {true, {}, {}, parameters}
            : failure(QStringLiteral("UnexpectedActionParameters"), QStringLiteral("This action does not accept parameters."));
    }
    if (schema.value(QStringLiteral("type")).toString(QStringLiteral("object")) != QStringLiteral("object")) {
        return failure(QStringLiteral("InvalidActionSchema"), QStringLiteral("The action parameter schema root must be an object."));
    }
    const QJsonObject properties = schema.value(QStringLiteral("properties")).toObject();
    const QHash<QString, QString> runtimeParameterTypes {
        {QStringLiteral("playbackSpeed"), QStringLiteral("number")},
        {QStringLiteral("mirror"), QStringLiteral("boolean")},
        {QStringLiteral("offsetX"), QStringLiteral("integer")},
        {QStringLiteral("offsetY"), QStringLiteral("integer")},
        {QStringLiteral("scale"), QStringLiteral("number")}
    };
    for (auto schemaIt = properties.constBegin(); schemaIt != properties.constEnd(); ++schemaIt) {
        const QString expectedType = runtimeParameterTypes.value(schemaIt.key());
        const QString configuredType = schemaIt.value().toObject().value(QStringLiteral("type")).toString();
        if (expectedType.isEmpty() || configuredType != expectedType) {
            return failure(QStringLiteral("UnsupportedActionParameterSchema"),
                           QStringLiteral("Action parameter %1 has no matching safe runtime effect.").arg(schemaIt.key()));
        }
    }
    if (schema.value(QStringLiteral("additionalProperties")).toBool(false)) {
        return failure(QStringLiteral("UnsupportedAdditionalProperties"),
                       QStringLiteral("Runtime action schemas cannot enable additionalProperties."));
    }
    const QJsonArray required = schema.value(QStringLiteral("required")).toArray();
    for (const QJsonValue &requiredValue : required) {
        const QString key = requiredValue.toString();
        if (!key.isEmpty() && !parameters.contains(key)) {
            return failure(QStringLiteral("MissingActionParameter"),
                           QStringLiteral("Required action parameter is missing: %1").arg(key));
        }
    }
    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        if (!properties.contains(it.key())) {
            return failure(QStringLiteral("UnknownActionParameter"),
                           QStringLiteral("Unknown action parameter: %1").arg(it.key()));
        }
        const QJsonObject propertySchema = properties.value(it.key()).toObject();
        const QString type = propertySchema.value(QStringLiteral("type")).toString();
        if (type.isEmpty() || !matchesType(it.value(), type)) {
            return failure(QStringLiteral("InvalidActionParameterType"),
                           QStringLiteral("Action parameter %1 has an invalid type.").arg(it.key()));
        }
        const QJsonArray enumValues = propertySchema.value(QStringLiteral("enum")).toArray();
        if (!enumValues.isEmpty() && !enumValues.contains(it.value())) {
            return failure(QStringLiteral("InvalidActionParameterEnum"),
                           QStringLiteral("Action parameter %1 is outside the allowed values.").arg(it.key()));
        }
        if (it.value().isDouble()) {
            const double number = it.value().toDouble();
            if (propertySchema.contains(QStringLiteral("minimum"))
                && number < propertySchema.value(QStringLiteral("minimum")).toDouble()) {
                return failure(QStringLiteral("ActionParameterBelowMinimum"),
                               QStringLiteral("Action parameter %1 is below its minimum.").arg(it.key()));
            }
            if (propertySchema.contains(QStringLiteral("maximum"))
                && number > propertySchema.value(QStringLiteral("maximum")).toDouble()) {
                return failure(QStringLiteral("ActionParameterAboveMaximum"),
                               QStringLiteral("Action parameter %1 is above its maximum.").arg(it.key()));
            }
        }
    }
    const double playbackSpeed = parameters.value(QStringLiteral("playbackSpeed")).toDouble(1.0);
    if (playbackSpeed < 0.25 || playbackSpeed > 4.0) {
        return failure(QStringLiteral("ActionPlaybackSpeedOutOfRange"),
                       QStringLiteral("playbackSpeed must be between 0.25 and 4.0."));
    }
    const double scale = parameters.value(QStringLiteral("scale")).toDouble(1.0);
    if (scale < 0.5 || scale > 1.5) {
        return failure(QStringLiteral("ActionScaleOutOfRange"),
                       QStringLiteral("scale must be between 0.5 and 1.5."));
    }
    const int maxOffsetX = qMax(1, project.canvasSize.width());
    const int maxOffsetY = qMax(1, project.canvasSize.height());
    if (qAbs(parameters.value(QStringLiteral("offsetX")).toInt()) > maxOffsetX
        || qAbs(parameters.value(QStringLiteral("offsetY")).toInt()) > maxOffsetY) {
        return failure(QStringLiteral("ActionOffsetOutOfRange"),
                       QStringLiteral("Action offsets must stay within the pet canvas."));
    }
    if (parameters.value(QStringLiteral("mirror")).toBool(false) && !action.mirrorSupported) {
        return failure(QStringLiteral("ActionMirrorUnsupported"),
                       QStringLiteral("The requested action does not support mirroring."));
    }
    return {true, {}, {}, parameters};
}

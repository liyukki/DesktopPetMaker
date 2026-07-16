#ifndef PETBEHAVIORRULE_H
#define PETBEHAVIORRULE_H

#include <QString>
#include <QtGlobal>

enum class BehaviorTriggerType
{
    RandomIdle,
    ManualOnly,
    ProactiveChatFollowUp
};

struct PetBehaviorRule
{
    QString id;
    QString displayName;
    BehaviorTriggerType triggerType {BehaviorTriggerType::RandomIdle};
    QString actionId;
    double weight {1.0};
    int cooldownMs {0};
    bool requireOnGround {true};
    bool requireIdle {true};
    bool disabledWhileAiBusy {true};
};

inline bool randomIdleRuleMatchesConditions(const PetBehaviorRule &rule,
                                            bool actionHasFrames,
                                            bool onGround,
                                            bool idle,
                                            bool aiBusy,
                                            qint64 elapsedMs,
                                            qint64 lastTriggeredMs)
{
    if (rule.triggerType != BehaviorTriggerType::RandomIdle || rule.actionId.trimmed().isEmpty()) {
        return false;
    }
    if (!actionHasFrames || rule.weight <= 0.0) {
        return false;
    }
    if (rule.requireOnGround && !onGround) {
        return false;
    }
    if (rule.requireIdle && !idle) {
        return false;
    }
    if (rule.disabledWhileAiBusy && aiBusy) {
        return false;
    }
    if (lastTriggeredMs >= 0 && rule.cooldownMs > 0 && elapsedMs - lastTriggeredMs < rule.cooldownMs) {
        return false;
    }
    return true;
}

#endif // PETBEHAVIORRULE_H

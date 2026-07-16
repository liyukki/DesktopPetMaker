#ifndef PETCONVERSATIONCONTEXT_H
#define PETCONVERSATIONCONTEXT_H

#include <QString>
#include <QStringList>
#include <QVector>

#include "aiactiondescriptor.h"
#include "chathistoryentry.h"

class PetAiRequestLease;

struct PetConversationContext
{
    QVector<ChatHistoryEntry> history;
    int activeRequestCount {0};
    QString characterName;
    QString systemPrompt;
    QString providerProfileId;
    QString effectiveProjectId;
    QString petProjectPath;
    QStringList allowedActionIds;
    QVector<AIActionDescriptor> allowedActionDescriptors;

    bool busy() const { return activeRequestCount > 0; }

    void beginRequest()
    {
        ++activeRequestCount;
    }

    void endRequest()
    {
        if (activeRequestCount > 0) {
            --activeRequestCount;
        }
    }

    void updateAiProfile(const QString &name,
                         const QString &prompt,
                         const QString &profileId,
                         const QStringList &actions = {},
                         const QVector<AIActionDescriptor> &actionDescriptors = {})
    {
        characterName = name;
        systemPrompt = prompt;
        providerProfileId = profileId;
        allowedActionIds = actions;
        allowedActionDescriptors = actionDescriptors;
    }

    void clearHistory()
    {
        history.clear();
    }
};

#endif // PETCONVERSATIONCONTEXT_H

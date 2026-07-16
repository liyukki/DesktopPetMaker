#ifndef CHATHISTORYENTRY_H
#define CHATHISTORYENTRY_H

#include <QString>

enum class ChatHistoryRole
{
    User,
    Assistant,
    System
};

struct ChatHistoryEntry
{
    ChatHistoryRole role {ChatHistoryRole::User};
    QString content;
};

#endif // CHATHISTORYENTRY_H

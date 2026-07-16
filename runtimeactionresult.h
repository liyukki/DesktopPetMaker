#ifndef RUNTIMEACTIONRESULT_H
#define RUNTIMEACTIONRESULT_H

#include <QMetaType>
#include <QString>

enum class RuntimeActionDispatchStatus
{
    NotRequested,
    Executed,
    Queued,
    Rejected,
    Failed
};

struct RuntimeActionDispatchResult
{
    RuntimeActionDispatchStatus status {RuntimeActionDispatchStatus::NotRequested};
    QString errorCode;
    QString message;
};

struct RuntimeReplyDeliveryResult
{
    bool textDelivered {false};
    RuntimeActionDispatchResult action;
    QString errorCode;
    QString message;
};

Q_DECLARE_METATYPE(RuntimeActionDispatchStatus)
Q_DECLARE_METATYPE(RuntimeActionDispatchResult)
Q_DECLARE_METATYPE(RuntimeReplyDeliveryResult)

#endif // RUNTIMEACTIONRESULT_H

#ifndef AICONVERSATIONROOMREPOSITORY_H
#define AICONVERSATIONROOMREPOSITORY_H

#include <QString>
#include <QVector>

#include "aiconversationroom.h"

enum class RoomRepositoryLoadStatus
{
    Ok,
    MissingFile,
    CorruptJson,
    UnsupportedFutureVersion,
    MigrationRequired,
    PermissionDenied,
    InvalidData
};

class AIConversationRoomRepository
{
public:
    explicit AIConversationRoomRepository(QString storagePath = {});

    QString storagePath() const;
    bool load(QVector<AIConversationRoom> *rooms,
              QString *errorMessage = nullptr,
              RoomRepositoryLoadStatus *status = nullptr) const;
    bool save(const QVector<AIConversationRoom> &rooms, QString *errorMessage = nullptr) const;
    bool backupCorruptStore(QString *backupPath = nullptr, QString *errorMessage = nullptr) const;

private:
    QString m_storagePath;
};

#endif // AICONVERSATIONROOMREPOSITORY_H

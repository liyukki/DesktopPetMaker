#ifndef ARCHIVECOMMANDRUNNER_H
#define ARCHIVECOMMANDRUNNER_H

#include <QString>

class IArchiveCommandRunner
{
public:
    virtual ~IArchiveCommandRunner() = default;
    virtual bool run(const QString &command, QString *errorMessage) = 0;
};

class PowerShellArchiveCommandRunner final : public IArchiveCommandRunner
{
public:
    bool run(const QString &command, QString *errorMessage) override;
};

#endif // ARCHIVECOMMANDRUNNER_H

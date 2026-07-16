#ifndef MOTIONPROMPTLIBRARY_H
#define MOTIONPROMPTLIBRARY_H

#include <QString>
#include <QStringList>

struct MotionPromptSpec
{
    QString id;
    QString displayName;
    QString englishName;
    QString category;
    QString description;
    QString storyboard;
    QString prompt;
    QString negativePrompt;
    int frameCount {8};
    int fps {8};
    bool loop {false};
    int rows {2};
    int columns {4};
    bool allowHorizontalDisplacement {false};
    bool mirrorSupported {false};
};

class MotionPromptLibrary
{
public:
    static QStringList templates();
    static MotionPromptSpec specForAction(const QString &actionName);
    static QString promptForAction(const QString &actionName, int frameCount, int rows, int columns, const QString &steps = QString());
    static QString negativePromptForAction(const QString &actionName);
};

#endif // MOTIONPROMPTLIBRARY_H

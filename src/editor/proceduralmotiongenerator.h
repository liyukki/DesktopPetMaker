#ifndef PROCEDURALMOTIONGENERATOR_H
#define PROCEDURALMOTIONGENERATOR_H

#include <QString>
#include <QStringList>

struct ProceduralPose
{
    qreal offsetX {0};
    qreal offsetY {0};
    qreal rotationDegrees {0};
    qreal scaleX {1.0};
    qreal scaleY {1.0};
};

class ProceduralMotionGenerator
{
public:
    static QVector<ProceduralPose> posesForPreset(const QString &preset, const QString &style, int frameCount);
    static QStringList generateFrames(const QString &sourceImage,
                                      const QString &targetDir,
                                      const QString &preset,
                                      const QString &style,
                                      int frameCount,
                                      QString *errorMessage = nullptr);
};

#endif // PROCEDURALMOTIONGENERATOR_H

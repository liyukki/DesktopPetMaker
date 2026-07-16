#ifndef SCREENPLACEMENTUTIL_H
#define SCREENPLACEMENTUTIL_H

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVector>

class ScreenPlacementUtil
{
public:
    static QPoint clampTopLeft(const QPoint &desiredTopLeft,
                               const QSize &windowSize,
                               const QVector<QRect> &availableScreens,
                               int primaryScreenIndex = 0);
};

#endif // SCREENPLACEMENTUTIL_H

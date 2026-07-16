#include "screenplacementutil.h"

#include <QtGlobal>

QPoint ScreenPlacementUtil::clampTopLeft(const QPoint &desiredTopLeft,
                                         const QSize &windowSize,
                                         const QVector<QRect> &availableScreens,
                                         int primaryScreenIndex)
{
    if (availableScreens.isEmpty() || windowSize.isEmpty()) {
        return desiredTopLeft;
    }

    const QRect desiredRect(desiredTopLeft, windowSize);
    int targetIndex = -1;
    qint64 largestIntersection = 0;
    for (int index = 0; index < availableScreens.size(); ++index) {
        const QRect &screen = availableScreens.at(index);
        if (screen.contains(desiredRect.center())) {
            targetIndex = index;
            break;
        }
        const QRect intersection = desiredRect.intersected(screen);
        const qint64 area = qint64(intersection.width()) * intersection.height();
        if (area > largestIntersection) {
            largestIntersection = area;
            targetIndex = index;
        }
    }
    if (targetIndex < 0) {
        targetIndex = qBound(0, primaryScreenIndex, availableScreens.size() - 1);
    }

    const QRect target = availableScreens.at(targetIndex);
    const int maxX = windowSize.width() <= target.width()
        ? target.right() - windowSize.width() + 1
        : target.left();
    const int maxY = windowSize.height() <= target.height()
        ? target.bottom() - windowSize.height() + 1
        : target.top();
    return {
        qBound(target.left(), desiredTopLeft.x(), maxX),
        qBound(target.top(), desiredTopLeft.y(), maxY)
    };
}

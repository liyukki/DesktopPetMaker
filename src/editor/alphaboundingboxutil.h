#ifndef ALPHABOUNDINGBOXUTIL_H
#define ALPHABOUNDINGBOXUTIL_H

#include <QImage>
#include <QRect>

class AlphaBoundingBoxUtil
{
public:
    static QRect opaqueBounds(const QImage &image, int alphaThreshold = 1);
};

#endif // ALPHABOUNDINGBOXUTIL_H

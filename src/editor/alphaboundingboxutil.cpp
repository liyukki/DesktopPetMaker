#include "alphaboundingboxutil.h"

QRect AlphaBoundingBoxUtil::opaqueBounds(const QImage &image, int alphaThreshold)
{
    QRect result;
    if (image.isNull()) {
        return result;
    }
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < argb.height(); ++y) {
        for (int x = 0; x < argb.width(); ++x) {
            if (qAlpha(argb.pixel(x, y)) > alphaThreshold) {
                const QRect pixel(x, y, 1, 1);
                result = result.isNull() ? pixel : result.united(pixel);
            }
        }
    }
    return result;
}

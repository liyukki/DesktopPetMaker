#include "assetqualityanalyzer.h"

#include "alphaboundingboxutil.h"

#include <QImage>
#include <QtMath>

AssetQualityReport AssetQualityAnalyzer::analyzeFrames(const QStringList &framePaths)
{
    AssetQualityReport report;
    QSize canvas;
    QList<QRect> bounds;
    for (const QString &path : framePaths) {
        QImage image(path);
        if (image.isNull()) {
            report.allFramesReadable = false;
            report.warnings.append(QStringLiteral("Unreadable frame: %1").arg(path));
            continue;
        }
        if (!canvas.isValid()) {
            canvas = image.size();
        } else if (canvas != image.size()) {
            report.canvasSizeConsistent = false;
        }
        const QRect rect = AlphaBoundingBoxUtil::opaqueBounds(image);
        if (rect.isNull()) {
            report.warnings.append(QStringLiteral("Transparent frame: %1").arg(path));
        }
        bounds.append(rect);
    }
    if (bounds.size() < 2) {
        return report;
    }
    QRect base;
    for (const QRect &rect : bounds) {
        if (!rect.isNull()) {
            base = rect;
            break;
        }
    }
    if (base.isNull()) {
        report.warnings.append(QStringLiteral("All frames are fully transparent; quality metrics cannot be computed."));
        return report;
    }
    for (const QRect &rect : bounds) {
        if (rect.isNull()) {
            continue;
        }
        if (base.width() > 0) {
            report.maxWidthVariationPercent = qMax(report.maxWidthVariationPercent,
                qAbs(rect.width() - base.width()) * 100.0 / base.width());
        }
        if (base.height() > 0) {
            report.maxHeightVariationPercent = qMax(report.maxHeightVariationPercent,
                qAbs(rect.height() - base.height()) * 100.0 / base.height());
        }
        report.maxCenterXDrift = qMax(report.maxCenterXDrift, qAbs(rect.center().x() - base.center().x()));
        report.maxBottomYDrift = qMax(report.maxBottomYDrift, qAbs(rect.bottom() - base.bottom()));
    }
    return report;
}

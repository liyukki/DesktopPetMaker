#include "proceduralmotiongenerator.h"

#include <QDir>
#include <QImage>
#include <QPainter>
#include <QRect>
#include <QtMath>

namespace {
QRect alphaBounds(const QImage &image)
{
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(line[x]) > 0) {
                const QRect pixel(x, y, 1, 1);
                bounds = bounds.isNull() ? pixel : bounds.united(pixel);
            }
        }
    }
    return bounds;
}
}

QVector<ProceduralPose> ProceduralMotionGenerator::posesForPreset(const QString &preset, const QString &style, int frameCount)
{
    const qreal amp = style == QStringLiteral("large") ? 1.8 : (style == QStringLiteral("active") ? 1.3 : 0.8);
    QVector<ProceduralPose> poses;
    for (int i = 0; i < qMax(1, frameCount); ++i) {
        const qreal t = frameCount <= 1 ? 0 : (static_cast<qreal>(i) / (frameCount - 1));
        const qreal wave = qSin(t * M_PI * 2.0);
        ProceduralPose pose;
        if (preset == QStringLiteral("hop_left") || preset == QStringLiteral("hop_right") || preset.contains(QStringLiteral("hop"), Qt::CaseInsensitive)) {
            const qreal phase = qSin(t * M_PI * 4.0);
            const qreal hop = qAbs(phase);
            const qreal direction = preset == QStringLiteral("hop_right") ? 1.0 : -1.0;
            pose.offsetY = -hop * 4.0 * amp;
            pose.offsetX = direction * phase * 2.0 * amp;
            pose.rotationDegrees = phase * 2.0 * amp;
        } else if (preset == QStringLiteral("nod") || preset.contains(QStringLiteral("nod"), Qt::CaseInsensitive)) {
            pose.rotationDegrees = qAbs(wave) * 5.0 * amp;
            pose.offsetY = qAbs(wave) * 2.0 * amp;
        } else if (preset == QStringLiteral("shake_head") || preset.contains(QStringLiteral("shake"), Qt::CaseInsensitive)) {
            pose.rotationDegrees = wave * 6.0 * amp;
        } else if (preset == QStringLiteral("rhythm_step")) {
            pose.offsetY = -qAbs(qSin(t * M_PI * 4.0)) * 3.0 * amp;
            pose.rotationDegrees = qSin(t * M_PI * 4.0) * 1.5 * amp;
            pose.scaleY = 1.0 - qAbs(qSin(t * M_PI * 4.0)) * 0.01 * amp;
        } else if (preset == QStringLiteral("peek")) {
            const qreal lean = qSin(t * M_PI);
            pose.offsetX = lean * 2.0 * amp;
            pose.rotationDegrees = -lean * 5.0 * amp;
        } else if (preset == QStringLiteral("look_far")) {
            const qreal lift = qSin(t * M_PI);
            pose.offsetY = -lift * 2.0 * amp;
            pose.rotationDegrees = -lift * 3.0 * amp;
        } else if (preset == QStringLiteral("adjust_clothes")) {
            const qreal bow = qSin(t * M_PI);
            pose.offsetY = bow * 2.0 * amp;
            pose.rotationDegrees = bow * 2.0 * amp;
        } else if (preset == QStringLiteral("hands_behind_sway")) {
            pose.rotationDegrees = wave * 3.0 * amp;
            pose.offsetY = -qAbs(wave) * 1.0 * amp;
        } else if (preset == QStringLiteral("listen")) {
            const qreal lean = qSin(t * M_PI);
            pose.rotationDegrees = -lean * 4.0 * amp;
            pose.offsetX = lean * 1.0 * amp;
        } else if (preset == QStringLiteral("sleep_breathe") || preset.contains(QStringLiteral("sleep"), Qt::CaseInsensitive)) {
            pose.scaleY = 1.0 + qAbs(wave) * 0.02 * amp;
            pose.offsetY = qAbs(wave) * 1.5 * amp;
        } else {
            pose.scaleY = 1.0 + qAbs(wave) * 0.025 * amp;
            pose.offsetY = -qAbs(wave) * 2.0 * amp;
        }
        poses.append(pose);
    }
    return poses;
}


QStringList ProceduralMotionGenerator::generateFrames(const QString &sourceImage,
                                                      const QString &targetDir,
                                                      const QString &preset,
                                                      const QString &style,
                                                      int frameCount,
                                                      QString *errorMessage)
{
    QImage source(sourceImage);
    if (source.isNull()) {
        if (errorMessage) *errorMessage = QStringLiteral("Source image cannot be read: %1").arg(sourceImage);
        return {};
    }
    QDir().mkpath(targetDir);
    const QVector<ProceduralPose> poses = posesForPreset(preset, style, frameCount);
    const QRect opaque = alphaBounds(source);
    const QPointF pivot = opaque.isNull()
        ? QPointF(source.width() / 2.0, source.height())
        : QPointF(opaque.center().x(), opaque.bottom());
    QStringList files;
    for (int i = 0; i < poses.size(); ++i) {
        QImage canvas(source.size(), QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.translate(pivot.x() + poses[i].offsetX, pivot.y() + poses[i].offsetY);
        painter.rotate(poses[i].rotationDegrees);
        painter.scale(poses[i].scaleX, poses[i].scaleY);
        painter.drawImage(QPointF(-pivot.x(), -pivot.y()), source);
        painter.end();
        const QString path = QDir(targetDir).filePath(QStringLiteral("%1.png").arg(i + 1, 4, 10, QChar('0')));
        if (!canvas.save(path, "PNG")) {
            if (errorMessage) *errorMessage = QStringLiteral("Failed to save generated frame: %1").arg(path);
            return {};
        }
        files.append(path);
    }
    return files;
}

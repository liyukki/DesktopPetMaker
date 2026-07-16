#include "spritesheetslicer.h"

#include <QDir>
#include <QRect>

namespace {
bool isFullyTransparent(const QImage &image)
{
    if (!image.hasAlphaChannel()) return false;
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < argb.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(argb.constScanLine(y));
        for (int x = 0; x < argb.width(); ++x) {
            if (qAlpha(line[x]) != 0) return false;
        }
    }
    return true;
}
}

SpriteSheetSliceResult SpriteSheetSlicer::sliceFrameResult(const QImage &sheet,
                                                           const SpriteSheetSliceOptions &options,
                                                           QString *errorMessage)
{
    if (sheet.isNull()) {
        if (errorMessage) *errorMessage = QStringLiteral("Sprite sheet cannot be read.");
        return {};
    }
    if (options.rows <= 0 || options.columns <= 0) {
        if (errorMessage) *errorMessage = QStringLiteral("Rows and columns must be greater than zero.");
        return {};
    }
    if (options.maxFrames < 0) {
        if (errorMessage) *errorMessage = QStringLiteral("End frame cannot be before start frame.");
        return {};
    }
    if (options.marginLeft < 0 || options.marginTop < 0 || options.marginRight < 0
        || options.marginBottom < 0 || options.spacingX < 0 || options.spacingY < 0) {
        if (errorMessage) *errorMessage = QStringLiteral("Margins and spacing cannot be negative.");
        return {};
    }
    const int usableWidth = sheet.width() - options.marginLeft - options.marginRight
                            - options.spacingX * (options.columns - 1);
    const int usableHeight = sheet.height() - options.marginTop - options.marginBottom
                             - options.spacingY * (options.rows - 1);
    if (usableWidth <= 0 || usableHeight <= 0) {
        if (errorMessage) *errorMessage = QStringLiteral("Margins and spacing leave no usable image area.");
        return {};
    }
    if ((usableWidth % options.columns != 0 || usableHeight % options.rows != 0)
        && (options.frameWidth <= 0 || options.frameHeight <= 0)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Sprite sheet does not divide evenly into %1 x %2 cells.")
                                .arg(options.columns).arg(options.rows);
        }
        return {};
    }
    const int frameWidth = options.frameWidth > 0 ? options.frameWidth : usableWidth / options.columns;
    const int frameHeight = options.frameHeight > 0 ? options.frameHeight : usableHeight / options.rows;
    if (frameWidth <= 0 || frameHeight <= 0) {
        if (errorMessage) *errorMessage = QStringLiteral("Computed frame size is invalid.");
        return {};
    }

    QVector<QImage> allFrames;
    const QRect bounds(0, 0, sheet.width(), sheet.height());
    const auto appendCell = [&](int row, int column) -> bool {
        const QRect cropRect(options.marginLeft + column * (frameWidth + options.spacingX),
                             options.marginTop + row * (frameHeight + options.spacingY),
                             frameWidth,
                             frameHeight);
        if (!bounds.contains(cropRect)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Crop rectangle out of bounds at row %1 column %2.")
                                    .arg(row + 1).arg(column + 1);
            }
            return false;
        }
        allFrames.append(sheet.copy(cropRect));
        return true;
    };
    if (options.readingOrder == SpriteSheetReadingOrder::LeftToRightTopToBottom) {
        for (int row = 0; row < options.rows; ++row)
            for (int column = 0; column < options.columns; ++column)
                if (!appendCell(row, column)) return {};
    } else {
        for (int column = 0; column < options.columns; ++column)
            for (int row = 0; row < options.rows; ++row)
                if (!appendCell(row, column)) return {};
    }

    const int start = qBound(0, options.startFrame, allFrames.size());
    const int available = allFrames.size() - start;
    const int count = options.maxFrames > 0 ? qMin(options.maxFrames, available) : available;
    SpriteSheetSliceResult result;
    result.frames.reserve(count);
    result.sourceCellIndices.reserve(count);
    for (int index = start; index < start + count; ++index) {
        if (!options.skipTransparentFrames || !isFullyTransparent(allFrames.at(index))) {
            result.frames.append(allFrames.at(index));
            result.sourceCellIndices.append(index);
        }
    }
    if (result.frames.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("No frames remain after applying the selected range and filters.");
        return {};
    }
    return result;
}

QVector<QImage> SpriteSheetSlicer::sliceFrames(const QImage &sheet,
                                               const SpriteSheetSliceOptions &options,
                                               QString *errorMessage)
{
    return sliceFrameResult(sheet, options, errorMessage).frames;
}

QStringList SpriteSheetSlicer::sliceToPngFrames(const QString &sheetPath,
                                                const QString &targetDir,
                                                const SpriteSheetSliceOptions &options,
                                                QString *errorMessage)
{
    const QImage sheet(sheetPath);
    if (sheet.isNull()) {
        if (errorMessage) *errorMessage = QStringLiteral("Sprite sheet cannot be read: %1").arg(sheetPath);
        return {};
    }
    const QVector<QImage> frames = sliceFrames(sheet, options, errorMessage);
    if (frames.isEmpty()) return {};

    QDir().mkpath(targetDir);
    QStringList files;
    for (int index = 0; index < frames.size(); ++index) {
        const QString path = QDir(targetDir).filePath(QStringLiteral("%1.png").arg(index + 1, 4, 10, QChar('0')));
        if (!frames.at(index).save(path, "PNG")) {
            if (errorMessage) *errorMessage = QStringLiteral("Failed to save sliced frame: %1").arg(path);
            return {};
        }
        files.append(path);
    }
    return files;
}

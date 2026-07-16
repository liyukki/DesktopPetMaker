#ifndef SPRITESHEETSLICER_H
#define SPRITESHEETSLICER_H

#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>

enum class SpriteSheetReadingOrder
{
    LeftToRightTopToBottom,
    TopToBottomLeftToRight
};

struct SpriteSheetSliceOptions
{
    int rows {1};
    int columns {1};
    int frameWidth {0};
    int frameHeight {0};
    int marginLeft {0};
    int marginTop {0};
    int marginRight {0};
    int marginBottom {0};
    int spacingX {0};
    int spacingY {0};
    int startFrame {0};
    int maxFrames {0};
    bool skipTransparentFrames {false};
    SpriteSheetReadingOrder readingOrder {SpriteSheetReadingOrder::LeftToRightTopToBottom};
};

struct SpriteSheetSliceResult
{
    QVector<QImage> frames;
    QVector<int> sourceCellIndices;
};

class SpriteSheetSlicer
{
public:
    static SpriteSheetSliceResult sliceFrameResult(const QImage &sheet,
                                                   const SpriteSheetSliceOptions &options,
                                                   QString *errorMessage = nullptr);
    static QVector<QImage> sliceFrames(const QImage &sheet,
                                       const SpriteSheetSliceOptions &options,
                                       QString *errorMessage = nullptr);
    static QStringList sliceToPngFrames(const QString &sheetPath,
                                        const QString &targetDir,
                                        const SpriteSheetSliceOptions &options,
                                        QString *errorMessage = nullptr);
};

#endif // SPRITESHEETSLICER_H

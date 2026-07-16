#include <QtTest>

#include "spritesheetslicer.h"

class SpriteSheetTests : public QObject
{
    Q_OBJECT
private slots:
    void preservesSourceCellMappingWhenTransparentFramesAreSkipped()
    {
        QImage sheet(30, 10, QImage::Format_ARGB32_Premultiplied);
        sheet.fill(Qt::transparent);
        for (int y = 0; y < 10; ++y) for (int x = 10; x < 20; ++x) sheet.setPixelColor(x, y, Qt::red);
        SpriteSheetSliceOptions options;
        options.rows = 1;
        options.columns = 3;
        options.frameWidth = 10;
        options.frameHeight = 10;
        options.skipTransparentFrames = true;
        QString error;
        const SpriteSheetSliceResult result = SpriteSheetSlicer::sliceFrameResult(sheet, options, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(result.frames.size(), 1);
        QCOMPARE(result.sourceCellIndices, QVector<int>({1}));
    }
};

QTEST_MAIN(SpriteSheetTests)
#include "sprite_sheet_tests.moc"

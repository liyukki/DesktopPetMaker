#include <QtTest>

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include "spritesheetimportdialog.h"

class GuiWidgetTests : public QObject
{
    Q_OBJECT
private slots:
    void spriteSheetDialogSupportsFrameSelectionAndValidation()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QImage sheet(20, 10, QImage::Format_ARGB32_Premultiplied);
        sheet.fill(Qt::transparent);
        for (int y = 0; y < 10; ++y) {
            for (int x = 0; x < 10; ++x) sheet.setPixelColor(x, y, Qt::red);
            for (int x = 10; x < 20; ++x) sheet.setPixelColor(x, y, Qt::blue);
        }
        const QString path = dir.filePath("sheet.png");
        QVERIFY(sheet.save(path));
        SpriteSheetImportDialog dialog(path);
        auto *columns = dialog.findChild<QSpinBox *>(QStringLiteral("columnsSpin"));
        auto *frameIndex = dialog.findChild<QSpinBox *>(QStringLiteral("frameIndexSpin"));
        auto *grid = dialog.findChild<QLabel *>(QStringLiteral("gridPreview"));
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("dialogButtons"));
        QVERIFY(columns && frameIndex && grid && buttons);
        columns->setValue(2);
        QCOMPARE(frameIndex->maximum(), 2);
        QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
        const qint64 firstGrid = grid->pixmap().cacheKey();
        frameIndex->setValue(2);
        QVERIFY(grid->pixmap().cacheKey() != firstGrid);
        QCOMPARE(dialog.options().columns, 2);
        columns->setValue(3);
        QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
    }
};

QTEST_MAIN(GuiWidgetTests)
#include "gui_widget_tests.moc"

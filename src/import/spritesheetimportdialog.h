#ifndef SPRITESHEETIMPORTDIALOG_H
#define SPRITESHEETIMPORTDIALOG_H

#include <QDialog>
#include <QImage>
#include <QVector>

#include "spritesheetslicer.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;

class SpriteSheetImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpriteSheetImportDialog(const QString &sheetPath, QWidget *parent = nullptr);
    SpriteSheetSliceOptions options() const;

private:
    void updatePreview();
    void updateSelectedFrame();
    void updateGridPreview();
    QSpinBox *makeSpin(int minimum, int maximum, int value);

    QImage m_sheet;
    QVector<QImage> m_frames;
    QVector<int> m_sourceCellIndices;
    QSpinBox *m_rows {nullptr};
    QSpinBox *m_columns {nullptr};
    QSpinBox *m_frameWidth {nullptr};
    QSpinBox *m_frameHeight {nullptr};
    QSpinBox *m_marginLeft {nullptr};
    QSpinBox *m_marginTop {nullptr};
    QSpinBox *m_marginRight {nullptr};
    QSpinBox *m_marginBottom {nullptr};
    QSpinBox *m_spacingX {nullptr};
    QSpinBox *m_spacingY {nullptr};
    QSpinBox *m_startFrame {nullptr};
    QSpinBox *m_endFrame {nullptr};
    QSpinBox *m_zoom {nullptr};
    QSpinBox *m_frameIndex {nullptr};
    QComboBox *m_readingOrder {nullptr};
    QCheckBox *m_skipTransparent {nullptr};
    QLabel *m_gridPreview {nullptr};
    QLabel *m_framePreview {nullptr};
    QLabel *m_status {nullptr};
    QLabel *m_frameIndexStatus {nullptr};
};

#endif // SPRITESHEETIMPORTDIALOG_H

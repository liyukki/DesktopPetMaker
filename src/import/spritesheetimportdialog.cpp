#include "spritesheetimportdialog.h"
#include "ui/theme/apptheme.h"
#include "ui/theme/iconprovider.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SpriteSheetImportDialog::SpriteSheetImportDialog(const QString &sheetPath, QWidget *parent)
    : QDialog(parent)
    , m_sheet(sheetPath)
{
    setWindowTitle(QStringLiteral("Sprite Sheet 导入"));
    setMinimumSize(860, 620);
    resize(980, 720);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(14);
    auto *title = new QLabel(QStringLiteral("Sprite Sheet 切帧导入"), this);
    auto *subtitle = new QLabel(QStringLiteral("调整网格、边距和读取顺序，并在导入前检查每一帧。"), this);
    AppTheme::setRole(title, QStringLiteral("pageTitle"));
    AppTheme::setRole(subtitle, QStringLiteral("muted"));
    root->addWidget(title);
    root->addWidget(subtitle);
    auto *content = new QGridLayout;
    content->setHorizontalSpacing(14);
    content->setVerticalSpacing(14);
    root->addLayout(content, 1);

    auto *settings = new QGroupBox(QStringLiteral("切帧参数"), this);
    auto *form = new QFormLayout(settings);
    m_rows = makeSpin(1, 256, 1);
    m_columns = makeSpin(1, 256, 1);
    m_frameWidth = makeSpin(0, 16384, 0);
    m_frameHeight = makeSpin(0, 16384, 0);
    m_rows->setObjectName(QStringLiteral("rowsSpin"));
    m_columns->setObjectName(QStringLiteral("columnsSpin"));
    m_frameWidth->setObjectName(QStringLiteral("frameWidthSpin"));
    m_frameHeight->setObjectName(QStringLiteral("frameHeightSpin"));
    m_marginLeft = makeSpin(0, 16384, 0);
    m_marginTop = makeSpin(0, 16384, 0);
    m_marginRight = makeSpin(0, 16384, 0);
    m_marginBottom = makeSpin(0, 16384, 0);
    m_spacingX = makeSpin(0, 16384, 0);
    m_spacingY = makeSpin(0, 16384, 0);
    m_startFrame = makeSpin(1, 65536, 1);
    m_endFrame = makeSpin(0, 65536, 0);
    m_endFrame->setSpecialValueText(QStringLiteral("全部"));
    m_zoom = makeSpin(25, 400, 100);
    m_zoom->setSuffix(QStringLiteral("%"));
    m_readingOrder = new QComboBox(settings);
    m_readingOrder->addItem(QStringLiteral("从左到右，再从上到下"));
    m_readingOrder->addItem(QStringLiteral("从上到下，再从左到右"));
    m_skipTransparent = new QCheckBox(QStringLiteral("跳过全透明帧"), settings);
    form->addRow(QStringLiteral("行数"), m_rows);
    form->addRow(QStringLiteral("列数"), m_columns);
    form->addRow(QStringLiteral("帧宽（0=自动）"), m_frameWidth);
    form->addRow(QStringLiteral("帧高（0=自动）"), m_frameHeight);
    form->addRow(QStringLiteral("左边距"), m_marginLeft);
    form->addRow(QStringLiteral("上边距"), m_marginTop);
    form->addRow(QStringLiteral("右边距"), m_marginRight);
    form->addRow(QStringLiteral("下边距"), m_marginBottom);
    form->addRow(QStringLiteral("水平间距"), m_spacingX);
    form->addRow(QStringLiteral("垂直间距"), m_spacingY);
    form->addRow(QStringLiteral("读取顺序"), m_readingOrder);
    form->addRow(QStringLiteral("起始帧"), m_startFrame);
    form->addRow(QStringLiteral("结束帧"), m_endFrame);
    form->addRow(QStringLiteral("预览缩放"), m_zoom);
    form->addRow(QString(), m_skipTransparent);
    content->addWidget(settings, 0, 0, 2, 1);

    auto *gridBox = new QGroupBox(QStringLiteral("网格预览"), this);
    auto *gridLayout = new QVBoxLayout(gridBox);
    m_gridPreview = new QLabel(gridBox);
    m_gridPreview->setObjectName(QStringLiteral("gridPreview"));
    m_gridPreview->setAlignment(Qt::AlignCenter);
    m_gridPreview->setMinimumSize(480, 320);
    AppTheme::setRole(m_gridPreview, QStringLiteral("subtleCard"));
    gridLayout->addWidget(m_gridPreview);
    content->addWidget(gridBox, 0, 1);

    auto *frameBox = new QGroupBox(QStringLiteral("当前帧预览"), this);
    auto *frameLayout = new QVBoxLayout(frameBox);
    m_framePreview = new QLabel(frameBox);
    m_framePreview->setObjectName(QStringLiteral("framePreview"));
    m_framePreview->setAlignment(Qt::AlignCenter);
    m_framePreview->setMinimumHeight(180);
    AppTheme::setRole(m_framePreview, QStringLiteral("subtleCard"));
    frameLayout->addWidget(m_framePreview);
    auto *frameNavigation = new QHBoxLayout;
    auto *previousButton = new QPushButton(QStringLiteral("上一帧"), frameBox);
    auto *nextButton = new QPushButton(QStringLiteral("下一帧"), frameBox);
    IconProvider::apply(previousButton, AppIcon::Motion);
    IconProvider::apply(nextButton, AppIcon::Motion);
    m_frameIndex = makeSpin(1, 1, 1);
    m_frameIndex->setObjectName(QStringLiteral("frameIndexSpin"));
    m_frameIndexStatus = new QLabel(frameBox);
    frameNavigation->addWidget(previousButton);
    frameNavigation->addWidget(m_frameIndex);
    frameNavigation->addWidget(nextButton);
    frameNavigation->addWidget(m_frameIndexStatus, 1);
    frameLayout->addLayout(frameNavigation);
    content->addWidget(frameBox, 1, 1);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("validationStatus"));
    m_status->setWordWrap(true);
    AppTheme::setRole(m_status, QStringLiteral("status"));
    root->addWidget(m_status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("dialogButtons"));
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("导入"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    AppTheme::setButtonRole(buttons->button(QDialogButtonBox::Ok), QStringLiteral("primary"));
    IconProvider::apply(buttons->button(QDialogButtonBox::Ok), AppIcon::Import);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    const QList<QSpinBox *> spins {m_rows, m_columns, m_frameWidth, m_frameHeight,
                                   m_marginLeft, m_marginTop, m_marginRight, m_marginBottom,
                                   m_spacingX, m_spacingY, m_startFrame, m_endFrame, m_zoom};
    for (QSpinBox *spin : spins) {
        connect(spin, &QSpinBox::valueChanged, this, &SpriteSheetImportDialog::updatePreview);
    }
    connect(m_readingOrder, &QComboBox::currentIndexChanged, this, &SpriteSheetImportDialog::updatePreview);
    connect(m_skipTransparent, &QCheckBox::toggled, this, &SpriteSheetImportDialog::updatePreview);
    connect(m_frameIndex, &QSpinBox::valueChanged, this, &SpriteSheetImportDialog::updateSelectedFrame);
    connect(previousButton, &QPushButton::clicked, this, [this]() { m_frameIndex->stepDown(); });
    connect(nextButton, &QPushButton::clicked, this, [this]() { m_frameIndex->stepUp(); });
    updatePreview();
}

QSpinBox *SpriteSheetImportDialog::makeSpin(int minimum, int maximum, int value)
{
    auto *spin = new QSpinBox(this);
    spin->setRange(minimum, maximum);
    spin->setValue(value);
    return spin;
}

SpriteSheetSliceOptions SpriteSheetImportDialog::options() const
{
    SpriteSheetSliceOptions value;
    value.rows = m_rows->value();
    value.columns = m_columns->value();
    value.frameWidth = m_frameWidth->value();
    value.frameHeight = m_frameHeight->value();
    value.marginLeft = m_marginLeft->value();
    value.marginTop = m_marginTop->value();
    value.marginRight = m_marginRight->value();
    value.marginBottom = m_marginBottom->value();
    value.spacingX = m_spacingX->value();
    value.spacingY = m_spacingY->value();
    value.startFrame = m_startFrame->value() - 1;
    const int endFrame = m_endFrame->value();
    value.maxFrames = endFrame > 0 ? endFrame - value.startFrame : 0;
    value.skipTransparentFrames = m_skipTransparent->isChecked();
    value.readingOrder = m_readingOrder->currentIndex() == 0
                             ? SpriteSheetReadingOrder::LeftToRightTopToBottom
                             : SpriteSheetReadingOrder::TopToBottomLeftToRight;
    return value;
}

void SpriteSheetImportDialog::updatePreview()
{
    QString error;
    const SpriteSheetSliceResult sliceResult = SpriteSheetSlicer::sliceFrameResult(m_sheet, options(), &error);
    m_frames = sliceResult.frames;
    m_sourceCellIndices = sliceResult.sourceCellIndices;
    const bool valid = !m_frames.isEmpty();
    if (!valid) {
        m_status->setText(QStringLiteral("错误：%1").arg(error));
        m_status->setProperty("status", QStringLiteral("error"));
        AppTheme::setRole(m_status, QStringLiteral("status"));
        m_framePreview->clear();
    } else {
        m_status->setText(QStringLiteral("预计导入 %1 帧，单帧 %2 × %3")
                              .arg(m_frames.size()).arg(m_frames.first().width()).arg(m_frames.first().height()));
        m_status->setProperty("status", QStringLiteral("success"));
        AppTheme::setRole(m_status, QStringLiteral("status"));
        const int previousIndex = m_frameIndex->value();
        m_frameIndex->setRange(1, m_frames.size());
        m_frameIndex->setValue(qBound(1, previousIndex, m_frames.size()));
    }

    updateGridPreview();
    if (auto *button = findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)) {
        button->setEnabled(valid);
    }
    updateSelectedFrame();
}

void SpriteSheetImportDialog::updateSelectedFrame()
{
    if (m_frames.isEmpty()) {
        m_framePreview->clear();
        m_frameIndexStatus->clear();
        return;
    }
    const int index = qBound(0, m_frameIndex->value() - 1, m_frames.size() - 1);
    const QSize previewSize = m_frames.at(index).size() * (m_zoom->value() / 100.0);
    m_framePreview->setPixmap(QPixmap::fromImage(m_frames.at(index)).scaled(previewSize,
                                                                            Qt::KeepAspectRatio,
                                                                            Qt::SmoothTransformation));
    m_frameIndexStatus->setText(QStringLiteral("导入帧 %1/%2 · 原始格 %3/%4")
                                    .arg(index + 1)
                                    .arg(m_frames.size())
                                    .arg(m_sourceCellIndices.value(index, index) + 1)
                                    .arg(options().rows * options().columns));
    updateGridPreview();
}

void SpriteSheetImportDialog::updateGridPreview()
{
    if (m_sheet.isNull()) return;
    QImage overlay = m_sheet.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&overlay);
    const SpriteSheetSliceOptions value = options();
    const int usableWidth = overlay.width() - value.marginLeft - value.marginRight - value.spacingX * (value.columns - 1);
    const int usableHeight = overlay.height() - value.marginTop - value.marginBottom - value.spacingY * (value.rows - 1);
    const int frameWidth = value.frameWidth > 0 ? value.frameWidth : usableWidth / value.columns;
    const int frameHeight = value.frameHeight > 0 ? value.frameHeight : usableHeight / value.rows;
    if (frameWidth > 0 && frameHeight > 0) {
        for (int row = 0; row < value.rows; ++row) {
            for (int column = 0; column < value.columns; ++column) {
                const int sourceCell = value.readingOrder == SpriteSheetReadingOrder::LeftToRightTopToBottom
                    ? row * value.columns + column
                    : column * value.rows + row;
                const int selectedSourceCell = m_sourceCellIndices.value(m_frameIndex->value() - 1, -1);
                painter.setPen(QPen(sourceCell == selectedSourceCell
                                        ? QColor(220, 38, 38, 240)
                                        : QColor(26, 115, 232, 180),
                                    sourceCell == selectedSourceCell ? qMax(2, overlay.width() / 300)
                                                                     : qMax(1, overlay.width() / 500)));
                painter.drawRect(value.marginLeft + column * (frameWidth + value.spacingX),
                                 value.marginTop + row * (frameHeight + value.spacingY), frameWidth, frameHeight);
            }
        }
    }
    painter.end();
    m_gridPreview->setPixmap(QPixmap::fromImage(overlay).scaled(m_gridPreview->size(),
                                                                Qt::KeepAspectRatio,
                                                                Qt::SmoothTransformation));
}

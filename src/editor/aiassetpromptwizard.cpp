#include "aiassetpromptwizard.h"

#include "motionpromptlibrary.h"

#include <QApplication>
#include <QClipboard>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

AIAssetPromptWizard::AIAssetPromptWizard(const QString &actionName,
                                         int frameCount,
                                         int rows,
                                         int columns,
                                         QWidget *parent)
    : QDialog(parent)
{
    const MotionPromptSpec spec = MotionPromptLibrary::specForAction(actionName);

    setWindowTitle(QStringLiteral("AI 动作素材提示词"));
    resize(760, 660);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_actionEdit = new QLineEdit(actionName.trimmed().isEmpty() ? spec.id : actionName.trimmed(), this);

    m_frameSpin = new QSpinBox(this);
    m_frameSpin->setRange(1, 64);
    m_frameSpin->setValue(qBound(1, frameCount > 0 ? frameCount : spec.frameCount, 64));

    m_rowsSpin = new QSpinBox(this);
    m_rowsSpin->setRange(1, 16);
    m_rowsSpin->setValue(qBound(1, rows > 0 ? rows : spec.rows, 16));

    m_columnsSpin = new QSpinBox(this);
    m_columnsSpin->setRange(1, 16);
    m_columnsSpin->setValue(qBound(1, columns > 0 ? columns : spec.columns, 16));

    m_stepsEdit = new QTextEdit(this);
    m_stepsEdit->setFixedHeight(120);
    m_stepsEdit->setPlaceholderText(QStringLiteral("可选：描述每一帧的动作变化。"));
    m_stepsEdit->setPlainText(spec.storyboard);

    form->addRow(QStringLiteral("动作 ID:"), m_actionEdit);
    form->addRow(QStringLiteral("帧数:"), m_frameSpin);
    form->addRow(QStringLiteral("行数:"), m_rowsSpin);
    form->addRow(QStringLiteral("列数:"), m_columnsSpin);
    form->addRow(QStringLiteral("动作分镜:"), m_stepsEdit);
    layout->addLayout(form);

    m_promptEdit = new QTextEdit(this);
    m_negativePromptEdit = new QTextEdit(this);
    m_negativePromptEdit->setFixedHeight(100);

    auto *copy = new QPushButton(QStringLiteral("复制 Prompt"), this);
    auto *copyNegative = new QPushButton(QStringLiteral("复制 Negative Prompt"), this);
    connect(copy, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_promptEdit->toPlainText());
    });
    connect(copyNegative, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_negativePromptEdit->toPlainText());
    });
    connect(m_actionEdit, &QLineEdit::textChanged, this, &AIAssetPromptWizard::updatePrompt);
    connect(m_frameSpin, qOverload<int>(&QSpinBox::valueChanged), this, &AIAssetPromptWizard::updatePrompt);
    connect(m_rowsSpin, qOverload<int>(&QSpinBox::valueChanged), this, &AIAssetPromptWizard::updatePrompt);
    connect(m_columnsSpin, qOverload<int>(&QSpinBox::valueChanged), this, &AIAssetPromptWizard::updatePrompt);
    connect(m_stepsEdit, &QTextEdit::textChanged, this, &AIAssetPromptWizard::updatePrompt);

    auto *copyRow = new QHBoxLayout();
    copyRow->addWidget(copy);
    copyRow->addWidget(copyNegative);

    layout->addWidget(m_promptEdit, 1);
    layout->addWidget(m_negativePromptEdit);
    layout->addLayout(copyRow);
    updatePrompt();
}

void AIAssetPromptWizard::updatePrompt()
{
    const QString actionName = m_actionEdit->text().trimmed();
    m_promptEdit->setPlainText(MotionPromptLibrary::promptForAction(actionName,
                                                                    m_frameSpin->value(),
                                                                    m_rowsSpin->value(),
                                                                    m_columnsSpin->value(),
                                                                    m_stepsEdit->toPlainText()));
    m_negativePromptEdit->setPlainText(MotionPromptLibrary::negativePromptForAction(actionName));
}

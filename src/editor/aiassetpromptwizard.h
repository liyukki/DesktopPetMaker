#ifndef AIASSETPROMPTWIZARD_H
#define AIASSETPROMPTWIZARD_H

#include <QDialog>

class QLineEdit;
class QSpinBox;
class QTextEdit;

class AIAssetPromptWizard : public QDialog
{
    Q_OBJECT

public:
    explicit AIAssetPromptWizard(const QString &actionName = QString(),
                                 int frameCount = 8,
                                 int rows = 2,
                                 int columns = 4,
                                 QWidget *parent = nullptr);

private:
    void updatePrompt();

    QLineEdit *m_actionEdit {nullptr};
    QSpinBox *m_frameSpin {nullptr};
    QSpinBox *m_rowsSpin {nullptr};
    QSpinBox *m_columnsSpin {nullptr};
    QTextEdit *m_stepsEdit {nullptr};
    QTextEdit *m_promptEdit {nullptr};
    QTextEdit *m_negativePromptEdit {nullptr};
};

#endif // AIASSETPROMPTWIZARD_H

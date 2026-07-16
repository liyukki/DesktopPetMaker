#ifndef AISETTINGSDIALOG_H
#define AISETTINGSDIALOG_H

#include <QDialog>
#include <QVector>
#include <functional>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTextEdit;

class AISettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AISettingsDialog(QWidget *parent = nullptr);

private:
    struct PersonalityPreset {
        QString id;
        QString name;
        QString emoji;
        QString systemPrompt;
    };

    void setupUi();
    void applyStyle();
    void initPresets();
    void loadSettings();
    bool saveSettings();
    QString currentBaseUrl() const;
    void onPersonalityChanged(int index);
    void onTestClicked();
    void onSaveClicked();
    void onResetClicked();
    void onShowKeyToggled(bool checked);

    QComboBox *m_baseUrlCombo {nullptr};
    QLineEdit *m_apiKeyEdit {nullptr};
    QLineEdit *m_modelEdit {nullptr};
    QCheckBox *m_showKeyCheck {nullptr};
    QComboBox *m_personalityCombo {nullptr};
    QTextEdit *m_systemPromptEdit {nullptr};
    QPushButton *m_testButton {nullptr};
    QPushButton *m_saveButton {nullptr};
    QPushButton *m_cancelButton {nullptr};
    QPushButton *m_resetButton {nullptr};

    QVector<PersonalityPreset> m_presets;
    bool m_loading {false};
};

#endif // AISETTINGSDIALOG_H

#ifndef ACTIONMATERIALWINDOW_H
#define ACTIONMATERIALWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QTimer>

#include "petproject.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTextEdit;
class PreviewCanvas;
class RuntimePetManager;

class ActionMaterialWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ActionMaterialWindow(RuntimePetManager *runtimeManager = nullptr, QWidget *parent = nullptr);
    void loadProject(const QString &petJsonPath);

private:
    void setupUi();
    void refreshActionList();
    void loadSelectedAction();
    bool saveCurrentAction();
    void addAction();
    void copyAction();
    void deleteAction();
    void moveFrameUp();
    void moveFrameDown();
    void deleteFrame();
    void reloadFrameList(const PetAction &action);
    void writeFrameListToAction(PetAction &action) const;
    void refreshPreview();
    void previousFrame();
    void nextFrame();
    void togglePreviewPlayback();
    int previewFrameDurationMs() const;
    void importMaterial();
    void importPngSequence();
    void importGif();
    void importSpriteSheet();
    void importProceduralMotion();
    void openAiPromptWizard();
    void openShimejiWizard();
    void runQualityCheck();
    void autoAlignFrames();
    void saveAndRestart();
    QString currentActionId() const;
    QString uniqueActionId() const;

    PetProject m_project;
    QString m_projectPath;
    RuntimePetManager *m_runtimeManager {nullptr};
    QListWidget *m_actionList {nullptr};
    QLabel *m_projectLabel {nullptr};
    QLineEdit *m_displayNameEdit {nullptr};
    QLineEdit *m_englishNameEdit {nullptr};
    QLineEdit *m_actionIdEdit {nullptr};
    QTextEdit *m_descriptionEdit {nullptr};
    QComboBox *m_roleCombo {nullptr};
    QComboBox *m_sourceTypeCombo {nullptr};
    QSpinBox *m_fpsSpin {nullptr};
    QCheckBox *m_loopCheck {nullptr};
    QSpinBox *m_playCountSpin {nullptr};
    QSpinBox *m_expectedFrameCountSpin {nullptr};
    QSpinBox *m_sheetRowsSpin {nullptr};
    QSpinBox *m_sheetColumnsSpin {nullptr};
    QLineEdit *m_previewScaleEdit {nullptr};
    QSpinBox *m_footBaselineOffsetSpin {nullptr};
    QCheckBox *m_allowHorizontalDisplacementCheck {nullptr};
    QCheckBox *m_mirrorSupportedCheck {nullptr};
    QComboBox *m_nextCombo {nullptr};
    QCheckBox *m_showInMenuCheck {nullptr};
    QLineEdit *m_menuGroupEdit {nullptr};
    QCheckBox *m_allowAiCheck {nullptr};
    QLineEdit *m_aiAllowedStatesEdit {nullptr};
    QTextEdit *m_aiParameterSchemaEdit {nullptr};
    QCheckBox *m_allowRandomCheck {nullptr};
    QLineEdit *m_randomWeightEdit {nullptr};
    QLineEdit *m_randomCooldownEdit {nullptr};
    QTextEdit *m_defaultPromptEdit {nullptr};
    QTextEdit *m_negativePromptEdit {nullptr};
    QListWidget *m_frameList {nullptr};
    PreviewCanvas *m_preview {nullptr};
    QPushButton *m_playButton {nullptr};
    QCheckBox *m_previousOnionCheck {nullptr};
    QCheckBox *m_nextOnionCheck {nullptr};
    QTimer m_previewTimer;
    bool m_previewPlaying {false};
};

#endif // ACTIONMATERIALWINDOW_H

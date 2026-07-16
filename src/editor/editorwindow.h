#ifndef EDITORWINDOW_H
#define EDITORWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QTimer>

#include "petproject.h"

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class PreviewCanvas;
class QPushButton;
class QSpinBox;
class QTextEdit;
class RuntimePetWindow;

class EditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit EditorWindow(QWidget *parent = nullptr);
    void openProjectFile(const QString &petJsonPath);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createUi();
    void newProject();
    void openProject();
    void saveProject();
    void importFramesForCurrentState();
    void importGifForCurrentState();
    void replaceCover();
    void validateProject();
    void exportPetpack();
    void importPetpack();
    void runPet();
    void refreshUi();
    void refreshStatePanel();
    void refreshRuntimeControls();
    void applyStateConfig();
    void applyProjectConfig();
    void applyFrameOffset();
    void resetStateOffset();
    void resetFrameOffset();
    void applyFrameOffsetToState();
    void suggestAnchor();
    void startPreview();
    void tickPreview();
    void updatePreview();
    QString currentStateName() const;
    int currentFrameIndex() const;
    void setStatus(const QString &message);
    void loadSettings();
    void saveSettings();
    void rememberCurrentProject();
    void showValidationResult(const QStringList &errors, const QStringList &warnings);
    bool ensureProjectReady() const;

    PetProject m_project;
    bool m_hasProject {false};
    bool m_updatingUi {false};
    int m_previewFrameIndex {0};
    QTimer *m_previewTimer {nullptr};
    QPointer<RuntimePetWindow> m_runtimeWindow;

    QComboBox *m_templateCombo {nullptr};
    QPushButton *m_newButton {nullptr};
    QPushButton *m_openButton {nullptr};
    QPushButton *m_saveButton {nullptr};
    QPushButton *m_importButton {nullptr};
    QPushButton *m_importGifButton {nullptr};
    QPushButton *m_coverButton {nullptr};
    QPushButton *m_validateButton {nullptr};
    QPushButton *m_exportPetpackButton {nullptr};
    QPushButton *m_importPetpackButton {nullptr};
    QPushButton *m_runButton {nullptr};
    QListWidget *m_stateList {nullptr};
    PreviewCanvas *m_previewCanvas {nullptr};
    QLabel *m_infoLabel {nullptr};
    QSpinBox *m_fpsSpin {nullptr};
    QCheckBox *m_loopCheck {nullptr};
    QComboBox *m_nextCombo {nullptr};
    QSpinBox *m_anchorXSpin {nullptr};
    QSpinBox *m_anchorYSpin {nullptr};
    QPushButton *m_suggestAnchorButton {nullptr};
    QSpinBox *m_stateOffsetXSpin {nullptr};
    QSpinBox *m_stateOffsetYSpin {nullptr};
    QPushButton *m_resetStateOffsetButton {nullptr};
    QSpinBox *m_frameSpin {nullptr};
    QSpinBox *m_frameOffsetXSpin {nullptr};
    QSpinBox *m_frameOffsetYSpin {nullptr};
    QPushButton *m_resetFrameOffsetButton {nullptr};
    QPushButton *m_applyFrameToStateButton {nullptr};
    QDoubleSpinBox *m_importGifScaleSpin {nullptr};
    QDoubleSpinBox *m_scaleSpin {nullptr};
    QCheckBox *m_mousePassthroughCheck {nullptr};
    QCheckBox *m_lockedCheck {nullptr};
    QCheckBox *m_topMostCheck {nullptr};
    QLineEdit *m_aiCharacterNameEdit {nullptr};
    QTextEdit *m_aiSystemPromptEdit {nullptr};
    QComboBox *m_renderBackendCombo {nullptr};
    QLineEdit *m_live2dModelPathEdit {nullptr};
};

#endif // EDITORWINDOW_H

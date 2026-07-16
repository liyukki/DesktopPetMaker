#include "editorwindow.h"
#include "renderbackend.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include "previewcanvas.h"
#include "runtimepetwindow.h"
#include "ui/theme/apptheme.h"
#include "ui/theme/iconprovider.h"

namespace {
constexpr int kSpinLimit = 10000;

} // namespace

EditorWindow::EditorWindow(QWidget *parent)
    : QMainWindow(parent)
{
    createUi();
    loadSettings();
    refreshUi();
}

void EditorWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void EditorWindow::createUi()
{
    setWindowTitle(QStringLiteral("桌宠制作器"));
    resize(1160, 720);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto *toolbar = new QHBoxLayout();
    m_templateCombo = new QComboBox(central);
    m_templateCombo->addItems(PetProject::templateNames());
    m_newButton = new QPushButton(tr("New Project"), central);
    m_openButton = new QPushButton(tr("Open"), central);
    m_saveButton = new QPushButton(tr("Save"), central);
    m_importButton = new QPushButton(tr("Import PNGs"), central);
    m_importGifButton = new QPushButton(tr("Import GIF"), central);
    m_coverButton = new QPushButton(tr("Replace Cover"), central);
    m_validateButton = new QPushButton(tr("Validate"), central);
    m_exportPetpackButton = new QPushButton(tr("Export Petpack"), central);
    m_importPetpackButton = new QPushButton(tr("Import Petpack"), central);
    m_runButton = new QPushButton(tr("Run Pet"), central);
    IconProvider::apply(m_newButton, AppIcon::Add);
    IconProvider::apply(m_openButton, AppIcon::Import);
    IconProvider::apply(m_saveButton, AppIcon::Save);
    IconProvider::apply(m_importButton, AppIcon::Import);
    IconProvider::apply(m_importGifButton, AppIcon::Import);
    IconProvider::apply(m_validateButton, AppIcon::Test);
    IconProvider::apply(m_exportPetpackButton, AppIcon::Save);
    IconProvider::apply(m_importPetpackButton, AppIcon::Import);
    IconProvider::apply(m_runButton, AppIcon::Play);
    AppTheme::setButtonRole(m_saveButton, QStringLiteral("primary"));
    AppTheme::setButtonRole(m_runButton, QStringLiteral("coral"));

    toolbar->addWidget(new QLabel(tr("Template:"), central));
    toolbar->addWidget(m_templateCombo);
    toolbar->addWidget(m_newButton);
    toolbar->addWidget(m_openButton);
    toolbar->addWidget(m_saveButton);
    toolbar->addStretch(1);
    toolbar->addWidget(m_importButton);
    toolbar->addWidget(m_importGifButton);
    toolbar->addWidget(m_coverButton);
    toolbar->addWidget(m_validateButton);
    toolbar->addWidget(m_exportPetpackButton);
    toolbar->addWidget(m_importPetpackButton);
    toolbar->addWidget(m_runButton);

    auto *content = new QHBoxLayout();
    m_stateList = new QListWidget(central);
    m_stateList->setMinimumWidth(150);

    m_previewCanvas = new PreviewCanvas(central);
    AppTheme::setRole(m_previewCanvas, QStringLiteral("card"));

    auto *side = new QVBoxLayout();
    auto *form = new QFormLayout();

    m_fpsSpin = new QSpinBox(central);
    m_fpsSpin->setRange(1, 60);
    m_loopCheck = new QCheckBox(tr("Loop"), central);
    m_nextCombo = new QComboBox(central);
    form->addRow(tr("FPS"), m_fpsSpin);
    form->addRow(tr("Loop"), m_loopCheck);
    form->addRow(tr("Next"), m_nextCombo);

    m_anchorXSpin = new QSpinBox(central);
    m_anchorYSpin = new QSpinBox(central);
    m_anchorXSpin->setRange(0, kSpinLimit);
    m_anchorYSpin->setRange(0, kSpinLimit);
    auto *anchorRow = new QHBoxLayout();
    anchorRow->addWidget(m_anchorXSpin);
    anchorRow->addWidget(m_anchorYSpin);
    m_suggestAnchorButton = new QPushButton(tr("Suggest"), central);
    anchorRow->addWidget(m_suggestAnchorButton);
    form->addRow(tr("Anchor X/Y"), anchorRow);

    m_stateOffsetXSpin = new QSpinBox(central);
    m_stateOffsetYSpin = new QSpinBox(central);
    m_stateOffsetXSpin->setRange(-kSpinLimit, kSpinLimit);
    m_stateOffsetYSpin->setRange(-kSpinLimit, kSpinLimit);
    auto *stateOffsetRow = new QHBoxLayout();
    stateOffsetRow->addWidget(m_stateOffsetXSpin);
    stateOffsetRow->addWidget(m_stateOffsetYSpin);
    m_resetStateOffsetButton = new QPushButton(tr("Reset"), central);
    stateOffsetRow->addWidget(m_resetStateOffsetButton);
    form->addRow(tr("State Offset"), stateOffsetRow);

    m_frameSpin = new QSpinBox(central);
    m_frameSpin->setRange(1, 1);
    form->addRow(tr("Frame"), m_frameSpin);

    m_frameOffsetXSpin = new QSpinBox(central);
    m_frameOffsetYSpin = new QSpinBox(central);
    m_frameOffsetXSpin->setRange(-kSpinLimit, kSpinLimit);
    m_frameOffsetYSpin->setRange(-kSpinLimit, kSpinLimit);
    auto *frameOffsetRow = new QHBoxLayout();
    frameOffsetRow->addWidget(m_frameOffsetXSpin);
    frameOffsetRow->addWidget(m_frameOffsetYSpin);
    m_resetFrameOffsetButton = new QPushButton(tr("Reset"), central);
    frameOffsetRow->addWidget(m_resetFrameOffsetButton);
    form->addRow(tr("Frame Offset"), frameOffsetRow);

    m_applyFrameToStateButton = new QPushButton(tr("Apply Frame Offset To State"), central);
    form->addRow(QString(), m_applyFrameToStateButton);

    m_importGifScaleSpin = new QDoubleSpinBox(central);
    m_importGifScaleSpin->setRange(10.0, 400.0);
    m_importGifScaleSpin->setSingleStep(5.0);
    m_importGifScaleSpin->setDecimals(0);
    m_importGifScaleSpin->setSuffix(tr("%"));
    m_importGifScaleSpin->setValue(100.0);
    form->addRow(tr("Import GIF Scale"), m_importGifScaleSpin);

    m_scaleSpin = new QDoubleSpinBox(central);
    m_scaleSpin->setRange(0.1, 8.0);
    m_scaleSpin->setSingleStep(0.1);
    m_scaleSpin->setDecimals(2);
    form->addRow(tr("Runtime Scale"), m_scaleSpin);

    m_mousePassthroughCheck = new QCheckBox(tr("Mouse passthrough"), central);
    m_lockedCheck = new QCheckBox(tr("Lock position"), central);
    m_topMostCheck = new QCheckBox(tr("Always on top"), central);
    form->addRow(QString(), m_mousePassthroughCheck);
    form->addRow(QString(), m_lockedCheck);
    form->addRow(QString(), m_topMostCheck);

    auto *aiGroup = new QGroupBox(QString::fromUtf8("\x41\x49\x20\xE8\xA7\x92\xE8\x89\xB2"), central);
    auto *aiLayout = new QFormLayout(aiGroup);
    m_aiCharacterNameEdit = new QLineEdit(central);
    m_aiCharacterNameEdit->setPlaceholderText(QString::fromUtf8("\xE7\x95\x99\xE7\xA9\xBA\xE6\x97\xB6\xE4\xBD\xBF\xE7\x94\xA8\xE9\xA1\xB9\xE7\x9B\xAE\xE5\x90\x8D\xE7\xA7\xB0"));
    m_aiSystemPromptEdit = new QTextEdit(central);
    m_aiSystemPromptEdit->setPlaceholderText(QString::fromUtf8("\xE7\x95\x99\xE7\xA9\xBA\xE6\x97\xB6\xE4\xBD\xBF\xE7\x94\xA8\xE5\x85\xA8\xE5\xB1\x80\xE9\xBB\x98\xE8\xAE\xA4\xE6\x8F\x90\xE7\xA4\xBA\xE8\xAF\x8D"));
    m_aiSystemPromptEdit->setMinimumHeight(90);
    m_aiSystemPromptEdit->setMaximumHeight(150);
    auto *aiHint = new QLabel(QString::fromUtf8("\xE8\xA7\x92\xE8\x89\xB2\xE5\x90\x8D\xE7\xA7\xB0\xE4\xB8\xBA\xE7\xA9\xBA\xE6\x97\xB6\xE4\xBD\xBF\xE7\x94\xA8\xE9\xA1\xB9\xE7\x9B\xAE\xE5\x90\x8D\xE7\xA7\xB0\xE3\x80\x82\xE8\xA7\x92\xE8\x89\xB2\xE6\x8F\x90\xE7\xA4\xBA\xE8\xAF\x8D\xE4\xB8\xBA\xE7\xA9\xBA\xE6\x97\xB6\xE4\xBD\xBF\xE7\x94\xA8\xE5\x85\xA8\xE5\xB1\x80\xE9\xBB\x98\xE8\xAE\xA4\xE6\x80\xA7\xE6\xA0\xBC\xE3\x80\x82"), central);
    aiHint->setWordWrap(true);
    aiLayout->addRow(QString::fromUtf8("\xE8\xA7\x92\xE8\x89\xB2\xE5\x90\x8D\xE7\xA7\xB0"), m_aiCharacterNameEdit);
    aiLayout->addRow(QString::fromUtf8("\xE8\xA7\x92\xE8\x89\xB2\xE7\xB3\xBB\xE7\xBB\x9F\xE6\x8F\x90\xE7\xA4\xBA\xE8\xAF\x8D"), m_aiSystemPromptEdit);
    aiLayout->addRow(QString(), aiHint);

    m_renderBackendCombo = new QComboBox(central);
    m_renderBackendCombo->setObjectName(QStringLiteral("renderBackendCombo"));
    m_renderBackendCombo->addItem(QStringLiteral("Sprite（当前稳定后端）"), QStringLiteral("sprite"));
    m_renderBackendCombo->addItem(QStringLiteral("Live2D（不可用时安全降级）"), QStringLiteral("live2d"));
    m_live2dModelPathEdit = new QLineEdit(central);
    m_live2dModelPathEdit->setObjectName(QStringLiteral("live2dModelPathEdit"));
    m_live2dModelPathEdit->setPlaceholderText(QStringLiteral("项目内相对路径，例如 model/model3.json"));
    auto *renderHint = new QLabel(Live2DRenderBackend::availabilityText(), central);
    renderHint->setWordWrap(true);
    aiLayout->addRow(QStringLiteral("渲染后端"), m_renderBackendCombo);
    aiLayout->addRow(QStringLiteral("Live2D Model3"), m_live2dModelPathEdit);
    aiLayout->addRow(QString(), renderHint);

    m_infoLabel = new QLabel(central);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setMinimumWidth(300);

    side->addLayout(form);
    side->addWidget(aiGroup);
    side->addWidget(m_infoLabel);
    side->addStretch(1);

    content->addWidget(m_stateList);
    content->addWidget(m_previewCanvas, 1);
    content->addLayout(side);

    root->addLayout(toolbar);
    root->addLayout(content, 1);
    setCentralWidget(central);

    m_previewTimer = new QTimer(this);
    connect(m_previewTimer, &QTimer::timeout, this, &EditorWindow::tickPreview);

    connect(m_newButton, &QPushButton::clicked, this, &EditorWindow::newProject);
    connect(m_openButton, &QPushButton::clicked, this, &EditorWindow::openProject);
    connect(m_saveButton, &QPushButton::clicked, this, &EditorWindow::saveProject);
    connect(m_importButton, &QPushButton::clicked, this, &EditorWindow::importFramesForCurrentState);
    connect(m_importGifButton, &QPushButton::clicked, this, &EditorWindow::importGifForCurrentState);
    connect(m_coverButton, &QPushButton::clicked, this, &EditorWindow::replaceCover);
    connect(m_validateButton, &QPushButton::clicked, this, &EditorWindow::validateProject);
    connect(m_exportPetpackButton, &QPushButton::clicked, this, &EditorWindow::exportPetpack);
    connect(m_importPetpackButton, &QPushButton::clicked, this, &EditorWindow::importPetpack);
    connect(m_runButton, &QPushButton::clicked, this, &EditorWindow::runPet);
    connect(m_previewCanvas, &PreviewCanvas::anchorPicked, this, [this](const QPoint &canvasPoint) {
        if (!m_hasProject || !m_project.canvasSize.isValid()) {
            return;
        }

        const int x = qBound(0, canvasPoint.x(), qMax(0, m_project.canvasSize.width()));
        const int y = qBound(0, canvasPoint.y(), qMax(0, m_project.canvasSize.height()));
        m_anchorXSpin->setValue(x);
        m_anchorYSpin->setValue(y);
        applyProjectConfig();
    });
    connect(m_previewCanvas, &PreviewCanvas::stateOffsetDragged, this, [this](const QPoint &delta) {
        if (!m_hasProject) {
            return;
        }

        m_stateOffsetXSpin->setValue(m_stateOffsetXSpin->value() + delta.x());
        m_stateOffsetYSpin->setValue(m_stateOffsetYSpin->value() + delta.y());
        applyStateConfig();
    });
    connect(m_stateList, &QListWidget::currentTextChanged, this, [this]() {
        m_previewFrameIndex = 0;
        refreshStatePanel();
        startPreview();
    });

    connect(m_fpsSpin, qOverload<int>(&QSpinBox::valueChanged), this, &EditorWindow::applyStateConfig);
    connect(m_loopCheck, &QCheckBox::toggled, this, &EditorWindow::applyStateConfig);
    connect(m_nextCombo, &QComboBox::currentTextChanged, this, &EditorWindow::applyStateConfig);
    connect(m_anchorXSpin, qOverload<int>(&QSpinBox::valueChanged), this, &EditorWindow::applyProjectConfig);
    connect(m_anchorYSpin, qOverload<int>(&QSpinBox::valueChanged), this, &EditorWindow::applyProjectConfig);
    connect(m_stateOffsetXSpin, qOverload<int>(&QSpinBox::valueChanged), this, &EditorWindow::applyStateConfig);
    connect(m_stateOffsetYSpin, qOverload<int>(&QSpinBox::valueChanged), this, &EditorWindow::applyStateConfig);
    connect(m_frameSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this]() {
        if (m_updatingUi) {
            return;
        }
        m_previewFrameIndex = currentFrameIndex();
        refreshStatePanel();
        updatePreview();
    });
    connect(m_frameOffsetXSpin, qOverload<int>(&QSpinBox::valueChanged), this, &EditorWindow::applyFrameOffset);
    connect(m_frameOffsetYSpin, qOverload<int>(&QSpinBox::valueChanged), this, &EditorWindow::applyFrameOffset);
    connect(m_resetStateOffsetButton, &QPushButton::clicked, this, &EditorWindow::resetStateOffset);
    connect(m_resetFrameOffsetButton, &QPushButton::clicked, this, &EditorWindow::resetFrameOffset);
    connect(m_applyFrameToStateButton, &QPushButton::clicked, this, &EditorWindow::applyFrameOffsetToState);
    connect(m_suggestAnchorButton, &QPushButton::clicked, this, &EditorWindow::suggestAnchor);
    connect(m_scaleSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &EditorWindow::applyProjectConfig);
    connect(m_mousePassthroughCheck, &QCheckBox::toggled, this, &EditorWindow::applyProjectConfig);
    connect(m_lockedCheck, &QCheckBox::toggled, this, &EditorWindow::applyProjectConfig);
    connect(m_topMostCheck, &QCheckBox::toggled, this, &EditorWindow::applyProjectConfig);
    connect(m_aiCharacterNameEdit, &QLineEdit::textChanged, this, &EditorWindow::applyProjectConfig);
    connect(m_aiSystemPromptEdit, &QTextEdit::textChanged, this, &EditorWindow::applyProjectConfig);
    connect(m_renderBackendCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &EditorWindow::applyProjectConfig);
    connect(m_live2dModelPathEdit, &QLineEdit::textChanged, this, &EditorWindow::applyProjectConfig);
}

void EditorWindow::newProject()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Project"), tr("Project name:"), QLineEdit::Normal,
                                               QStringLiteral("MyPet"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    const QString baseDir = QFileDialog::getExistingDirectory(
        this,
        tr("Choose Project Parent Folder"),
        QSettings().value("lastDirectory", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString());
    if (baseDir.isEmpty()) {
        return;
    }

    const QString projectDir = QDir(baseDir).filePath(name.trimmed());
    m_project = PetProject::createNew(projectDir, name, m_templateCombo->currentText());
    m_hasProject = true;

    QString error;
    if (!m_project.save(&error)) {
        QMessageBox::warning(this, tr("Save Failed"), error);
    }

    refreshUi();
    rememberCurrentProject();
    setStatus(tr("Project created. Import idle PNGs first to initialize the canvas."));
}

void EditorWindow::openProject()
{
    const QString petJson = QFileDialog::getOpenFileName(
        this,
        tr("Open pet.json"),
        QSettings().value("lastDirectory", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString(),
        tr("Pet Project (pet.json)"));
    if (petJson.isEmpty()) {
        return;
    }
    openProjectFile(petJson);
}

void EditorWindow::openProjectFile(const QString &petJson)
{
    PetProject loaded;
    QString error;
    if (!loaded.load(petJson, &error)) {
        QMessageBox::warning(this, tr("Open Failed"), error);
        return;
    }

    m_project = loaded;
    m_hasProject = true;
    refreshUi();
    rememberCurrentProject();
    setStatus(tr("Project loaded: %1").arg(m_project.petJsonPath()));
}

void EditorWindow::saveProject()
{
    if (!ensureProjectReady()) {
        return;
    }

    applyStateConfig();
    applyProjectConfig();
    QString error;
    if (!m_project.save(&error)) {
        QMessageBox::warning(this, tr("Save Failed"), error);
        return;
    }

    setStatus(tr("Saved: %1").arg(m_project.petJsonPath()));
    rememberCurrentProject();
}

void EditorWindow::importFramesForCurrentState()
{
    if (!ensureProjectReady()) {
        return;
    }

    const QString state = currentStateName();
    if (state.isEmpty()) {
        return;
    }

    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Import PNG Sequence"), QString(), tr("PNG Files (*.png)"));
    if (files.isEmpty()) {
        return;
    }

    QString error;
    if (!m_project.importPngFrames(state, files, &error)) {
        QMessageBox::warning(this, tr("Import Failed"), error);
        return;
    }

    refreshUi();
    QList<QListWidgetItem *> matches = m_stateList->findItems(state, Qt::MatchExactly);
    if (!matches.isEmpty()) {
        m_stateList->setCurrentItem(matches.first());
    }
    setStatus(tr("%1 imported: %2 frame(s)").arg(state).arg(files.size()));
}

void EditorWindow::importGifForCurrentState()
{
    if (!ensureProjectReady()) {
        return;
    }

    const QString state = currentStateName();
    if (state.isEmpty()) {
        return;
    }

    const QString gifFile = QFileDialog::getOpenFileName(this, tr("Import GIF"), QString(), tr("GIF Files (*.gif)"));
    if (gifFile.isEmpty()) {
        return;
    }

    QString error;
    const double importScale = m_importGifScaleSpin->value() / 100.0;
    if (!m_project.importGifFrames(state, gifFile, importScale, &error)) {
        QMessageBox::warning(this, tr("Import Failed"), error);
        return;
    }

    refreshUi();
    QList<QListWidgetItem *> matches = m_stateList->findItems(state, Qt::MatchExactly);
    if (!matches.isEmpty()) {
        m_stateList->setCurrentItem(matches.first());
    }
    setStatus(tr("%1 imported from GIF at %2%").arg(state).arg(m_importGifScaleSpin->value(), 0, 'f', 0));
}

void EditorWindow::replaceCover()
{
    if (!ensureProjectReady()) {
        return;
    }

    const QString file = QFileDialog::getOpenFileName(this, tr("Replace Cover"), QString(), tr("Images (*.png *.jpg *.jpeg)"));
    if (file.isEmpty()) {
        return;
    }

    QString error;
    if (!m_project.setCustomCover(file, &error)) {
        QMessageBox::warning(this, tr("Cover Failed"), error);
        return;
    }

    refreshStatePanel();
    setStatus(tr("Custom cover set. Auto cover updates are now disabled for this project."));
}

void EditorWindow::validateProject()
{
    if (!ensureProjectReady()) {
        return;
    }

    saveProject();
    QStringList errors;
    QStringList warnings;
    m_project.validate(&errors, &warnings);
    showValidationResult(errors, warnings);
}

void EditorWindow::exportPetpack()
{
    if (!ensureProjectReady()) {
        return;
    }

    saveProject();
    QStringList errors;
    QStringList warnings;
    if (!m_project.validate(&errors, &warnings)) {
        showValidationResult(errors, warnings);
        return;
    }

    const QString defaultPath = QDir(m_project.projectDir).filePath(m_project.name + ".petpack");
    const QString petpackPath = QFileDialog::getSaveFileName(
        this,
        tr("Export Petpack"),
        defaultPath,
        tr("Pet Pack (*.petpack)"));
    if (petpackPath.isEmpty()) {
        return;
    }

    QString error;
    if (!m_project.exportPetpack(petpackPath, &error)) {
        QMessageBox::warning(this, tr("Export Failed"), error);
        return;
    }

    setStatus(tr("Petpack exported: %1").arg(petpackPath));
    if (!warnings.isEmpty()) {
        QMessageBox::information(this, tr("Exported With Warnings"), warnings.join('\n'));
    }
}

void EditorWindow::importPetpack()
{
    const QString petpackPath = QFileDialog::getOpenFileName(
        this,
        tr("Import Petpack"),
        QSettings().value("lastDirectory", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString(),
        tr("Pet Pack (*.petpack *.zip)"));
    if (petpackPath.isEmpty()) {
        return;
    }

    const QString targetParentDir = QFileDialog::getExistingDirectory(
        this,
        tr("Choose Import Parent Folder"),
        QSettings().value("lastDirectory", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString());
    if (targetParentDir.isEmpty()) {
        return;
    }

    QString importedPetJson;
    QString error;
    if (!PetProject::importPetpack(petpackPath, targetParentDir, &importedPetJson, &error)) {
        QMessageBox::warning(this, tr("Import Failed"), error);
        return;
    }

    PetProject imported;
    if (!imported.load(importedPetJson, &error)) {
        QMessageBox::warning(this, tr("Open Imported Project Failed"), error);
        return;
    }

    m_project = imported;
    m_hasProject = true;
    refreshUi();
    rememberCurrentProject();
    setStatus(tr("Petpack imported: %1").arg(importedPetJson));
}

void EditorWindow::runPet()
{
    if (!ensureProjectReady()) {
        return;
    }

    saveProject();
    if (m_project.actions.value("idle").frames.isEmpty()) {
        QMessageBox::information(this, tr("Missing Idle"), tr("Please import idle PNGs before running the pet."));
        return;
    }

    if (m_runtimeWindow) {
        m_runtimeWindow->close();
    }

    m_runtimeWindow = new RuntimePetWindow(m_project);
    connect(m_runtimeWindow, &RuntimePetWindow::runtimeStateChanged, this, [this](const RuntimePetWindow::RuntimeStatePatch &patch) {
        if (patch.hasLocked) {
            m_project.locked = patch.locked;
        }
        if (patch.hasMousePassthrough) {
            m_project.mousePassthrough = patch.mousePassthrough;
        }
        if (patch.hasTopMost) {
            m_project.topMost = patch.topMost;
        }
        if (patch.hasAnchorScreen) {
            m_project.runtimeAnchorScreen = patch.runtimeAnchorScreen;
            m_project.hasRuntimeAnchorScreen = true;
        } else if (patch.clearAnchorScreen) {
            m_project.runtimeAnchorScreen = QPoint();
            m_project.hasRuntimeAnchorScreen = false;
        }
        refreshRuntimeControls();
    });
    connect(m_runtimeWindow, &QObject::destroyed, this, [this]() {
        m_runtimeWindow = nullptr;
    });
    m_runtimeWindow->show();
    setStatus(tr("Runtime pet launched."));
}

void EditorWindow::refreshUi()
{
    const QString selected = currentStateName();
    m_updatingUi = true;
    m_stateList->clear();
    if (m_hasProject) {
        m_stateList->addItems(m_project.actionNames());
        QList<QListWidgetItem *> matches = m_stateList->findItems(selected, Qt::MatchExactly);
        if (!matches.isEmpty()) {
            m_stateList->setCurrentItem(matches.first());
        } else if (m_stateList->count() > 0) {
            m_stateList->setCurrentRow(0);
        }
    }

    const bool enabled = m_hasProject;
    m_saveButton->setEnabled(enabled);
    m_importButton->setEnabled(enabled);
    m_importGifButton->setEnabled(enabled);
    m_coverButton->setEnabled(enabled);
    m_validateButton->setEnabled(enabled);
    m_exportPetpackButton->setEnabled(enabled);
    m_importPetpackButton->setEnabled(true);
    m_runButton->setEnabled(enabled);
    m_stateList->setEnabled(enabled);
    m_fpsSpin->setEnabled(enabled);
    m_loopCheck->setEnabled(enabled);
    m_nextCombo->setEnabled(enabled);
    m_anchorXSpin->setEnabled(enabled);
    m_anchorYSpin->setEnabled(enabled);
    m_suggestAnchorButton->setEnabled(enabled);
    m_stateOffsetXSpin->setEnabled(enabled);
    m_stateOffsetYSpin->setEnabled(enabled);
    m_resetStateOffsetButton->setEnabled(enabled);
    m_frameSpin->setEnabled(enabled);
    m_frameOffsetXSpin->setEnabled(enabled);
    m_frameOffsetYSpin->setEnabled(enabled);
    m_resetFrameOffsetButton->setEnabled(enabled);
    m_applyFrameToStateButton->setEnabled(enabled);
    m_importGifScaleSpin->setEnabled(enabled);
    m_scaleSpin->setEnabled(enabled);
    m_mousePassthroughCheck->setEnabled(enabled);
    m_lockedCheck->setEnabled(enabled);
    m_topMostCheck->setEnabled(enabled);
    m_aiCharacterNameEdit->setEnabled(enabled);
    m_aiSystemPromptEdit->setEnabled(enabled);
    m_renderBackendCombo->setEnabled(enabled);
    m_live2dModelPathEdit->setEnabled(enabled && m_renderBackendCombo->currentData().toString() == QStringLiteral("live2d"));
    m_updatingUi = false;

    refreshStatePanel();
    startPreview();
}

void EditorWindow::refreshStatePanel()
{
    if (!m_hasProject) {
        m_previewCanvas->setPreview(nullptr, nullptr, 0);
        m_infoLabel->setText(QString());
        return;
    }

    const QString state = currentStateName();
    const PetAction action = m_project.actions.value(state);
    const int frameCount = qMax(1, action.frames.size());
    const int frameIndex = qBound(0, m_previewFrameIndex, frameCount - 1);
    const PetFrame frame = action.frames.isEmpty() ? PetFrame() : action.frames.at(frameIndex);

    m_updatingUi = true;
    m_fpsSpin->setValue(action.fps);
    m_loopCheck->setChecked(action.loop);
    m_nextCombo->clear();
    m_nextCombo->addItem(QString());
    m_nextCombo->addItems(m_project.actionNames());
    m_nextCombo->setCurrentText(action.next);

    m_anchorXSpin->setValue(m_project.anchor.x());
    m_anchorYSpin->setValue(m_project.anchor.y());
    m_stateOffsetXSpin->setValue(action.offset.x());
    m_stateOffsetYSpin->setValue(action.offset.y());
    m_frameSpin->setMaximum(frameCount);
    m_frameSpin->setValue(frameIndex + 1);
    m_frameOffsetXSpin->setValue(frame.offset.x());
    m_frameOffsetYSpin->setValue(frame.offset.y());
    m_scaleSpin->setValue(m_project.scale);
    m_mousePassthroughCheck->setChecked(m_project.mousePassthrough);
    m_lockedCheck->setChecked(m_project.locked);
    m_topMostCheck->setChecked(m_project.topMost);
    m_aiCharacterNameEdit->setText(m_project.aiCharacterName);
    m_aiSystemPromptEdit->setPlainText(m_project.aiSystemPrompt);
    m_renderBackendCombo->setCurrentIndex(qMax(0, m_renderBackendCombo->findData(m_project.renderBackend)));
    m_live2dModelPathEdit->setText(m_project.live2dModelMetadata.value(QStringLiteral("model3Json")).toString());
    m_live2dModelPathEdit->setEnabled(m_renderBackendCombo->currentData().toString() == QStringLiteral("live2d"));
    m_updatingUi = false;

    const QString canvasText = m_project.canvasSize.isValid()
        ? QStringLiteral("%1x%2").arg(m_project.canvasSize.width()).arg(m_project.canvasSize.height())
        : QStringLiteral("not initialized");
    const QString runtimeAnchor = m_project.hasRuntimeAnchorScreen
        ? QStringLiteral("%1,%2").arg(m_project.runtimeAnchorScreen.x()).arg(m_project.runtimeAnchorScreen.y())
        : QStringLiteral("not saved");
    m_infoLabel->setText(tr("Project: %1\nDir: %2\nCanvas: %3\nState: %4\nFrames: %5\nCover: %6 (%7)\nRuntime anchor: %8")
                             .arg(m_project.name)
                             .arg(m_project.projectDir)
                             .arg(canvasText)
                             .arg(state)
                             .arg(action.frames.size())
                             .arg(m_project.coverPath)
                             .arg(m_project.coverMode)
                             .arg(runtimeAnchor));
}

void EditorWindow::refreshRuntimeControls()
{
    if (!m_hasProject) {
        return;
    }

    const bool wasUpdating = m_updatingUi;
    m_updatingUi = true;
    m_mousePassthroughCheck->setChecked(m_project.mousePassthrough);
    m_lockedCheck->setChecked(m_project.locked);
    m_topMostCheck->setChecked(m_project.topMost);
    m_updatingUi = wasUpdating;

    const QString state = currentStateName();
    const QString canvasText = m_project.canvasSize.isValid()
        ? QStringLiteral("%1x%2").arg(m_project.canvasSize.width()).arg(m_project.canvasSize.height())
        : QStringLiteral("not initialized");
    const QString runtimeAnchor = m_project.hasRuntimeAnchorScreen
        ? QStringLiteral("%1,%2").arg(m_project.runtimeAnchorScreen.x()).arg(m_project.runtimeAnchorScreen.y())
        : QStringLiteral("not saved");
    m_infoLabel->setText(tr("Project: %1\nDir: %2\nCanvas: %3\nState: %4\nFrames: %5\nCover: %6 (%7)\nRuntime anchor: %8")
                             .arg(m_project.name)
                             .arg(m_project.projectDir)
                             .arg(canvasText)
                             .arg(state)
                             .arg(m_project.actions.value(state).frames.size())
                             .arg(m_project.coverPath)
                             .arg(m_project.coverMode)
                             .arg(runtimeAnchor));
}

void EditorWindow::applyStateConfig()
{
    if (m_updatingUi || !m_hasProject) {
        return;
    }

    const QString state = currentStateName();
    if (state.isEmpty() || !m_project.actions.contains(state)) {
        return;
    }

    PetAction &action = m_project.actions[state];
    action.fps = m_fpsSpin->value();
    action.loop = m_loopCheck->isChecked();
    action.next = m_nextCombo->currentText();
    action.offset = QPoint(m_stateOffsetXSpin->value(), m_stateOffsetYSpin->value());
    startPreview();
}

void EditorWindow::applyProjectConfig()
{
    if (m_updatingUi || !m_hasProject) {
        return;
    }

    m_project.anchor = QPoint(m_anchorXSpin->value(), m_anchorYSpin->value());
    m_project.scale = m_scaleSpin->value();
    m_project.mousePassthrough = m_mousePassthroughCheck->isChecked();
    m_project.locked = m_lockedCheck->isChecked();
    m_project.topMost = m_topMostCheck->isChecked();
    m_project.aiCharacterName = m_aiCharacterNameEdit->text().trimmed();
    m_project.aiSystemPrompt = m_aiSystemPromptEdit->toPlainText().trimmed();
    m_project.renderBackend = m_renderBackendCombo->currentData().toString();
    m_project.live2dModelMetadata.insert(QStringLiteral("model3Json"), m_live2dModelPathEdit->text().trimmed());
    m_live2dModelPathEdit->setEnabled(m_project.renderBackend == QStringLiteral("live2d"));
    if (m_runtimeWindow) {
        m_runtimeWindow->setRuntimeMousePassthrough(m_project.mousePassthrough);
        m_runtimeWindow->setRuntimeLocked(m_project.locked);
        m_runtimeWindow->setRuntimeTopMost(m_project.topMost);
    }
    updatePreview();
}

void EditorWindow::applyFrameOffset()
{
    if (m_updatingUi || !m_hasProject) {
        return;
    }

    const QString state = currentStateName();
    if (!m_project.actions.contains(state)) {
        return;
    }

    PetAction &action = m_project.actions[state];
    const int index = currentFrameIndex();
    if (index < 0 || index >= action.frames.size()) {
        return;
    }

    action.frames[index].offset = QPoint(m_frameOffsetXSpin->value(), m_frameOffsetYSpin->value());
    updatePreview();
}

void EditorWindow::resetStateOffset()
{
    if (!m_hasProject) {
        return;
    }
    m_stateOffsetXSpin->setValue(0);
    m_stateOffsetYSpin->setValue(0);
    applyStateConfig();
}

void EditorWindow::resetFrameOffset()
{
    if (!m_hasProject) {
        return;
    }
    m_frameOffsetXSpin->setValue(0);
    m_frameOffsetYSpin->setValue(0);
    applyFrameOffset();
}

void EditorWindow::applyFrameOffsetToState()
{
    if (!m_hasProject) {
        return;
    }

    const QString state = currentStateName();
    if (!m_project.actions.contains(state)) {
        return;
    }

    PetAction &action = m_project.actions[state];
    const QPoint offset(m_frameOffsetXSpin->value(), m_frameOffsetYSpin->value());
    for (PetFrame &frame : action.frames) {
        frame.offset = offset;
    }
    refreshStatePanel();
    updatePreview();
    setStatus(tr("Frame offset applied to all frames in %1.").arg(state));
}

void EditorWindow::suggestAnchor()
{
    if (!m_hasProject) {
        return;
    }

    const QPoint suggested = m_project.suggestedAnchorFromIdle();
    m_anchorXSpin->setValue(suggested.x());
    m_anchorYSpin->setValue(suggested.y());
    applyProjectConfig();
    setStatus(tr("Anchor suggested from idle alpha bounds."));
}

void EditorWindow::startPreview()
{
    if (!m_hasProject) {
        m_previewTimer->stop();
        return;
    }

    const PetAction action = m_project.actions.value(currentStateName());
    if (action.frames.isEmpty()) {
        m_previewTimer->stop();
        m_previewCanvas->setPreview(&m_project, &m_project.actions[currentStateName()], 0);
        return;
    }

    m_previewTimer->start(qMax(16, 1000 / qMax(1, action.fps)));
    updatePreview();
}

void EditorWindow::tickPreview()
{
    if (!m_hasProject) {
        return;
    }

    const PetAction action = m_project.actions.value(currentStateName());
    if (action.frames.isEmpty()) {
        return;
    }

    m_previewFrameIndex = (m_previewFrameIndex + 1) % action.frames.size();
    m_updatingUi = true;
    m_frameSpin->setValue(m_previewFrameIndex + 1);
    m_updatingUi = false;
    updatePreview();
}

void EditorWindow::updatePreview()
{
    if (!m_hasProject) {
        return;
    }

    const PetAction action = m_project.actions.value(currentStateName());
    if (action.frames.isEmpty()) {
        return;
    }

    m_previewCanvas->setPreview(&m_project, &m_project.actions[currentStateName()], m_previewFrameIndex);
}

QString EditorWindow::currentStateName() const
{
    const QListWidgetItem *item = m_stateList->currentItem();
    return item ? item->text() : QString();
}

int EditorWindow::currentFrameIndex() const
{
    return qMax(0, m_frameSpin->value() - 1);
}

void EditorWindow::setStatus(const QString &message)
{
    statusBar()->showMessage(message, 8000);
}

void EditorWindow::loadSettings()
{
    QSettings settings;
    restoreGeometry(settings.value("windowGeometry").toByteArray());
    if (m_importGifScaleSpin) {
        m_importGifScaleSpin->setValue(settings.value("importGifScalePercent", 100.0).toDouble());
    }
    const QString lastProject = settings.value("lastProject").toString();
    if (lastProject.isEmpty() || !QFileInfo::exists(lastProject)) {
        return;
    }

    PetProject loaded;
    if (loaded.load(lastProject)) {
        m_project = loaded;
        m_hasProject = true;
    }
}

void EditorWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("windowGeometry", saveGeometry());
    if (m_importGifScaleSpin) {
        settings.setValue("importGifScalePercent", m_importGifScaleSpin->value());
    }
    if (m_hasProject) {
        settings.setValue("lastProject", m_project.petJsonPath());
        settings.setValue("lastDirectory", m_project.projectDir);
    }
}

void EditorWindow::rememberCurrentProject()
{
    if (!m_hasProject) {
        return;
    }

    QSettings settings;
    settings.setValue("lastProject", m_project.petJsonPath());
    settings.setValue("lastDirectory", m_project.projectDir);
}

void EditorWindow::showValidationResult(const QStringList &errors, const QStringList &warnings)
{
    if (errors.isEmpty() && warnings.isEmpty()) {
        QMessageBox::information(this, tr("Validation Passed"), tr("Project validation passed."));
        return;
    }

    QString message;
    if (!errors.isEmpty()) {
        message += tr("Errors:\n%1").arg(errors.join('\n'));
    }
    if (!warnings.isEmpty()) {
        if (!message.isEmpty()) {
            message += "\n\n";
        }
        message += tr("Warnings:\n%1").arg(warnings.join('\n'));
    }

    if (errors.isEmpty()) {
        QMessageBox::information(this, tr("Validation Warnings"), message);
    } else {
        QMessageBox::warning(this, tr("Validation Failed"), message);
    }
}

bool EditorWindow::ensureProjectReady() const
{
    return m_hasProject && m_project.isValid();
}

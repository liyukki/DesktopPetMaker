#include "actionmaterialwindow.h"

#include "alphaboundingboxutil.h"
#include "aiassetpromptwizard.h"
#include "assetqualityanalyzer.h"
#include "previewcanvas.h"
#include "proceduralmotiongenerator.h"
#include "runtimepetmanager.h"
#include "shimejiimportwizard.h"
#include "spritesheetimportdialog.h"
#include "spritesheetslicer.h"
#include "ui/theme/apptheme.h"
#include "ui/theme/iconprovider.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QImage>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QUuid>

namespace {
QString sourceTypeName(ActionSourceType type)
{
    switch (type) {
    case ActionSourceType::GifImported: return QStringLiteral("GifImported");
    case ActionSourceType::SpriteSheet: return QStringLiteral("SpriteSheet");
    case ActionSourceType::ProceduralGenerated: return QStringLiteral("ProceduralGenerated");
    case ActionSourceType::ShimejiImported: return QStringLiteral("ShimejiImported");
    case ActionSourceType::PngSequence:
    default:
        return QStringLiteral("PngSequence");
    }
}
}

ActionMaterialWindow::ActionMaterialWindow(RuntimePetManager *runtimeManager, QWidget *parent)
    : QMainWindow(parent)
    , m_runtimeManager(runtimeManager)
{
    setupUi();
}

void ActionMaterialWindow::setupUi()
{
    setWindowTitle(QStringLiteral("动作与素材管理"));
    setMinimumSize(1040, 680);
    resize(1260, 780);
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(14);
    m_projectLabel = new QLabel(QStringLiteral("未加载项目"), central);
    AppTheme::setRole(m_projectLabel, QStringLiteral("sectionTitle"));
    root->addWidget(m_projectLabel);
    auto *splitter = new QSplitter(central);
    auto *leftPanel = new QWidget(splitter);
    AppTheme::setRole(leftPanel, QStringLiteral("card"));
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    auto *actionTitle = new QLabel(QStringLiteral("动作列表"), leftPanel);
    AppTheme::setRole(actionTitle, QStringLiteral("sectionTitle"));
    m_actionList = new QListWidget(leftPanel);
    m_actionList->setObjectName(QStringLiteral("actionList"));
    m_actionList->setMinimumWidth(210);
    leftLayout->addWidget(actionTitle);
    leftLayout->addWidget(m_actionList, 1);
    auto *right = new QWidget(splitter);
    auto *rightLayout = new QHBoxLayout(right);
    rightLayout->setContentsMargins(12, 0, 0, 0);
    rightLayout->setSpacing(12);
    auto *editor = new QWidget(right);
    AppTheme::setRole(editor, QStringLiteral("card"));
    auto *form = new QFormLayout(editor);
    form->setContentsMargins(16, 14, 16, 14);
    form->setVerticalSpacing(10);
    m_displayNameEdit = new QLineEdit(right);
    m_displayNameEdit->setObjectName(QStringLiteral("actionDisplayNameEdit"));
    m_englishNameEdit = new QLineEdit(right);
    m_actionIdEdit = new QLineEdit(right);
    m_actionIdEdit->setReadOnly(true);
    m_descriptionEdit = new QTextEdit(right);
    m_descriptionEdit->setFixedHeight(64);
    m_roleCombo = new QComboBox(right);
    m_roleCombo->addItems({QStringLiteral("None"), QStringLiteral("Idle"), QStringLiteral("WalkLeft"), QStringLiteral("WalkRight"),
                           QStringLiteral("Dragging"), QStringLiteral("Falling"), QStringLiteral("Landing"),
                           QStringLiteral("Sleeping"), QStringLiteral("AIThinking"), QStringLiteral("AITalking"),
                           QStringLiteral("ClickReaction")});
    m_sourceTypeCombo = new QComboBox(right);
    m_sourceTypeCombo->addItems({QStringLiteral("PngSequence"), QStringLiteral("GifImported"),
                                 QStringLiteral("SpriteSheet"), QStringLiteral("ProceduralGenerated"),
                                 QStringLiteral("ShimejiImported")});
    m_sourceTypeCombo->setEnabled(false);
    m_fpsSpin = new QSpinBox(right);
    m_fpsSpin->setRange(1, 60);
    m_loopCheck = new QCheckBox(QStringLiteral("循环播放"), right);
    m_playCountSpin = new QSpinBox(right);
    m_playCountSpin->setRange(1, 99);
    m_expectedFrameCountSpin = new QSpinBox(right);
    m_expectedFrameCountSpin->setRange(0, 128);
    m_sheetRowsSpin = new QSpinBox(right);
    m_sheetRowsSpin->setRange(1, 16);
    m_sheetColumnsSpin = new QSpinBox(right);
    m_sheetColumnsSpin->setRange(1, 16);
    m_previewScaleEdit = new QLineEdit(right);
    m_footBaselineOffsetSpin = new QSpinBox(right);
    m_footBaselineOffsetSpin->setRange(-999, 999);
    m_allowHorizontalDisplacementCheck = new QCheckBox(QStringLiteral("允许水平位移"), right);
    m_mirrorSupportedCheck = new QCheckBox(QStringLiteral("支持左右镜像"), right);
    m_nextCombo = new QComboBox(right);
    m_showInMenuCheck = new QCheckBox(QStringLiteral("右键菜单显示"), right);
    m_menuGroupEdit = new QLineEdit(right);
    m_allowAiCheck = new QCheckBox(QStringLiteral("允许 AI 调用"), right);
    m_aiAllowedStatesEdit = new QLineEdit(right);
    m_aiAllowedStatesEdit->setObjectName(QStringLiteral("aiAllowedStatesEdit"));
    m_aiAllowedStatesEdit->setText(QStringLiteral("Normal"));
    m_aiAllowedStatesEdit->setReadOnly(true);
    m_aiAllowedStatesEdit->setToolTip(QStringLiteral("当前版本仅允许 AI 动作在 Normal 状态执行；其他状态会排队等待。"));
    m_aiParameterSchemaEdit = new QTextEdit(right);
    m_aiParameterSchemaEdit->setObjectName(QStringLiteral("aiParameterSchemaEdit"));
    m_aiParameterSchemaEdit->setFixedHeight(90);
    m_aiParameterSchemaEdit->setPlaceholderText(
        QStringLiteral("JSON Schema；仅支持 playbackSpeed、mirror、offsetX、offsetY、scale，这些参数会直接影响运行时动画"));
    m_allowRandomCheck = new QCheckBox(QStringLiteral("允许随机触发"), right);
    m_randomWeightEdit = new QLineEdit(right);
    m_randomCooldownEdit = new QLineEdit(right);
    m_defaultPromptEdit = new QTextEdit(right);
    m_defaultPromptEdit->setFixedHeight(110);
    m_negativePromptEdit = new QTextEdit(right);
    m_negativePromptEdit->setFixedHeight(70);
    m_frameList = new QListWidget(right);
    auto *frameButtons = new QHBoxLayout();
    auto *frameUp = new QPushButton(QStringLiteral("上移"), right);
    auto *frameDown = new QPushButton(QStringLiteral("下移"), right);
    auto *frameDelete = new QPushButton(QStringLiteral("删除帧"), right);
    IconProvider::apply(frameUp, AppIcon::Motion);
    IconProvider::apply(frameDown, AppIcon::Motion);
    IconProvider::apply(frameDelete, AppIcon::Delete);
    AppTheme::setButtonRole(frameDelete, QStringLiteral("danger"));
    frameButtons->addWidget(frameUp);
    frameButtons->addWidget(frameDown);
    frameButtons->addWidget(frameDelete);
    form->addRow(QStringLiteral("动作名称："), m_displayNameEdit);
    form->addRow(QStringLiteral("动作 ID："), m_actionIdEdit);
    form->addRow(QStringLiteral("英文名称："), m_englishNameEdit);
    form->addRow(QStringLiteral("说明："), m_descriptionEdit);
    form->addRow(QStringLiteral("系统角色："), m_roleCombo);
    form->addRow(QStringLiteral("素材来源："), m_sourceTypeCombo);
    form->addRow(QStringLiteral("FPS："), m_fpsSpin);
    form->addRow(m_loopCheck);
    form->addRow(QStringLiteral("播放次数："), m_playCountSpin);
    form->addRow(QStringLiteral("Next Action："), m_nextCombo);
    form->addRow(QStringLiteral("建议帧数："), m_expectedFrameCountSpin);
    form->addRow(QStringLiteral("Sprite Sheet 行："), m_sheetRowsSpin);
    form->addRow(QStringLiteral("Sprite Sheet 列："), m_sheetColumnsSpin);
    form->addRow(QStringLiteral("预览缩放："), m_previewScaleEdit);
    form->addRow(QStringLiteral("脚底基准偏移："), m_footBaselineOffsetSpin);
    form->addRow(m_allowHorizontalDisplacementCheck);
    form->addRow(m_mirrorSupportedCheck);
    form->addRow(m_showInMenuCheck);
    form->addRow(QStringLiteral("菜单分组："), m_menuGroupEdit);
    form->addRow(m_allowAiCheck);
    form->addRow(QStringLiteral("AI 允许状态："), m_aiAllowedStatesEdit);
    form->addRow(QStringLiteral("AI 参数 Schema："), m_aiParameterSchemaEdit);
    form->addRow(m_allowRandomCheck);
    form->addRow(QStringLiteral("随机权重："), m_randomWeightEdit);
    form->addRow(QStringLiteral("随机冷却 ms："), m_randomCooldownEdit);
    form->addRow(QStringLiteral("默认 Prompt："), m_defaultPromptEdit);
    form->addRow(QStringLiteral("Negative Prompt："), m_negativePromptEdit);
    form->addRow(QStringLiteral("帧列表："), m_frameList);
    form->addRow(frameButtons);
    auto *previewPanel = new QWidget(right);
    AppTheme::setRole(previewPanel, QStringLiteral("card"));
    auto *previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(14, 12, 14, 12);
    auto *previewTitle = new QLabel(QStringLiteral("动作预览"), previewPanel);
    AppTheme::setRole(previewTitle, QStringLiteral("sectionTitle"));
    m_preview = new PreviewCanvas(previewPanel);
    auto *previewButtons = new QHBoxLayout();
    auto *prevFrame = new QPushButton(QStringLiteral("上一帧"), previewPanel);
    m_playButton = new QPushButton(QStringLiteral("播放"), previewPanel);
    auto *nextFrameButton = new QPushButton(QStringLiteral("下一帧"), previewPanel);
    m_previousOnionCheck = new QCheckBox(QStringLiteral("上一帧洋葱皮"), previewPanel);
    m_nextOnionCheck = new QCheckBox(QStringLiteral("下一帧洋葱皮"), previewPanel);
    IconProvider::apply(m_playButton, AppIcon::Play);
    AppTheme::setButtonRole(m_playButton, QStringLiteral("primary"));
    previewButtons->addWidget(prevFrame);
    previewButtons->addWidget(m_playButton);
    previewButtons->addWidget(nextFrameButton);
    previewLayout->addWidget(previewTitle);
    previewLayout->addWidget(m_preview, 1);
    previewLayout->addLayout(previewButtons);
    previewLayout->addWidget(m_previousOnionCheck);
    previewLayout->addWidget(m_nextOnionCheck);
    auto *editorScroll = new QScrollArea(right);
    editorScroll->setFrameShape(QFrame::NoFrame);
    editorScroll->setWidgetResizable(true);
    editorScroll->setWidget(editor);
    editorScroll->setMinimumWidth(430);
    rightLayout->addWidget(editorScroll);
    rightLayout->addWidget(previewPanel, 1);
    splitter->addWidget(leftPanel);
    splitter->addWidget(right);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);
    auto *buttons = new QHBoxLayout();
    auto *add = new QPushButton(QStringLiteral("添加动作"), central);
    auto *copy = new QPushButton(QStringLiteral("复制动作"), central);
    auto *remove = new QPushButton(QStringLiteral("删除动作"), central);
    auto *import = new QPushButton(QStringLiteral("导入素材"), central);
    auto *quality = new QPushButton(QStringLiteral("质量检查"), central);
    auto *align = new QPushButton(QStringLiteral("自动对齐"), central);
    auto *save = new QPushButton(QStringLiteral("保存"), central);
    save->setObjectName(QStringLiteral("saveActionButton"));
    auto *saveRestart = new QPushButton(QStringLiteral("保存并重启桌宠"), central);
    IconProvider::apply(add, AppIcon::Add);
    IconProvider::apply(copy, AppIcon::Copy);
    IconProvider::apply(remove, AppIcon::Delete);
    IconProvider::apply(import, AppIcon::Import);
    IconProvider::apply(quality, AppIcon::Test);
    IconProvider::apply(align, AppIcon::Locate);
    IconProvider::apply(save, AppIcon::Save);
    IconProvider::apply(saveRestart, AppIcon::Refresh);
    AppTheme::setButtonRole(remove, QStringLiteral("danger"));
    AppTheme::setButtonRole(save, QStringLiteral("primary"));
    AppTheme::setButtonRole(saveRestart, QStringLiteral("coral"));
    buttons->addWidget(add);
    buttons->addWidget(copy);
    buttons->addWidget(remove);
    buttons->addWidget(import);
    buttons->addWidget(quality);
    buttons->addWidget(align);
    buttons->addStretch(1);
    buttons->addWidget(save);
    buttons->addWidget(saveRestart);
    root->addLayout(buttons);
    setCentralWidget(central);
    connect(m_actionList, &QListWidget::currentRowChanged, this, &ActionMaterialWindow::loadSelectedAction);
    connect(add, &QPushButton::clicked, this, &ActionMaterialWindow::addAction);
    connect(copy, &QPushButton::clicked, this, &ActionMaterialWindow::copyAction);
    connect(remove, &QPushButton::clicked, this, &ActionMaterialWindow::deleteAction);
    connect(save, &QPushButton::clicked, this, [this]() { saveCurrentAction(); });
    connect(saveRestart, &QPushButton::clicked, this, &ActionMaterialWindow::saveAndRestart);
    connect(import, &QPushButton::clicked, this, &ActionMaterialWindow::importMaterial);
    connect(quality, &QPushButton::clicked, this, &ActionMaterialWindow::runQualityCheck);
    connect(align, &QPushButton::clicked, this, &ActionMaterialWindow::autoAlignFrames);
    connect(frameUp, &QPushButton::clicked, this, &ActionMaterialWindow::moveFrameUp);
    connect(frameDown, &QPushButton::clicked, this, &ActionMaterialWindow::moveFrameDown);
    connect(frameDelete, &QPushButton::clicked, this, &ActionMaterialWindow::deleteFrame);
    connect(m_frameList, &QListWidget::currentRowChanged, this, &ActionMaterialWindow::refreshPreview);
    connect(prevFrame, &QPushButton::clicked, this, &ActionMaterialWindow::previousFrame);
    connect(nextFrameButton, &QPushButton::clicked, this, &ActionMaterialWindow::nextFrame);
    connect(m_playButton, &QPushButton::clicked, this, &ActionMaterialWindow::togglePreviewPlayback);
    connect(m_previousOnionCheck, &QCheckBox::toggled, this, &ActionMaterialWindow::refreshPreview);
    connect(m_nextOnionCheck, &QCheckBox::toggled, this, &ActionMaterialWindow::refreshPreview);
    connect(&m_previewTimer, &QTimer::timeout, this, &ActionMaterialWindow::nextFrame);
}

void ActionMaterialWindow::loadProject(const QString &petJsonPath)
{
    m_projectPath = petJsonPath;
    QString error;
    if (!m_project.load(petJsonPath, &error)) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), error);
        return;
    }
    m_projectLabel->setText(QStringLiteral("当前项目：%1").arg(m_project.name));
    m_projectLabel->setToolTip(petJsonPath);
    refreshActionList();
}

void ActionMaterialWindow::refreshActionList()
{
    m_actionList->clear();
    for (auto it = m_project.actions.constBegin(); it != m_project.actions.constEnd(); ++it) {
        const QString text = QStringLiteral("%1  [%2]").arg(it.value().displayName.isEmpty() ? it.key() : it.value().displayName, it.key());
        auto *item = new QListWidgetItem(text, m_actionList);
        item->setData(Qt::UserRole, it.key());
    }
    if (m_actionList->count() > 0) {
        m_actionList->setCurrentRow(0);
    }
}

QString ActionMaterialWindow::currentActionId() const
{
    QListWidgetItem *item = m_actionList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void ActionMaterialWindow::loadSelectedAction()
{
    const QString id = currentActionId();
    const PetAction action = m_project.actions.value(id);
    m_displayNameEdit->setText(action.displayName);
    m_englishNameEdit->setText(action.englishName);
    m_actionIdEdit->setText(id);
    m_descriptionEdit->setPlainText(action.description);
    m_roleCombo->setCurrentText(action.systemRole.isEmpty() ? QStringLiteral("None") : action.systemRole);
    m_sourceTypeCombo->setCurrentText(sourceTypeName(action.sourceType));
    m_fpsSpin->setValue(qMax(1, action.fps));
    m_loopCheck->setChecked(action.loop);
    m_playCountSpin->setValue(qMax(1, action.playCount));
    m_expectedFrameCountSpin->setValue(qMax(0, action.expectedFrameCount));
    m_sheetRowsSpin->setValue(qMax(1, action.spriteSheetRows));
    m_sheetColumnsSpin->setValue(qMax(1, action.spriteSheetColumns));
    m_previewScaleEdit->setText(QString::number(action.previewScale));
    m_footBaselineOffsetSpin->setValue(action.footBaselineOffset);
    m_allowHorizontalDisplacementCheck->setChecked(action.allowHorizontalDisplacement);
    m_mirrorSupportedCheck->setChecked(action.mirrorSupported);
    m_nextCombo->clear();
    m_nextCombo->addItem(QString(), QString());
    for (const QString &name : m_project.actions.keys()) {
        m_nextCombo->addItem(name, name);
    }
    const int nextIndex = m_nextCombo->findData(action.nextActionId);
    if (nextIndex >= 0) {
        m_nextCombo->setCurrentIndex(nextIndex);
    }
    m_showInMenuCheck->setChecked(action.showInContextMenu);
    m_menuGroupEdit->setText(action.menuGroup);
    m_allowAiCheck->setChecked(action.allowAiTrigger);
    m_aiAllowedStatesEdit->setText(QStringLiteral("Normal"));
    m_aiParameterSchemaEdit->setPlainText(action.aiParameterSchema.isEmpty()
                                              ? QString()
                                              : QString::fromUtf8(QJsonDocument(action.aiParameterSchema).toJson(QJsonDocument::Indented)));
    m_allowRandomCheck->setChecked(action.allowRandomTrigger);
    m_randomWeightEdit->setText(QString::number(action.randomWeight));
    m_randomCooldownEdit->setText(QString::number(action.randomCooldownMs));
    m_defaultPromptEdit->setPlainText(action.defaultPrompt);
    m_negativePromptEdit->setPlainText(action.negativePrompt);
    reloadFrameList(action);
    refreshPreview();
}

bool ActionMaterialWindow::saveCurrentAction()
{
    const QString id = currentActionId();
    if (!m_project.actions.contains(id)) {
        return false;
    }
    PetAction &action = m_project.actions[id];
    action.id = id;
    action.displayName = m_displayNameEdit->text().trimmed();
    action.englishName = m_englishNameEdit->text().trimmed();
    action.description = m_descriptionEdit->toPlainText().trimmed();
    const QString requestedRole = m_roleCombo->currentText();
    if (!requestedRole.isEmpty() && requestedRole != QStringLiteral("None")) {
        for (auto it = m_project.actions.begin(); it != m_project.actions.end(); ++it) {
            if (it.key() != id && it.value().systemRole == requestedRole) {
                const auto answer = QMessageBox::question(
                    this,
                    QStringLiteral("重新绑定系统角色"),
                    QStringLiteral("系统角色 %1 当前绑定：\n%2\n\n是否改绑到：\n%3")
                        .arg(requestedRole, it.key(), id),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No);
                if (answer != QMessageBox::Yes) {
                    return false;
                }
                it.value().systemRole = QStringLiteral("None");
                break;
            }
        }
    }
    action.systemRole = requestedRole;
    action.fps = m_fpsSpin->value();
    action.loop = m_loopCheck->isChecked();
    action.playCount = m_playCountSpin->value();
    action.expectedFrameCount = m_expectedFrameCountSpin->value();
    action.spriteSheetRows = m_sheetRowsSpin->value();
    action.spriteSheetColumns = m_sheetColumnsSpin->value();
    action.previewScale = m_previewScaleEdit->text().toDouble();
    action.footBaselineOffset = m_footBaselineOffsetSpin->value();
    action.allowHorizontalDisplacement = m_allowHorizontalDisplacementCheck->isChecked();
    action.mirrorSupported = m_mirrorSupportedCheck->isChecked();
    action.nextActionId = m_nextCombo->currentData().toString();
    action.next = action.nextActionId;
    action.showInContextMenu = m_showInMenuCheck->isChecked();
    action.menuGroup = m_menuGroupEdit->text().trimmed();
    action.allowAiTrigger = m_allowAiCheck->isChecked();
    action.aiAllowedStates = {QStringLiteral("Normal")};
    const QByteArray schemaData = m_aiParameterSchemaEdit->toPlainText().trimmed().toUtf8();
    if (schemaData.isEmpty()) {
        action.aiParameterSchema = {};
    } else {
        QJsonParseError schemaError;
        const QJsonDocument schemaDocument = QJsonDocument::fromJson(schemaData, &schemaError);
        if (schemaError.error != QJsonParseError::NoError || !schemaDocument.isObject()) {
            QMessageBox::warning(this,
                                 QStringLiteral("AI 参数 Schema 无效"),
                                 QStringLiteral("请输入有效的 JSON Object：%1").arg(schemaError.errorString()));
            return false;
        }
        action.aiParameterSchema = schemaDocument.object();
    }
    action.allowRandomTrigger = m_allowRandomCheck->isChecked();
    action.randomWeight = m_randomWeightEdit->text().toDouble();
    action.randomCooldownMs = m_randomCooldownEdit->text().toInt();
    action.defaultPrompt = m_defaultPromptEdit->toPlainText().trimmed();
    action.negativePrompt = m_negativePromptEdit->toPlainText().trimmed();
    writeFrameListToAction(action);
    QString error;
    if (!m_project.save(&error)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), error);
        return false;
    }
    refreshActionList();
    refreshPreview();
    return true;
}

QString ActionMaterialWindow::uniqueActionId() const
{
    QString id;
    do {
        id = QStringLiteral("action_%1").arg(QUuid::createUuid().toString(QUuid::Id128).left(8));
    } while (m_project.actions.contains(id));
    return id;
}

void ActionMaterialWindow::addAction()
{
    if (m_project.projectDir.isEmpty()) {
        return;
    }
    const QString id = uniqueActionId();
    PetAction action;
    action.id = id;
    action.name = id;
    action.displayName = QStringLiteral("新动作");
    action.fps = 8;
    action.systemRole = QStringLiteral("None");
    m_project.actions.insert(id, action);
    refreshActionList();
}

void ActionMaterialWindow::copyAction()
{
    const QString id = currentActionId();
    if (!m_project.actions.contains(id)) {
        return;
    }
    const QString newId = uniqueActionId();
    PetAction action = m_project.actions.value(id);
    action.id = newId;
    action.name = newId;
    action.displayName += QStringLiteral(" 副本");
    action.systemRole = QStringLiteral("None");
    action.sourceType = ActionSourceType::PngSequence;
    action.frames.clear();
    action.frameDurationsMs.clear();
    m_project.actions.insert(newId, action);
    refreshActionList();
    QMessageBox::information(this, QStringLiteral("已复制"), QStringLiteral("已复制动作配置，素材未复制。"));
}

void ActionMaterialWindow::deleteAction()
{
    const QString id = currentActionId();
    const QString role = m_project.actions.value(id).systemRole;
    if (role == QStringLiteral("Idle") || role == QStringLiteral("Dragging") || role == QStringLiteral("Falling")) {
        QMessageBox::warning(this, QStringLiteral("不能删除"), QStringLiteral("Required Role 动作不能直接删除，请先重新绑定角色。"));
        return;
    }
    for (const PetAction &action : m_project.actions) {
        if (action.nextActionId == id) {
            QMessageBox::warning(this, QStringLiteral("不能删除"), QStringLiteral("仍有动作的 Next Action 引用此动作。"));
            return;
        }
    }
    m_project.actions.remove(id);
    refreshActionList();
}

void ActionMaterialWindow::reloadFrameList(const PetAction &action)
{
    m_frameList->clear();
    for (int i = 0; i < action.frames.size(); ++i) {
        const PetFrame &frame = action.frames.at(i);
        auto *item = new QListWidgetItem(QStringLiteral("%1  %2").arg(i + 1, 4, 10, QChar('0')).arg(frame.path), m_frameList);
        item->setData(Qt::UserRole, frame.path);
        item->setData(Qt::UserRole + 1, frame.offset);
        const int duration = action.frameDurationsMs.size() == action.frames.size() ? action.frameDurationsMs.value(i) : 0;
        item->setData(Qt::UserRole + 2, duration);
    }
}

void ActionMaterialWindow::writeFrameListToAction(PetAction &action) const
{
    QVector<PetFrame> frames;
    QVector<int> durations;
    for (int i = 0; i < m_frameList->count(); ++i) {
        QListWidgetItem *item = m_frameList->item(i);
        PetFrame frame;
        frame.path = item->data(Qt::UserRole).toString();
        frame.offset = item->data(Qt::UserRole + 1).toPoint();
        if (!frame.path.isEmpty()) {
            frames.append(frame);
            const int duration = item->data(Qt::UserRole + 2).toInt();
            if (duration > 0) {
                durations.append(duration);
            }
        }
    }
    action.frames = frames;
    action.frameDurationsMs = durations.size() == frames.size() ? durations : QVector<int>();
}

void ActionMaterialWindow::moveFrameUp()
{
    const int row = m_frameList->currentRow();
    if (row <= 0) return;
    QListWidgetItem *item = m_frameList->takeItem(row);
    m_frameList->insertItem(row - 1, item);
    m_frameList->setCurrentRow(row - 1);
}

void ActionMaterialWindow::moveFrameDown()
{
    const int row = m_frameList->currentRow();
    if (row < 0 || row >= m_frameList->count() - 1) return;
    QListWidgetItem *item = m_frameList->takeItem(row);
    m_frameList->insertItem(row + 1, item);
    m_frameList->setCurrentRow(row + 1);
}

void ActionMaterialWindow::deleteFrame()
{
    delete m_frameList->takeItem(m_frameList->currentRow());
    refreshPreview();
}

void ActionMaterialWindow::refreshPreview()
{
    const QString id = currentActionId();
    if (!m_project.actions.contains(id) || !m_preview) {
        return;
    }
    PetAction &action = m_project.actions[id];
    writeFrameListToAction(action);
    const int row = qMax(0, m_frameList->currentRow());
    m_preview->setPreview(&m_project, &action, row);
    m_preview->setOnionSkin(m_previousOnionCheck && m_previousOnionCheck->isChecked(),
                            m_nextOnionCheck && m_nextOnionCheck->isChecked());
}

void ActionMaterialWindow::previousFrame()
{
    if (m_frameList->count() == 0) return;
    m_frameList->setCurrentRow(qMax(0, m_frameList->currentRow() - 1));
}

void ActionMaterialWindow::nextFrame()
{
    if (m_frameList->count() == 0) return;
    const int row = m_frameList->currentRow();
    if (row >= m_frameList->count() - 1) {
        if (m_loopCheck->isChecked()) {
            m_frameList->setCurrentRow(0);
        } else {
            m_previewTimer.stop();
            m_previewPlaying = false;
            m_playButton->setText(QStringLiteral("播放"));
        }
        return;
    }
    m_frameList->setCurrentRow(row + 1);
    if (m_previewPlaying) {
        m_previewTimer.start(previewFrameDurationMs());
    }
}

void ActionMaterialWindow::togglePreviewPlayback()
{
    m_previewPlaying = !m_previewPlaying;
    m_playButton->setText(m_previewPlaying ? QStringLiteral("暂停") : QStringLiteral("播放"));
    if (m_previewPlaying) {
        m_previewTimer.start(previewFrameDurationMs());
    } else {
        m_previewTimer.stop();
    }
}

int ActionMaterialWindow::previewFrameDurationMs() const
{
    const QString id = currentActionId();
    const PetAction action = m_project.actions.value(id);
    const int row = m_frameList ? m_frameList->currentRow() : 0;
    if (action.frameDurationsMs.size() == action.frames.size()
        && row >= 0 && row < action.frameDurationsMs.size()) {
        return qMax(1, action.frameDurationsMs.value(row));
    }
    return qMax(1, 1000 / qMax(1, m_fpsSpin->value()));
}

void ActionMaterialWindow::importMaterial()
{
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("导入素材"));
    box.setText(QStringLiteral("选择导入或生成方式"));
    auto *png = box.addButton(QStringLiteral("PNG 序列"), QMessageBox::ActionRole);
    auto *gif = box.addButton(QStringLiteral("GIF"), QMessageBox::ActionRole);
    auto *sheet = box.addButton(QStringLiteral("Sprite Sheet"), QMessageBox::ActionRole);
    auto *procedural = box.addButton(QStringLiteral("程序生成动作"), QMessageBox::ActionRole);
    auto *prompt = box.addButton(QStringLiteral("AI Prompt"), QMessageBox::ActionRole);
    auto *shimeji = box.addButton(QStringLiteral("Shimeji 向导"), QMessageBox::ActionRole);
    box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == png) {
        importPngSequence();
    } else if (box.clickedButton() == gif) {
        importGif();
    } else if (box.clickedButton() == sheet) {
        importSpriteSheet();
    } else if (box.clickedButton() == procedural) {
        importProceduralMotion();
    } else if (box.clickedButton() == prompt) {
        openAiPromptWizard();
    } else if (box.clickedButton() == shimeji) {
        openShimejiWizard();
    }
}

void ActionMaterialWindow::importPngSequence()
{
    const QString id = currentActionId();
    if (id.isEmpty()) return;
    const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("导入 PNG 序列"), QString(), QStringLiteral("PNG (*.png)"));
    if (files.isEmpty()) return;
    QString error;
    if (!saveCurrentAction()) {
        return;
    }
    if (!m_project.importPngFrames(id, files, &error)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), error);
        return;
    }
    loadProject(m_project.petJsonPath());
}

void ActionMaterialWindow::importGif()
{
    const QString id = currentActionId();
    if (id.isEmpty()) return;
    const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("导入 GIF"), QString(), QStringLiteral("GIF (*.gif)"));
    if (file.isEmpty()) return;
    QString error;
    if (!saveCurrentAction()) {
        return;
    }
    if (!m_project.importGifFrames(id, file, 1.0, &error)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), error);
        return;
    }
    loadProject(m_project.petJsonPath());
}

void ActionMaterialWindow::importSpriteSheet()
{
    const QString id = currentActionId();
    if (id.isEmpty()) return;
    const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("\u5bfc\u5165 Sprite Sheet"), QString(), QStringLiteral("Images (*.png *.webp *.jpg *.jpeg)"));
    if (file.isEmpty()) return;
    SpriteSheetImportDialog dialog(file, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const SpriteSheetSliceOptions options = dialog.options();

    const QString transactionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString targetDir = QDir(m_project.projectDir).filePath(QStringLiteral(".asset_staging/spritesheet_%1/%2")
                                                                       .arg(transactionId, id));
    QString error;
    const QStringList frameFiles = SpriteSheetSlicer::sliceToPngFrames(file, targetDir, options, &error);
    if (frameFiles.isEmpty()) {
        QDir(QFileInfo(targetDir).absolutePath()).removeRecursively();
        QMessageBox::warning(this, QStringLiteral("\u5bfc\u5165\u5931\u8d25"), error);
        return;
    }

    ImportFramesOptions importOptions;
    importOptions.sourceType = ActionSourceType::SpriteSheet;
    importOptions.hasSpriteSheetMetadata = true;
    importOptions.spriteSheetMetadata.sourceFileName = QFileInfo(file).fileName();
    importOptions.spriteSheetMetadata.rows = options.rows;
    importOptions.spriteSheetMetadata.columns = options.columns;
    importOptions.spriteSheetMetadata.frameWidth = options.frameWidth;
    importOptions.spriteSheetMetadata.frameHeight = options.frameHeight;
    importOptions.spriteSheetMetadata.marginLeft = options.marginLeft;
    importOptions.spriteSheetMetadata.marginTop = options.marginTop;
    importOptions.spriteSheetMetadata.marginRight = options.marginRight;
    importOptions.spriteSheetMetadata.marginBottom = options.marginBottom;
    importOptions.spriteSheetMetadata.spacingX = options.spacingX;
    importOptions.spriteSheetMetadata.spacingY = options.spacingY;
    importOptions.spriteSheetMetadata.readingOrder = options.readingOrder == SpriteSheetReadingOrder::LeftToRightTopToBottom
        ? QStringLiteral("LeftToRightTopToBottom")
        : QStringLiteral("TopToBottomLeftToRight");
    importOptions.spriteSheetMetadata.startFrame = options.startFrame;
    importOptions.spriteSheetMetadata.maxFrames = options.maxFrames;
    importOptions.spriteSheetMetadata.skipTransparentFrames = options.skipTransparentFrames;
    if (!m_project.importPngFrames(id, frameFiles, importOptions, &error)) {
        QDir(QFileInfo(targetDir).absolutePath()).removeRecursively();
        QMessageBox::warning(this, QStringLiteral("\u5bfc\u5165\u5931\u8d25"), error);
        return;
    }
    QDir(QFileInfo(targetDir).absolutePath()).removeRecursively();
    loadProject(m_project.petJsonPath());
}

void ActionMaterialWindow::importProceduralMotion()
{
    const QString id = currentActionId();
    if (id.isEmpty()) return;

    const QString source = QFileDialog::getOpenFileName(this,
                                                        QStringLiteral("\u9009\u62e9\u6e90 PNG"),
                                                        QString(),
                                                        QStringLiteral("PNG (*.png)"));
    if (source.isEmpty()) return;

    bool ok = false;
    const int frameCount = QInputDialog::getInt(this, QStringLiteral("\u7a0b\u5e8f\u751f\u6210\u52a8\u4f5c"), QStringLiteral("\u5e27\u6570"), 6, 2, 64, 1, &ok);
    if (!ok) return;

    const QStringList presetIds {
        QStringLiteral("idle_breathe"),
        QStringLiteral("hop_left"),
        QStringLiteral("hop_right"),
        QStringLiteral("nod"),
        QStringLiteral("shake_head"),
        QStringLiteral("sleep_breathe"),
        QStringLiteral("rhythm_step"),
        QStringLiteral("peek"),
        QStringLiteral("look_far"),
        QStringLiteral("adjust_clothes"),
        QStringLiteral("hands_behind_sway"),
        QStringLiteral("listen"),
    };
    const QStringList presetLabels {
        QStringLiteral("\u5f85\u673a\u547c\u5438"),
        QStringLiteral("\u5de6\u8df3"),
        QStringLiteral("\u53f3\u8df3"),
        QStringLiteral("\u70b9\u5934"),
        QStringLiteral("\u6447\u5934"),
        QStringLiteral("\u7761\u7720\u547c\u5438"),
        QStringLiteral("节奏踏步"),
        QStringLiteral("侧身探头"),
        QStringLiteral("抬手远望"),
        QStringLiteral("整理衣服"),
        QStringLiteral("双手背后摇摆"),
        QStringLiteral("侧耳倾听"),
    };
    const QString selectedPresetLabel = QInputDialog::getItem(this,
                                                 QStringLiteral("\u7a0b\u5e8f\u751f\u6210\u52a8\u4f5c"),
                                                 QStringLiteral("\u52a8\u4f5c\u9884\u8bbe"),
                                                 presetLabels,
                                                 0,
                                                 false,
                                                 &ok);
    if (!ok || selectedPresetLabel.isEmpty()) return;
    const int presetIndex = presetLabels.indexOf(selectedPresetLabel);
    const QString preset = presetIndex >= 0 ? presetIds.at(presetIndex) : QStringLiteral("idle_breathe");

    const QString transactionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString targetDir = QDir(m_project.projectDir).filePath(QStringLiteral(".asset_staging/procedural_%1/%2")
                                                                       .arg(transactionId, id));
    QString error;
    const QStringList generated = ProceduralMotionGenerator::generateFrames(source,
                                                                            targetDir,
                                                                            preset,
                                                                            QStringLiteral("normal"),
                                                                            frameCount,
                                                                            &error);
    if (generated.isEmpty()) {
        QDir(QFileInfo(targetDir).absolutePath()).removeRecursively();
        QMessageBox::warning(this, QStringLiteral("\u5bfc\u5165\u5931\u8d25"), error);
        return;
    }

    ImportFramesOptions importOptions;
    importOptions.sourceType = ActionSourceType::ProceduralGenerated;
    if (!m_project.importPngFrames(id, generated, importOptions, &error)) {
        QDir(QFileInfo(targetDir).absolutePath()).removeRecursively();
        QMessageBox::warning(this, QStringLiteral("\u5bfc\u5165\u5931\u8d25"), error);
        return;
    }
    QDir(QFileInfo(targetDir).absolutePath()).removeRecursively();
    loadProject(m_project.petJsonPath());
}

void ActionMaterialWindow::openAiPromptWizard()
{
    const QString id = currentActionId();
    const PetAction action = m_project.actions.value(id);
    const int frames = action.expectedFrameCount > 0
        ? action.expectedFrameCount
        : (m_frameList && m_frameList->count() > 0 ? m_frameList->count() : 8);
    const int rows = action.spriteSheetRows > 0 ? action.spriteSheetRows : 2;
    const int columns = action.spriteSheetColumns > 0 ? action.spriteSheetColumns : qMax(1, (frames + rows - 1) / rows);
    auto *wizard = new AIAssetPromptWizard(id, frames, rows, columns, this);
    wizard->setAttribute(Qt::WA_DeleteOnClose);
    wizard->show();
}

void ActionMaterialWindow::openShimejiWizard()
{
    if (m_project.projectDir.isEmpty()) {
        return;
    }

    const QString imageSetDir = QFileDialog::getExistingDirectory(this, QStringLiteral("\u9009\u62e9 Shimeji \u56fe\u7247\u76ee\u5f55"));
    if (imageSetDir.isEmpty()) {
        return;
    }
    const QString actionsXml = QFileDialog::getOpenFileName(this,
                                                            QStringLiteral("\u9009\u62e9 actions.xml"),
                                                            imageSetDir,
                                                            QStringLiteral("XML (*.xml)"));
    if (actionsXml.isEmpty()) {
        return;
    }

    QStringList unsupported;
    const QVector<ShimejiActionImport> actions = ShimejiImportWizard::parseActions(actionsXml, &unsupported);
    if (actions.isEmpty()) {
        const QString xmlError = unsupported.filter(QStringLiteral("XML_ERROR")).join('\n');
        QMessageBox::warning(this,
                             QStringLiteral("Shimeji \u5bfc\u5165"),
                             xmlError.isEmpty()
                                 ? QStringLiteral("\u6ca1\u6709\u627e\u5230\u53ef\u5bfc\u5165\u7684 Shimeji \u52a8\u4f5c\u3002")
                                 : QStringLiteral("actions.xml \u89e3\u6790\u5931\u8d25\uff1a\n%1").arg(xmlError));
        return;
    }

    QStringList labels;
    labels << QStringLiteral("\u5168\u90e8\u52a8\u4f5c");
    for (const ShimejiActionImport &action : actions) {
        labels << QStringLiteral("%1 (%2, %3 \u5e27)").arg(action.name, action.type).arg(action.poses.size());
    }

    bool ok = false;
    const QString selected = QInputDialog::getItem(this,
                                                   QStringLiteral("Shimeji \u5bfc\u5165"),
                                                   QStringLiteral("\u5bfc\u5165\u52a8\u4f5c"),
                                                   labels,
                                                   0,
                                                   false,
                                                   &ok);
    if (!ok) {
        return;
    }

    const QString role = QInputDialog::getItem(this,
                                               QStringLiteral("Shimeji \u5bfc\u5165"),
                                               QStringLiteral("\u7cfb\u7edf\u89d2\u8272"),
                                               QStringList() << QStringLiteral("None")
                                                             << QStringLiteral("Idle")
                                                             << QStringLiteral("WalkLeft")
                                                             << QStringLiteral("WalkRight")
                                                             << QStringLiteral("Dragging")
                                                             << QStringLiteral("Falling")
                                                             << QStringLiteral("Landing")
                                                             << QStringLiteral("Sleeping"),
                                               0,
                                               false,
                                               &ok);
    if (!ok) {
        return;
    }

    const int selectedIndex = labels.indexOf(selected);
    const bool importAll = selectedIndex == 0;
    const QString effectiveRole = importAll ? QStringLiteral("None") : role;
    QVector<ShimejiActionImport> selectedActions;
    if (importAll) selectedActions = actions;
    else if (selectedIndex > 0 && selectedIndex <= actions.size()) selectedActions.append(actions.at(selectedIndex - 1));

    const QString behaviorsXml = QFileInfo(actionsXml).absoluteDir().filePath(QStringLiteral("behaviors.xml"));
    QVector<ShimejiBehaviorImport> behaviors;
    if (QFileInfo::exists(behaviorsXml)) {
        QStringList behaviorUnsupported;
        behaviors = ShimejiImportWizard::parseBehaviors(behaviorsXml, &behaviorUnsupported);
        unsupported.append(behaviorUnsupported);
        const QString behaviorXmlError = behaviorUnsupported.filter(QStringLiteral("XML_ERROR")).join('\n');
        if (!behaviorXmlError.isEmpty()) {
            QMessageBox::warning(this,
                                 QStringLiteral("Shimeji \u5bfc\u5165\u5931\u8d25"),
                                 QStringLiteral("behaviors.xml \u89e3\u6790\u5931\u8d25\uff1a\n%1").arg(behaviorXmlError));
            return;
        }
    }

    const ShimejiBatchImportResult importResult = ShimejiImportWizard::importPackageToProject(
        m_project, imageSetDir, selectedActions, behaviors, effectiveRole);
    if (!importResult.ok) {
        QMessageBox::warning(this, QStringLiteral("Shimeji \u5bfc\u5165\u5931\u8d25"), importResult.errorMessage);
        return;
    }
    unsupported.append(importResult.warnings);

    loadProject(m_project.petJsonPath());
    QString message = QStringLiteral("\u5df2\u5bfc\u5165 %1 \u4e2a Shimeji \u52a8\u4f5c\uff0c%2 \u6761\u884c\u4e3a\u89c4\u5219\u3002")
                          .arg(importResult.actionsImported).arg(importResult.behaviorsImported);
    if (!unsupported.isEmpty()) {
        message += QStringLiteral("\n\n\u672a\u5b8c\u5168\u652f\u6301\uff1a\n%1").arg(unsupported.join('\n'));
    }
    QMessageBox::information(this, QStringLiteral("Shimeji \u5bfc\u5165\u5b8c\u6210"), message);
}

void ActionMaterialWindow::runQualityCheck()
{
    const QString id = currentActionId();
    if (!m_project.actions.contains(id)) return;
    QStringList paths;
    for (const PetFrame &frame : m_project.actions.value(id).frames) {
        paths.append(m_project.absolutePathFor(frame.path));
    }
    const AssetQualityReport report = AssetQualityAnalyzer::analyzeFrames(paths);
    QStringList lines;
    lines << QStringLiteral("帧数：%1").arg(paths.size())
          << QStringLiteral("全部可读：%1").arg(report.allFramesReadable ? QStringLiteral("是") : QStringLiteral("否"))
          << QStringLiteral("画布一致：%1").arg(report.canvasSizeConsistent ? QStringLiteral("是") : QStringLiteral("否"))
          << QStringLiteral("宽度变化：%1%").arg(report.maxWidthVariationPercent, 0, 'f', 2)
          << QStringLiteral("高度变化：%1%").arg(report.maxHeightVariationPercent, 0, 'f', 2)
          << QStringLiteral("中心 X 漂移：%1").arg(report.maxCenterXDrift)
          << QStringLiteral("底部 Y 漂移：%1").arg(report.maxBottomYDrift);
    lines.append(report.warnings);
    QMessageBox::information(this, QStringLiteral("质量检查"), lines.join('\n'));
}

void ActionMaterialWindow::autoAlignFrames()
{
    const QString id = currentActionId();
    if (!m_project.actions.contains(id)) return;
    PetAction &action = m_project.actions[id];
    if (action.frames.isEmpty()) return;
    QRect reference;
    for (const PetFrame &frame : action.frames) {
        const QRect bounds = AlphaBoundingBoxUtil::opaqueBounds(QImage(m_project.absolutePathFor(frame.path)));
        if (!bounds.isNull()) {
            reference = bounds;
            break;
        }
    }
    if (reference.isNull()) {
        QMessageBox::warning(this, QStringLiteral("自动对齐"), QStringLiteral("没有找到可见像素帧。"));
        return;
    }
    const QPoint refCenter(reference.center().x(), reference.bottom());
    for (PetFrame &frame : action.frames) {
        const QRect bounds = AlphaBoundingBoxUtil::opaqueBounds(QImage(m_project.absolutePathFor(frame.path)));
        if (!bounds.isNull()) {
            const QPoint current(bounds.center().x(), bounds.bottom());
            frame.autoOffset = refCenter - current;
        }
    }
    reloadFrameList(action);
    refreshPreview();
}

void ActionMaterialWindow::saveAndRestart()
{
    if (!saveCurrentAction()) {
        return;
    }
    if (m_runtimeManager && !m_projectPath.isEmpty() && m_runtimeManager->isPetRunning(m_projectPath)) {
        const QString path = m_projectPath;
        QMetaObject::Connection *connection = new QMetaObject::Connection;
        *connection = connect(m_runtimeManager, &RuntimePetManager::petStarted, this, [this, path, connection](const QString &startedPath) {
            if (QDir::cleanPath(startedPath) != QDir::cleanPath(path)) {
                return;
            }
            QMessageBox::information(this, QStringLiteral("已保存"), QStringLiteral("已保存并重新启动桌宠。"));
            disconnect(*connection);
            delete connection;
        });
        if (!m_runtimeManager->restartPet(path)) {
            disconnect(*connection);
            delete connection;
            QMessageBox::warning(this, QStringLiteral("重启失败"), QStringLiteral("保存成功，但桌宠重新启动失败。"));
        }
    }
}

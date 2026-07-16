#include "petcontrolcenterwindow.h"

#include "actionmaterialwindow.h"
#include "aiconversationconsolewindow.h"
#include "aiprovider.h"
#include "aiproviderprofileregistry.h"
#include "aiproviderprofileservice.h"
#include "credentialstore.h"
#include "editorwindow.h"
#include "petruntimeinstance.h"
#include "runtimepetwindow.h"
#include "renderbackend.h"
#include "toolintegrationmanager.h"
#include "ui/theme/apptheme.h"
#include "ui/theme/iconprovider.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QInputDialog>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QUuid>

namespace {
void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

QString safeDirName(QString name)
{
    name = name.trimmed().toLower();
    name.replace(QRegularExpression(QStringLiteral("[^a-z0-9_-]+")), QStringLiteral("_"));
    name.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    name = name.trimmed();
    return name.isEmpty() ? QStringLiteral("pet") : name;
}

bool copyDirectory(const QString &sourceDir, const QString &targetDir, QString *error)
{
    QDir source(sourceDir);
    if (!source.exists()) {
        if (error) *error = QStringLiteral("Source directory does not exist: %1").arg(sourceDir);
        return false;
    }
    QDir().mkpath(targetDir);
    QDirIterator it(sourceDir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString sourceFile = it.next();
        const QString relative = source.relativeFilePath(sourceFile);
        const QString targetFile = QDir(targetDir).filePath(relative);
        QDir().mkpath(QFileInfo(targetFile).absolutePath());
        QFile::remove(targetFile);
        if (!QFile::copy(sourceFile, targetFile)) {
            if (error) *error = QStringLiteral("Failed to copy %1").arg(relative);
            return false;
        }
    }
    return true;
}

void addPageHeading(QVBoxLayout *layout, const QString &title, const QString &description)
{
    auto *heading = new QWidget();
    auto *headingLayout = new QVBoxLayout(heading);
    headingLayout->setContentsMargins(0, 0, 0, 8);
    headingLayout->setSpacing(4);
    auto *titleLabel = new QLabel(title, heading);
    auto *descriptionLabel = new QLabel(description, heading);
    descriptionLabel->setWordWrap(true);
    AppTheme::setRole(titleLabel, QStringLiteral("pageTitle"));
    AppTheme::setRole(descriptionLabel, QStringLiteral("muted"));
    headingLayout->addWidget(titleLabel);
    headingLayout->addWidget(descriptionLabel);
    layout->addWidget(heading);
}

void preparePage(QWidget *page, QVBoxLayout *layout)
{
    page->setObjectName(QStringLiteral("controlCenterPage"));
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);
}

void prepareScrollArea(QScrollArea *scroll)
{
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
}
}

PetControlCenterWindow::PetControlCenterWindow(RuntimePetManager *manager, QWidget *parent)
    : QMainWindow(parent)
    , m_manager(manager)
{
    setupUi();
    restoreGeometry(QSettings().value(QStringLiteral("ui/controlCenterGeometry")).toByteArray());
    connect(m_manager, &RuntimePetManager::petStarted, this, [this](const QString &) { refreshAll(); });
    connect(m_manager, static_cast<void (RuntimePetManager::*)(const QString &)>(&RuntimePetManager::petStopped),
            this, [this](const QString &) { refreshAll(); });
    connect(m_manager, &RuntimePetManager::runningPetsChanged, this, &PetControlCenterWindow::refreshAll);
    refreshAll();
}

void PetControlCenterWindow::showCenter()
{
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    show();
    const QRect currentGeometry = frameGeometry();
    QScreen *screen = QGuiApplication::screenAt(currentGeometry.center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1536, 912);
    if (!available.intersects(currentGeometry) || currentGeometry.width() < 400 || currentGeometry.height() < 300) {
        resize(1080, 720);
        move(available.center() - rect().center());
    }
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    show();
    raise();
    activateWindow();
    QTimer::singleShot(500, this, [this]() {
        setWindowFlag(Qt::WindowStaysOnTopHint, false);
        show();
        raise();
        activateWindow();
    });
}

void PetControlCenterWindow::closeEvent(QCloseEvent *event)
{
    QSettings().setValue(QStringLiteral("ui/controlCenterGeometry"), saveGeometry());
    hide();
    event->ignore();
}

void PetControlCenterWindow::setupUi()
{
    setWindowTitle(QStringLiteral("Desktop Pet Maker - 桌宠管理中心"));
    setMinimumSize(960, 640);
    resize(1180, 760);
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QWidget(central);
    AppTheme::setRole(header, QStringLiteral("appHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(24, 12, 24, 12);
    headerLayout->setSpacing(12);
    auto *brandIcon = new QLabel(header);
    brandIcon->setFixedSize(42, 42);
    brandIcon->setPixmap(QIcon(QStringLiteral(":/branding/app_icon.png")).pixmap(42, 42));
    auto *brandText = new QWidget(header);
    auto *brandTextLayout = new QVBoxLayout(brandText);
    brandTextLayout->setContentsMargins(0, 0, 0, 0);
    brandTextLayout->setSpacing(0);
    auto *brandTitle = new QLabel(QStringLiteral("Desktop Pet Maker"), brandText);
    auto *brandSubtitle = new QLabel(QStringLiteral("桌宠工坊 · 创作、运行与 AI 互动"), brandText);
    AppTheme::setRole(brandTitle, QStringLiteral("brandTitle"));
    AppTheme::setRole(brandSubtitle, QStringLiteral("muted"));
    brandTextLayout->addWidget(brandTitle);
    brandTextLayout->addWidget(brandSubtitle);
    auto *version = new QLabel(QStringLiteral("1.0.0-beta"), header);
    AppTheme::setRole(version, QStringLiteral("status"));
    version->setProperty("status", QStringLiteral("info"));
    headerLayout->addWidget(brandIcon);
    headerLayout->addWidget(brandText);
    headerLayout->addStretch(1);
    headerLayout->addWidget(version);
    root->addWidget(header);

    auto *body = new QWidget(central);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    auto *sidebar = new QWidget(body);
    AppTheme::setRole(sidebar, QStringLiteral("sidebar"));
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(8, 12, 8, 12);
    sidebarLayout->setSpacing(8);
    m_navList = new QListWidget(sidebar);
    m_navList->setObjectName(QStringLiteral("controlCenterNavList"));
    AppTheme::setRole(m_navList, QStringLiteral("navigation"));
    const QList<QPair<QString, AppIcon>> navigation {
        {QStringLiteral("总览"), AppIcon::Home},
        {QStringLiteral("我的桌宠"), AppIcon::Pet},
        {QStringLiteral("动作与素材"), AppIcon::Motion},
        {QStringLiteral("AI 对话台"), AppIcon::Chat},
        {QStringLiteral("AI 角色"), AppIcon::Role},
        {QStringLiteral("AI 服务"), AppIcon::Cloud},
        {QStringLiteral("运行设置"), AppIcon::Runtime},
        {QStringLiteral("素材工具"), AppIcon::Tools},
        {QStringLiteral("通用设置"), AppIcon::Settings}
    };
    for (const auto &entry : navigation) {
        auto *item = new QListWidgetItem(IconProvider::icon(entry.second), entry.first, m_navList);
        item->setSizeHint(QSize(188, 44));
    }
    m_navList->setFixedWidth(212);
    sidebarLayout->addWidget(m_navList, 1);
    auto *sidebarHint = new QLabel(QStringLiteral("所有配置均保存在本机"), sidebar);
    sidebarHint->setAlignment(Qt::AlignCenter);
    AppTheme::setRole(sidebarHint, QStringLiteral("caption"));
    sidebarLayout->addWidget(sidebarHint);

    m_pages = new QStackedWidget(body);
    m_pages->setObjectName(QStringLiteral("controlCenterPages"));
    m_pages->addWidget(createOverviewPage());
    m_pages->addWidget(createMyPetsPage());
    m_pages->addWidget(createActionsPage());
    m_pages->addWidget(createAiConsolePage());
    m_pages->addWidget(createAiRolePage());
    m_pages->addWidget(createAiServicePage());
    m_pages->addWidget(createRuntimeSettingsPage());
    m_pages->addWidget(createAssetToolsPage());
    m_pages->addWidget(createGeneralSettingsPage());
    bodyLayout->addWidget(sidebar);
    bodyLayout->addWidget(m_pages, 1);
    root->addWidget(body, 1);
    setCentralWidget(central);
    connect(m_navList, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_navList->setCurrentRow(0);
}

QWidget *PetControlCenterWindow::createOverviewPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    preparePage(page, layout);
    addPageHeading(layout, QStringLiteral("运行总览"),
                   QStringLiteral("查看正在运行的桌宠，并快速进入聊天、定位或停止实例。"));
    auto *summary = new QFrame(page);
    AppTheme::setRole(summary, QStringLiteral("card"));
    auto *summaryLayout = new QHBoxLayout(summary);
    summaryLayout->setContentsMargins(18, 14, 18, 14);
    auto *summaryLabels = new QVBoxLayout();
    summaryLabels->setSpacing(0);
    auto *summaryText = new QLabel(QStringLiteral("当前运行实例"), summary);
    AppTheme::setRole(summaryText, QStringLiteral("muted"));
    m_runningCountLabel = new QLabel(summary);
    AppTheme::setRole(m_runningCountLabel, QStringLiteral("metric"));
    summaryLabels->addWidget(summaryText);
    summaryLabels->addWidget(m_runningCountLabel);
    summaryLayout->addLayout(summaryLabels);
    summaryLayout->addStretch(1);
    auto *startButton = new QPushButton(QStringLiteral("启动选中桌宠"), summary);
    auto *stopAllButton = new QPushButton(QStringLiteral("全部停止"), summary);
    startButton->setObjectName(QStringLiteral("startSelectedPetButton"));
    stopAllButton->setObjectName(QStringLiteral("stopAllPetsButton"));
    AppTheme::setButtonRole(startButton, QStringLiteral("primary"));
    AppTheme::setButtonRole(stopAllButton, QStringLiteral("danger"));
    IconProvider::apply(startButton, AppIcon::Play);
    IconProvider::apply(stopAllButton, AppIcon::Stop);
    summaryLayout->addWidget(startButton);
    summaryLayout->addWidget(stopAllButton);
    layout->addWidget(summary);
    auto *scroll = new QScrollArea(page);
    prepareScrollArea(scroll);
    auto *container = new QWidget(scroll);
    m_overviewLayout = new QVBoxLayout(container);
    m_overviewLayout->setContentsMargins(0, 0, 0, 0);
    m_overviewLayout->setSpacing(10);
    scroll->setWidget(container);
    layout->addWidget(scroll, 1);
    connect(startButton, &QPushButton::clicked, this, [this]() { if (!selectedPetPath().isEmpty()) m_manager->startPet(selectedPetPath()); });
    connect(stopAllButton, &QPushButton::clicked, m_manager, &RuntimePetManager::stopAllPets);
    return page;
}

QWidget *PetControlCenterWindow::createMyPetsPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    preparePage(page, layout);
    addPageHeading(layout, QStringLiteral("我的桌宠"),
                   QStringLiteral("管理桌宠项目、素材入口和运行状态。"));
    auto *toolbar = new QHBoxLayout();
    auto *create = new QPushButton(QStringLiteral("创建桌宠"), page);
    auto *importProject = new QPushButton(QStringLiteral("导入项目"), page);
    auto *importPack = new QPushButton(QStringLiteral("导入 petpack"), page);
    auto *refresh = new QPushButton(QStringLiteral("刷新"), page);
    AppTheme::setButtonRole(create, QStringLiteral("primary"));
    IconProvider::apply(create, AppIcon::Add);
    IconProvider::apply(importProject, AppIcon::Import);
    IconProvider::apply(importPack, AppIcon::Import);
    IconProvider::apply(refresh, AppIcon::Refresh);
    toolbar->addWidget(create);
    toolbar->addWidget(importProject);
    toolbar->addWidget(importPack);
    toolbar->addWidget(refresh);
    toolbar->addStretch(1);
    layout->addLayout(toolbar);
    auto *scroll = new QScrollArea(page);
    prepareScrollArea(scroll);
    auto *container = new QWidget(scroll);
    m_myPetsLayout = new QVBoxLayout(container);
    m_myPetsLayout->setContentsMargins(0, 0, 0, 0);
    m_myPetsLayout->setSpacing(10);
    scroll->setWidget(container);
    layout->addWidget(scroll, 1);
    connect(refresh, &QPushButton::clicked, this, &PetControlCenterWindow::refreshMyPets);
    connect(create, &QPushButton::clicked, this, &PetControlCenterWindow::createPetProject);
    connect(importProject, &QPushButton::clicked, this, &PetControlCenterWindow::importPetProject);
    connect(importPack, &QPushButton::clicked, this, &PetControlCenterWindow::importPetpack);
    return page;
}

QWidget *PetControlCenterWindow::createActionsPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    preparePage(page, layout);
    addPageHeading(layout, QStringLiteral("动作与素材"),
                   QStringLiteral("编辑动作属性、帧序列、AI 触发规则与预览参数。"));
    auto *card = new QFrame(page);
    AppTheme::setRole(card, QStringLiteral("card"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 18, 20, 18);
    auto *title = new QLabel(QStringLiteral("动作素材工作台"), card);
    auto *description = new QLabel(QStringLiteral("集中管理动作 ID、显示名称、系统角色、素材来源、菜单与 AI/随机触发。"), card);
    description->setWordWrap(true);
    AppTheme::setRole(title, QStringLiteral("sectionTitle"));
    AppTheme::setRole(description, QStringLiteral("muted"));
    auto *open = new QPushButton(QStringLiteral("打开动作与素材管理"), card);
    AppTheme::setButtonRole(open, QStringLiteral("primary"));
    IconProvider::apply(open, AppIcon::Motion);
    cardLayout->addWidget(title);
    cardLayout->addWidget(description);
    cardLayout->addWidget(open, 0, Qt::AlignLeft);
    layout->addWidget(card);
    layout->addStretch(1);
    connect(open, &QPushButton::clicked, this, &PetControlCenterWindow::openActionMaterial);
    return page;
}

QWidget *PetControlCenterWindow::createAiConsolePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    preparePage(page, layout);
    addPageHeading(layout, QStringLiteral("AI 多角色对话台"),
                   QStringLiteral("组织定向对话、自由群聊和轮流发言，并把回复投递到桌面角色。"));
    auto *card = new QFrame(page);
    AppTheme::setRole(card, QStringLiteral("card"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 18, 20, 18);
    auto *title = new QLabel(QStringLiteral("多角色会话空间"), card);
    auto *description = new QLabel(QStringLiteral("房间、参与角色、发言权重和运行时动作投递都在独立工作台中完成。"), card);
    AppTheme::setRole(title, QStringLiteral("sectionTitle"));
    AppTheme::setRole(description, QStringLiteral("muted"));
    auto *open = new QPushButton(QStringLiteral("打开 AI 多角色对话台"), card);
    AppTheme::setButtonRole(open, QStringLiteral("primary"));
    IconProvider::apply(open, AppIcon::Chat);
    cardLayout->addWidget(title);
    cardLayout->addWidget(description);
    cardLayout->addWidget(open, 0, Qt::AlignLeft);
    layout->addWidget(card);
    layout->addStretch(1);
    connect(open, &QPushButton::clicked, this, &PetControlCenterWindow::openAiConsole);
    return page;
}

QWidget *PetControlCenterWindow::createAiRolePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    preparePage(page, layout);
    addPageHeading(layout, QStringLiteral("AI 角色"),
                   QStringLiteral("为当前桌宠设置独立身份、系统提示词与服务配置。"));
    auto *card = new QFrame(page);
    AppTheme::setRole(card, QStringLiteral("card"));
    auto *form = new QFormLayout(card);
    form->setContentsMargins(20, 18, 20, 18);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(12);
    m_rolePetLabel = new QLabel(card);
    m_roleNameEdit = new QLineEdit(card);
    m_rolePromptEdit = new QTextEdit(card);
    m_rolePromptEdit->setMinimumHeight(150);
    m_roleProviderCombo = new QComboBox(card);
    m_saveRoleButton = new QPushButton(QStringLiteral("保存角色配置"), card);
    AppTheme::setButtonRole(m_saveRoleButton, QStringLiteral("primary"));
    IconProvider::apply(m_saveRoleButton, AppIcon::Save);
    form->addRow(QStringLiteral("当前项目："), m_rolePetLabel);
    form->addRow(QStringLiteral("角色名称："), m_roleNameEdit);
    form->addRow(QStringLiteral("系统提示词："), m_rolePromptEdit);
    form->addRow(QStringLiteral("AI Provider Profile："), m_roleProviderCombo);
    form->addRow(m_saveRoleButton);
    layout->addWidget(card);
    layout->addStretch(1);
    connect(m_saveRoleButton, &QPushButton::clicked, this, &PetControlCenterWindow::saveAiRole);
    return page;
}

QWidget *PetControlCenterWindow::createAiServicePage()
{
    auto *page = new QWidget(this);
    auto *root = new QVBoxLayout(page);
    preparePage(page, root);
    addPageHeading(root, QStringLiteral("AI 服务"),
                   QStringLiteral("管理 OpenAI-compatible Provider。密钥仅保存在 Windows 凭据存储中。"));
    auto *card = new QFrame(page);
    AppTheme::setRole(card, QStringLiteral("card"));
    auto *content = new QHBoxLayout(card);
    content->setContentsMargins(18, 18, 18, 18);
    content->setSpacing(18);
    auto *left = new QVBoxLayout();
    auto *profileTitle = new QLabel(QStringLiteral("服务配置"), card);
    AppTheme::setRole(profileTitle, QStringLiteral("sectionTitle"));
    m_aiProfileList = new QListWidget(card);
    auto *profileButtons = new QHBoxLayout();
    auto *addProfile = new QPushButton(QStringLiteral("添加"), card);
    auto *copyProfile = new QPushButton(QStringLiteral("复制"), card);
    auto *deleteProfile = new QPushButton(QStringLiteral("删除"), card);
    IconProvider::apply(addProfile, AppIcon::Add);
    IconProvider::apply(copyProfile, AppIcon::Copy);
    IconProvider::apply(deleteProfile, AppIcon::Delete);
    AppTheme::setButtonRole(deleteProfile, QStringLiteral("danger"));
    profileButtons->addWidget(addProfile);
    profileButtons->addWidget(copyProfile);
    profileButtons->addWidget(deleteProfile);
    left->addWidget(profileTitle);
    left->addWidget(m_aiProfileList, 1);
    left->addLayout(profileButtons);

    auto *editor = new QWidget(card);
    AppTheme::setRole(editor, QStringLiteral("subtleCard"));
    auto *form = new QFormLayout(editor);
    form->setContentsMargins(18, 16, 18, 16);
    m_profileNameEdit = new QLineEdit(page);
    m_profileNameEdit->setObjectName(QStringLiteral("profileNameEdit"));
    m_providerTypeCombo = new QComboBox(page);
    m_providerTypeCombo->addItems({QStringLiteral("DeepSeek"), QStringLiteral("OpenAI Compatible")});
    m_baseUrlCombo = new QComboBox(page);
    m_baseUrlCombo->setEditable(true);
    m_baseUrlCombo->addItem(QStringLiteral("DeepSeek"), QStringLiteral("https://api.deepseek.com/v1"));
    m_baseUrlCombo->addItem(QStringLiteral("OpenAI"), QStringLiteral("https://api.openai.com/v1"));
    m_apiKeyEdit = new QLineEdit(page);
    m_apiKeyEdit->setObjectName(QStringLiteral("apiKeyEdit"));
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_showApiKeyCheck = new QCheckBox(QStringLiteral("显示密钥"), page);
    auto *keyRow = new QHBoxLayout();
    keyRow->addWidget(m_apiKeyEdit, 1);
    keyRow->addWidget(m_showApiKeyCheck);
    m_modelEdit = new QLineEdit(page);
    m_globalPromptEdit = new QTextEdit(page);
    auto *test = new QPushButton(QStringLiteral("测试连接"), page);
    auto *save = new QPushButton(QStringLiteral("保存"), page);
    test->setObjectName(QStringLiteral("testAiServiceButton"));
    save->setObjectName(QStringLiteral("saveAiServiceButton"));
    IconProvider::apply(test, AppIcon::Test);
    IconProvider::apply(save, AppIcon::Save);
    AppTheme::setButtonRole(save, QStringLiteral("primary"));
    auto *buttons = new QHBoxLayout();
    buttons->addWidget(test);
    buttons->addWidget(save);
    buttons->addStretch(1);
    form->addRow(QStringLiteral("配置名称："), m_profileNameEdit);
    form->addRow(QStringLiteral("Provider Type："), m_providerTypeCombo);
    form->addRow(QStringLiteral("Provider / Base URL："), m_baseUrlCombo);
    form->addRow(QStringLiteral("API Key："), keyRow);
    form->addRow(QStringLiteral("模型："), m_modelEdit);
    form->addRow(QStringLiteral("全局默认 Prompt："), m_globalPromptEdit);
    form->addRow(buttons);
    content->addLayout(left, 1);
    content->addWidget(editor, 2);
    root->addWidget(card, 1);
    connect(m_showApiKeyCheck, &QCheckBox::toggled, this, [this](bool checked) { m_apiKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password); });
    connect(m_aiProfileList, &QListWidget::currentRowChanged, this, &PetControlCenterWindow::loadSelectedAiProfile);
    connect(addProfile, &QPushButton::clicked, this, &PetControlCenterWindow::addAiProfile);
    connect(copyProfile, &QPushButton::clicked, this, &PetControlCenterWindow::copyAiProfile);
    connect(deleteProfile, &QPushButton::clicked, this, &PetControlCenterWindow::deleteAiProfile);
    connect(test, &QPushButton::clicked, this, &PetControlCenterWindow::testAiService);
    connect(save, &QPushButton::clicked, this, &PetControlCenterWindow::saveAiService);
    return page;
}

QWidget *PetControlCenterWindow::createRuntimeSettingsPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    preparePage(page, layout);
    addPageHeading(layout, QStringLiteral("运行设置"),
                   QStringLiteral("这些设置会立即同步到当前选中的运行实例。"));
    auto *card = new QFrame(page);
    AppTheme::setRole(card, QStringLiteral("card"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 18, 20, 18);
    cardLayout->setSpacing(10);
    m_topMostCheck = new QCheckBox(QStringLiteral("窗口置顶"), card);
    m_lockedCheck = new QCheckBox(QStringLiteral("锁定位置"), card);
    m_mousePassthroughCheck = new QCheckBox(QStringLiteral("鼠标穿透"), card);
    m_patrolEnabledCheck = new QCheckBox(QStringLiteral("允许桌宠巡逻"), card);
    m_topMostCheck->setObjectName(QStringLiteral("runtimeTopMostCheck"));
    m_lockedCheck->setObjectName(QStringLiteral("runtimeLockedCheck"));
    m_mousePassthroughCheck->setObjectName(QStringLiteral("runtimeMousePassthroughCheck"));
    m_patrolEnabledCheck->setObjectName(QStringLiteral("runtimePatrolEnabledCheck"));
    for (QCheckBox *box : {m_topMostCheck, m_lockedCheck, m_mousePassthroughCheck, m_patrolEnabledCheck}) {
        cardLayout->addWidget(box);
        connect(box, &QCheckBox::toggled, this, &PetControlCenterWindow::applyRuntimeSettings);
    }
    layout->addWidget(card);
    layout->addStretch(1);
    return page;
}

QWidget *PetControlCenterWindow::createAssetToolsPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    preparePage(page, layout);
    addPageHeading(layout, QStringLiteral("素材工具"),
                   QStringLiteral("连接外部像素绘图和素材处理工具，参数模板不会写入桌宠包。"));
    auto *maker = new QPushButton(QStringLiteral("打开桌宠制作器"), page);
    IconProvider::apply(maker, AppIcon::Edit);
    AppTheme::setButtonRole(maker, QStringLiteral("primary"));
    auto *hint = new QLabel(QStringLiteral("参数模板支持 {file}、{dir}、{project}、{action}。"), page);
    AppTheme::setRole(hint, QStringLiteral("muted"));
    layout->addWidget(hint);

    auto *card = new QFrame(page);
    AppTheme::setRole(card, QStringLiteral("card"));
    auto *toolForm = new QFormLayout(card);
    toolForm->setContentsMargins(18, 16, 18, 16);
    auto addToolRow = [this, page, toolForm](const QString &id, const QString &title) {
        QSettings settings;
        auto *container = new QWidget(page);
        auto *row = new QHBoxLayout(container);
        row->setContentsMargins(0, 0, 0, 0);
        auto *pathEdit = new QLineEdit(settings.value(QStringLiteral("tools/%1/path").arg(id)).toString(), container);
        auto *argsEdit = new QLineEdit(settings.value(QStringLiteral("tools/%1/argumentsTemplate").arg(id)).toString(), container);
        argsEdit->setPlaceholderText(QStringLiteral("\"{file}\" --project \"{project}\" --action \"{action}\""));
        auto *browse = new QPushButton(QStringLiteral("浏览"), container);
        auto *save = new QPushButton(QStringLiteral("保存"), container);
        auto *open = new QPushButton(QStringLiteral("打开"), container);
        row->addWidget(pathEdit, 2);
        row->addWidget(argsEdit, 2);
        row->addWidget(browse);
        row->addWidget(save);
        row->addWidget(open);
        toolForm->addRow(title, container);

        connect(browse, &QPushButton::clicked, this, [this, pathEdit]() {
            const QString exe = QFileDialog::getOpenFileName(this,
                                                             QStringLiteral("选择工具程序"),
                                                             QString(),
                                                             QStringLiteral("Programs (*.exe);;All Files (*)"));
            if (!exe.isEmpty()) {
                pathEdit->setText(exe);
            }
        });
        connect(save, &QPushButton::clicked, this, [id, title, pathEdit, argsEdit]() {
            ToolIntegrationManager tools;
            tools.setTool(id, title, pathEdit->text().trimmed(), argsEdit->text());
        });
        connect(open, &QPushButton::clicked, this, [this, id, title, pathEdit, argsEdit]() {
            ToolIntegrationManager tools;
            tools.setTool(id, title, pathEdit->text().trimmed(), argsEdit->text());
            const PetProject project = selectedProject();
            const QString firstAction = project.actions.isEmpty() ? QString() : project.actions.firstKey();
            const QString firstFrame = firstAction.isEmpty() || project.actions.value(firstAction).frames.isEmpty()
                ? QString()
                : project.absolutePathFor(project.actions.value(firstAction).frames.first().path);
            const QHash<QString, QString> values {
                {QStringLiteral("file"), firstFrame},
                {QStringLiteral("dir"), project.projectDir},
                {QStringLiteral("project"), project.name},
                {QStringLiteral("action"), firstAction}
            };
            if (!tools.launchTool(id, values)) {
                QMessageBox::warning(this, QStringLiteral("打开失败"), QStringLiteral("请检查工具路径是否存在。"));
            }
        });
    };

    addToolRow(QStringLiteral("libresprite"), QStringLiteral("LibreSprite"));
    addToolRow(QStringLiteral("spriteful"), QStringLiteral("Spriteful"));
    addToolRow(QStringLiteral("custom"), QStringLiteral("自定义工具"));
    layout->addWidget(card);
    layout->addStretch(1);
    connect(maker, &QPushButton::clicked, this, [this]() { openPetMaker(selectedPetPath()); });
    layout->addWidget(maker, 0, Qt::AlignLeft);
    return page;
}

QWidget *PetControlCenterWindow::createGeneralSettingsPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    preparePage(page, layout);
    addPageHeading(layout, QStringLiteral("通用设置"),
                   QStringLiteral("调整启动恢复、主动聊天气泡和群聊默认行为。"));
    auto *card = new QFrame(page);
    AppTheme::setRole(card, QStringLiteral("card"));
    auto *form = new QFormLayout(card);
    form->setContentsMargins(20, 18, 20, 18);
    m_restoreRunningPetsCheck = new QCheckBox(QStringLiteral("恢复上次运行桌宠"), card);
    m_proactiveDefaultCheck = new QCheckBox(QStringLiteral("主动聊天默认启用"), card);
    m_bubbleSecondsEdit = new QLineEdit(card);
    m_bubbleMaxCharsEdit = new QLineEdit(card);
    m_defaultMaxRespondersEdit = new QLineEdit(card);
    auto *save = new QPushButton(QStringLiteral("保存通用设置"), card);
    m_bubbleSecondsEdit->setObjectName(QStringLiteral("bubbleSecondsEdit"));
    m_bubbleMaxCharsEdit->setObjectName(QStringLiteral("bubbleMaxCharsEdit"));
    m_defaultMaxRespondersEdit->setObjectName(QStringLiteral("defaultMaxRespondersEdit"));
    save->setObjectName(QStringLiteral("saveGeneralSettingsButton"));
    AppTheme::setButtonRole(save, QStringLiteral("primary"));
    IconProvider::apply(save, AppIcon::Save);
    form->addRow(m_restoreRunningPetsCheck);
    form->addRow(m_proactiveDefaultCheck);
    form->addRow(QStringLiteral("Bubble 显示秒数："), m_bubbleSecondsEdit);
    form->addRow(QStringLiteral("Bubble 最大字符数："), m_bubbleMaxCharsEdit);
    form->addRow(QStringLiteral("群聊默认最大回复数："), m_defaultMaxRespondersEdit);
    auto *live2dStatus = new QLabel(Live2DRenderBackend::availabilityText(), page);
    live2dStatus->setObjectName(QStringLiteral("live2dAvailabilityLabel"));
    live2dStatus->setWordWrap(true);
    AppTheme::setRole(live2dStatus, QStringLiteral("muted"));
    form->addRow(QStringLiteral("渲染后端："), live2dStatus);
    form->addRow(save);
    layout->addWidget(card);
    layout->addStretch(1);
    connect(save, &QPushButton::clicked, this, &PetControlCenterWindow::applyGeneralSettings);
    return page;
}

void PetControlCenterWindow::refreshAll()
{
    m_registry.scan();
    refreshOverview();
    refreshMyPets();
    refreshAiService();
    refreshAiRole();
    refreshRuntimeSettings();
}

void PetControlCenterWindow::refreshOverview()
{
    const int runningCount = m_manager->runningPetCount();
    m_runningCountLabel->setText(QString::number(runningCount));
    clearLayout(m_overviewLayout);
    for (PetRuntimeInstance *runtime : m_manager->runningInstances()) {
        auto *card = new QFrame();
        AppTheme::setRole(card, QStringLiteral("card"));
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(16, 12, 16, 12);
        auto *text = new QWidget(card);
        auto *textLayout = new QVBoxLayout(text);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);
        auto *name = new QLabel(runtime->displayName(), text);
        auto *detail = new QLabel(runtime->runtimeStatusText(), text);
        AppTheme::setRole(name, QStringLiteral("sectionTitle"));
        AppTheme::setRole(detail, QStringLiteral("muted"));
        textLayout->addWidget(name);
        textLayout->addWidget(detail);
        auto *status = new QLabel(QStringLiteral("运行中"), card);
        AppTheme::setRole(status, QStringLiteral("status"));
        status->setProperty("status", QStringLiteral("success"));
        auto *chat = new QPushButton(QStringLiteral("打开聊天"), card);
        auto *locate = new QPushButton(QStringLiteral("定位桌宠"), card);
        auto *stop = new QPushButton(QStringLiteral("停止"), card);
        IconProvider::apply(chat, AppIcon::Chat);
        IconProvider::apply(locate, AppIcon::Locate);
        IconProvider::apply(stop, AppIcon::Stop);
        AppTheme::setButtonRole(chat, QStringLiteral("primary"));
        AppTheme::setButtonRole(stop, QStringLiteral("danger"));
        connect(chat, &QPushButton::clicked, runtime->runtimeWindow(), [window = runtime->runtimeWindow()]() { if (window) QMetaObject::invokeMethod(window, "openChatWindow", Qt::DirectConnection); });
        connect(locate, &QPushButton::clicked, runtime->runtimeWindow(), [window = runtime->runtimeWindow()]() { if (window) { window->show(); window->raise(); window->activateWindow(); } });
        connect(stop, &QPushButton::clicked, this, [this, path = runtime->projectPath()]() { m_manager->stopPet(path); });
        row->addWidget(text, 1);
        row->addWidget(status);
        row->addWidget(chat);
        row->addWidget(locate);
        row->addWidget(stop);
        m_overviewLayout->addWidget(card);
    }
    if (runningCount == 0) {
        auto *empty = new QFrame();
        AppTheme::setRole(empty, QStringLiteral("subtleCard"));
        auto *emptyLayout = new QVBoxLayout(empty);
        emptyLayout->setContentsMargins(24, 34, 24, 34);
        auto *title = new QLabel(QStringLiteral("还没有正在运行的桌宠"), empty);
        auto *hint = new QLabel(QStringLiteral("先到“我的桌宠”选择项目，再从这里快速启动。"), empty);
        title->setAlignment(Qt::AlignCenter);
        hint->setAlignment(Qt::AlignCenter);
        AppTheme::setRole(title, QStringLiteral("sectionTitle"));
        AppTheme::setRole(hint, QStringLiteral("muted"));
        emptyLayout->addWidget(title);
        emptyLayout->addWidget(hint);
        m_overviewLayout->addWidget(empty);
    }
    m_overviewLayout->addStretch(1);
}

QFrame *PetControlCenterWindow::makePetCard(const PetProjectEntry &entry)
{
    auto *card = new QFrame();
    AppTheme::setRole(card, QStringLiteral("card"));
    auto *row = new QHBoxLayout(card);
    row->setContentsMargins(14, 12, 14, 12);
    row->setSpacing(14);
    auto *cover = new QLabel(card);
    cover->setFixedSize(82, 82);
    cover->setAlignment(Qt::AlignCenter);
    AppTheme::setRole(cover, QStringLiteral("subtleCard"));
    QPixmap pix(entry.coverPath);
    cover->setPixmap(pix.isNull() ? QPixmap() : pix.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    const bool running = m_manager->isPetRunning(entry.petJsonPath);
    auto *info = new QWidget(card);
    auto *infoLayout = new QVBoxLayout(info);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(3);
    auto *name = new QLabel(entry.displayName, info);
    auto *project = new QLabel(QStringLiteral("%1 · %2 个动作 · %3")
                                   .arg(entry.projectName)
                                   .arg(entry.actionCount)
                                   .arg(entry.legacyProject ? QStringLiteral("Legacy")
                                                            : (entry.externalProject ? QStringLiteral("外部项目")
                                                                                     : QStringLiteral("已管理项目"))),
                               info);
    auto *path = new QLabel(entry.petJsonPath, info);
    path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    AppTheme::setRole(name, QStringLiteral("sectionTitle"));
    AppTheme::setRole(project, QStringLiteral("muted"));
    AppTheme::setRole(path, QStringLiteral("caption"));
    infoLayout->addWidget(name);
    infoLayout->addWidget(project);
    infoLayout->addWidget(path);
    auto *status = new QLabel(running ? QStringLiteral("运行中") : QStringLiteral("已停止"), card);
    AppTheme::setRole(status, QStringLiteral("status"));
    status->setProperty("status", running ? QStringLiteral("success") : QStringLiteral("neutral"));
    auto *select = new QPushButton(QStringLiteral("设为当前"), card);
    auto *startStop = new QPushButton(running ? QStringLiteral("停止") : QStringLiteral("启动"), card);
    auto *actions = new QPushButton(QStringLiteral("动作与素材"), card);
    auto *maker = new QPushButton(QStringLiteral("制作器"), card);
    auto *copy = new QPushButton(QStringLiteral("复制"), card);
    auto *remove = new QPushButton(entry.externalProject ? QStringLiteral("解除注册") : QStringLiteral("删除"), card);
    IconProvider::apply(select, AppIcon::Locate);
    IconProvider::apply(startStop, running ? AppIcon::Stop : AppIcon::Play);
    IconProvider::apply(actions, AppIcon::Motion);
    IconProvider::apply(maker, AppIcon::Edit);
    IconProvider::apply(copy, AppIcon::Copy);
    IconProvider::apply(remove, AppIcon::Delete);
    AppTheme::setButtonRole(startStop, running ? QStringLiteral("danger") : QStringLiteral("primary"));
    AppTheme::setButtonRole(remove, QStringLiteral("danger"));
    connect(select, &QPushButton::clicked, this, [this, path = entry.petJsonPath]() { selectProject(path); });
    connect(startStop, &QPushButton::clicked, this, [this, path = entry.petJsonPath, running]() {
        if (running) {
            m_manager->stopPet(path);
        } else {
            m_manager->startPet(path);
        }
    });
    connect(actions, &QPushButton::clicked, this, [this, path = entry.petJsonPath]() { selectProject(path); openActionMaterial(); });
    connect(maker, &QPushButton::clicked, this, [this, path = entry.petJsonPath]() { openPetMaker(path); });
    connect(copy, &QPushButton::clicked, this, [this, path = entry.petJsonPath]() { copyPetProject(path); });
    connect(remove, &QPushButton::clicked, this, [this, entry]() { deletePetProject(entry); });
    row->addWidget(cover);
    row->addWidget(info, 1);
    row->addWidget(status);
    auto *actionsLayout = new QGridLayout();
    actionsLayout->setHorizontalSpacing(8);
    actionsLayout->setVerticalSpacing(8);
    actionsLayout->addWidget(startStop, 0, 0);
    actionsLayout->addWidget(select, 0, 1);
    actionsLayout->addWidget(actions, 0, 2);
    actionsLayout->addWidget(maker, 1, 0);
    actionsLayout->addWidget(copy, 1, 1);
    actionsLayout->addWidget(remove, 1, 2);
    row->addLayout(actionsLayout);
    return card;
}

void PetControlCenterWindow::refreshMyPets()
{
    clearLayout(m_myPetsLayout);
    for (const PetProjectEntry &entry : m_registry.entries()) {
        m_myPetsLayout->addWidget(makePetCard(entry));
    }
    if (m_registry.entries().isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("尚未发现桌宠项目。可以创建新项目或导入 petpack。"));
        empty->setAlignment(Qt::AlignCenter);
        AppTheme::setRole(empty, QStringLiteral("muted"));
        m_myPetsLayout->addWidget(empty);
    }
    m_myPetsLayout->addStretch(1);
}

void PetControlCenterWindow::refreshAiService()
{
    const QString currentId = m_aiProfileList && m_aiProfileList->currentItem()
        ? m_aiProfileList->currentItem()->data(Qt::UserRole).toString()
        : QString();
    if (m_aiProfileList) {
        m_aiProfileList->clear();
        AIProviderProfileRegistry profiles;
        int restoreRow = 0;
        const QVector<AIProviderProfile> all = profiles.profiles();
        for (int i = 0; i < all.size(); ++i) {
            const AIProviderProfile &profile = all.at(i);
            auto *item = new QListWidgetItem(profile.displayName.isEmpty() ? profile.id : profile.displayName, m_aiProfileList);
            item->setData(Qt::UserRole, profile.id);
            if (profile.id == currentId) restoreRow = i;
        }
        if (m_aiProfileList->count() > 0) {
            m_aiProfileList->setCurrentRow(qBound(0, restoreRow, m_aiProfileList->count() - 1));
        }
        loadSelectedAiProfile();
        return;
    }
}

void PetControlCenterWindow::loadSelectedAiProfile()
{
    if (!m_aiProfileList || !m_aiProfileList->currentItem()) {
        return;
    }
    const QString id = m_aiProfileList->currentItem()->data(Qt::UserRole).toString();
    AIProviderProfileRegistry profiles;
    AIProviderProfile profile = profiles.profile(id);
    m_profileNameEdit->setText(profile.displayName);
    m_providerTypeCombo->setCurrentText(profile.providerType == QStringLiteral("DeepSeek")
                                            ? QStringLiteral("DeepSeek")
                                            : QStringLiteral("OpenAI Compatible"));
    const int index = m_baseUrlCombo->findData(profile.baseUrl);
    if (index >= 0) m_baseUrlCombo->setCurrentIndex(index); else m_baseUrlCombo->setCurrentText(profile.baseUrl);
    m_modelEdit->setText(profile.model);
    m_apiKeyEdit->setText(CredentialStore::instance().readSecret(profile.credentialId));
    QSettings settings;
    m_globalPromptEdit->setPlainText(settings.value(QStringLiteral("ai/systemPrompt")).toString());
}

void PetControlCenterWindow::addAiProfile()
{
    AIProviderProfile profile;
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    profile.displayName = QStringLiteral("新 AI 服务");
    profile.providerType = QStringLiteral("OpenAICompatible");
    profile.baseUrl = QStringLiteral("https://api.deepseek.com/v1");
    profile.model = QStringLiteral("deepseek-v4-flash");
    profile.credentialId = profile.id;
    if (!AIProviderProfileRegistry().saveProfile(profile)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("新的 AI 服务档案未能保存。"));
        return;
    }
    refreshAiService();
}

void PetControlCenterWindow::copyAiProfile()
{
    if (!m_aiProfileList || !m_aiProfileList->currentItem()) return;
    AIProviderProfileRegistry profiles;
    const QString newId = profiles.duplicateProfile(m_aiProfileList->currentItem()->data(Qt::UserRole).toString());
    if (newId.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("复制失败"), QStringLiteral("AI 服务档案未能复制。"));
        return;
    }
    if (!CredentialStore::instance().deleteSecret(newId)) {
        profiles.deleteProfile(newId);
        QMessageBox::warning(this, QStringLiteral("复制失败"), QStringLiteral("新档案的凭据初始化失败，已撤销复制。"));
        return;
    }
    refreshAiService();
}

void PetControlCenterWindow::deleteAiProfile()
{
    if (!m_aiProfileList || !m_aiProfileList->currentItem()) return;
    const QString id = m_aiProfileList->currentItem()->data(Qt::UserRole).toString();
    if (id == QStringLiteral("default")) {
        QMessageBox::warning(this, QStringLiteral("不能删除"), QStringLiteral("默认 AI 服务不能删除。"));
        return;
    }
    QStringList users;
    for (const PetProjectEntry &entry : m_registry.entries()) {
        PetProject project;
        if (project.load(entry.petJsonPath) && project.aiProviderProfileId == id) {
            users.append(entry.displayName);
        }
    }
    if (!users.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("不能删除"),
                             QStringLiteral("该 AI 服务正在被以下角色使用：\n- %1").arg(users.join(QStringLiteral("\n- "))));
        return;
    }
    AIProviderProfileRegistry profiles;
    const AIProviderProfile profile = profiles.profile(id);
    const QString oldSecret = CredentialStore::instance().readSecret(profile.credentialId);
    if (!CredentialStore::instance().deleteSecret(profile.credentialId)) {
        QMessageBox::warning(this, QStringLiteral("删除失败"), QStringLiteral("API Key 安全存储未能删除，档案保持不变。"));
        return;
    }
    if (!profiles.deleteProfile(id)) {
        if (!oldSecret.isEmpty()) {
            CredentialStore::instance().writeSecret(profile.credentialId, oldSecret);
        }
        QMessageBox::warning(this, QStringLiteral("删除失败"), QStringLiteral("AI 服务档案未能删除，已尝试恢复原有凭据。"));
        return;
    }
    refreshAiService();
}

void PetControlCenterWindow::refreshAiRole()
{
    PetProject project = selectedProject();
    const bool ok = project.isValid();
    m_rolePetLabel->setText(ok ? project.name : QStringLiteral("未选择项目"));
    m_roleNameEdit->setEnabled(ok);
    m_rolePromptEdit->setEnabled(ok);
    m_roleProviderCombo->setEnabled(ok);
    m_saveRoleButton->setEnabled(ok);
    m_roleNameEdit->setText(ok ? project.aiCharacterName : QString());
    m_rolePromptEdit->setPlainText(ok ? project.aiSystemPrompt : QString());
    m_roleProviderCombo->clear();
    m_roleProviderCombo->addItem(QStringLiteral("AI 服务：未配置"), QString());
    AIProviderProfileRegistry profiles;
    for (const AIProviderProfile &profile : profiles.profiles()) {
        m_roleProviderCombo->addItem(profile.displayName, profile.id);
    }
    if (ok) {
        const int index = m_roleProviderCombo->findData(project.aiProviderProfileId);
        m_roleProviderCombo->setCurrentIndex(index >= 0 ? index : 0);
    }
}

void PetControlCenterWindow::refreshRuntimeSettings()
{
    m_updatingRuntimeSettings = true;
    PetProject project = selectedProject();
    setRuntimeCheckboxesEnabled(project.isValid());
    if (project.isValid()) {
        m_topMostCheck->setChecked(project.topMost);
        m_lockedCheck->setChecked(project.locked);
        m_mousePassthroughCheck->setChecked(project.mousePassthrough);
        m_patrolEnabledCheck->setChecked(project.patrolEnabled);
    }
    QSettings settings;
    m_restoreRunningPetsCheck->setChecked(settings.value(QStringLiteral("app/restoreRunningPets"), true).toBool());
    m_proactiveDefaultCheck->setChecked(settings.value(QStringLiteral("proactive/enabledDefault"), true).toBool());
    m_bubbleSecondsEdit->setText(settings.value(QStringLiteral("bubble/displaySeconds"), 10).toString());
    m_bubbleMaxCharsEdit->setText(settings.value(QStringLiteral("bubble/maxDisplayCharacters"), 80).toString());
    m_defaultMaxRespondersEdit->setText(settings.value(QStringLiteral("conversation/defaultMaxResponders"), 2).toString());
    m_updatingRuntimeSettings = false;
}

void PetControlCenterWindow::saveAiService()
{
    if (!m_aiProfileList || !m_aiProfileList->currentItem()) return;
    const QString id = m_aiProfileList->currentItem()->data(Qt::UserRole).toString();
    QString baseUrl = m_baseUrlCombo->currentData().toString();
    if (baseUrl.isEmpty()) baseUrl = m_baseUrlCombo->currentText().trimmed();
    while (baseUrl.endsWith('/')) baseUrl.chop(1);
    const QString apiKey = m_apiKeyEdit->text().trimmed();
    AIProviderProfileRegistry profiles;
    AIProviderProfile profile = profiles.profile(id);
    profile.id = id;
    profile.displayName = m_profileNameEdit->text().trimmed().isEmpty() ? id : m_profileNameEdit->text().trimmed();
    profile.providerType = m_providerTypeCombo->currentText() == QStringLiteral("DeepSeek")
        ? QStringLiteral("DeepSeek")
        : QStringLiteral("OpenAICompatible");
    profile.baseUrl = baseUrl;
    profile.model = m_modelEdit->text().trimmed();
    profile.credentialId = id;
    CredentialUpdateMode credentialMode = CredentialUpdateMode::Replace;
    if (apiKey.isEmpty()) {
        QMessageBox choice(this);
        choice.setWindowTitle(QStringLiteral("API Key 为空"));
        choice.setText(QStringLiteral("未填写 API Key。请选择如何处理当前已保存的安全凭据。"));
        auto *keep = choice.addButton(QStringLiteral("保留旧 Key"), QMessageBox::AcceptRole);
        auto *clear = choice.addButton(QStringLiteral("删除 Key 并标记未配置"), QMessageBox::DestructiveRole);
        choice.addButton(QMessageBox::Cancel);
        choice.exec();
        if (choice.clickedButton() == keep) {
            credentialMode = CredentialUpdateMode::KeepExisting;
        } else if (choice.clickedButton() == clear) {
            credentialMode = CredentialUpdateMode::Clear;
        } else {
            return;
        }
    }
    CredentialStoreAdapter credentials;
    ProviderProfileRegistryStoreAdapter profileStore;
    AIProviderProfileService service(credentials, profileStore);
    const ProfileMutationResult result = service.saveProfile(profile, credentialMode, apiKey);
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), result.message);
        return;
    }
    QMessageBox::information(this, QStringLiteral("已保存"), QStringLiteral("AI 服务配置已保存。"));
    refreshAiService();
}

void PetControlCenterWindow::testAiService()
{
    AIProvider::AIConfig config;
    config.baseUrl = m_baseUrlCombo->currentData().toString().isEmpty() ? m_baseUrlCombo->currentText().trimmed() : m_baseUrlCombo->currentData().toString();
    config.apiKey = m_apiKeyEdit->text().trimmed();
    config.model = m_modelEdit->text().trimmed();
    AIProvider::instance().testConnection(config, [this](bool success, const QString &message) {
        success ? QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("连接成功。"))
                : QMessageBox::warning(this, QStringLiteral("失败"), message);
    });
}

void PetControlCenterWindow::saveAiRole()
{
    PetProject project = selectedProject();
    if (!project.isValid()) return;
    project.aiCharacterName = m_roleNameEdit->text().trimmed();
    project.aiSystemPrompt = m_rolePromptEdit->toPlainText().trimmed();
    project.aiProviderProfileId = m_roleProviderCombo->currentData().toString();
    QString error;
    if (!project.save(&error)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), error);
        return;
    }
    if (PetRuntimeInstance *runtime = m_manager->instance(project.petJsonPath())) {
        if (runtime->runtimeWindow()) runtime->runtimeWindow()->reloadAiRoleFromProject();
    }
    refreshAll();
}

void PetControlCenterWindow::applyRuntimeSettings()
{
    if (m_updatingRuntimeSettings) return;
    PetProject project = selectedProject();
    if (!project.isValid()) return;
    if (RuntimePetWindow *window = m_manager->instance(project.petJsonPath()) ? m_manager->instance(project.petJsonPath())->runtimeWindow() : nullptr) {
        window->setRuntimeTopMost(m_topMostCheck->isChecked());
        window->setRuntimeLocked(m_lockedCheck->isChecked());
        window->setRuntimeMousePassthrough(m_mousePassthroughCheck->isChecked());
        window->setRuntimePatrolEnabled(m_patrolEnabledCheck->isChecked());
        return;
    }
    PetProject::RuntimeStatePatch patch;
    patch.hasTopMost = true; patch.topMost = m_topMostCheck->isChecked();
    patch.hasLocked = true; patch.locked = m_lockedCheck->isChecked();
    patch.hasMousePassthrough = true; patch.mousePassthrough = m_mousePassthroughCheck->isChecked();
    patch.hasPatrolEnabled = true; patch.patrolEnabled = m_patrolEnabledCheck->isChecked();
    QString error;
    if (!project.saveRuntimeStatePatch(patch, &error)) QMessageBox::warning(this, QStringLiteral("保存失败"), error);
}

void PetControlCenterWindow::applyGeneralSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("app/restoreRunningPets"), m_restoreRunningPetsCheck->isChecked());
    settings.setValue(QStringLiteral("proactive/enabledDefault"), m_proactiveDefaultCheck->isChecked());
    settings.setValue(QStringLiteral("bubble/displaySeconds"), m_bubbleSecondsEdit->text().toInt());
    settings.setValue(QStringLiteral("bubble/maxDisplayCharacters"), m_bubbleMaxCharsEdit->text().toInt());
    settings.setValue(QStringLiteral("conversation/defaultMaxResponders"), m_defaultMaxRespondersEdit->text().toInt());
}

void PetControlCenterWindow::createPetProject()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("创建桌宠"), QStringLiteral("项目名称"), QLineEdit::Normal, QStringLiteral("新桌宠"), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    QString dirName = QInputDialog::getText(this, QStringLiteral("创建桌宠"), QStringLiteral("目录名称"), QLineEdit::Normal, safeDirName(name), &ok);
    if (!ok || dirName.trimmed().isEmpty()) return;
    dirName = safeDirName(dirName);
    QDir petsRoot(QDir(m_registry.rootPath()).filePath(QStringLiteral("pets")));
    petsRoot.mkpath(QStringLiteral("."));
    QString target = petsRoot.filePath(dirName);
    int suffix = 2;
    while (QFileInfo::exists(target)) {
        target = petsRoot.filePath(QStringLiteral("%1_%2").arg(dirName).arg(suffix++));
    }
    PetProject project = PetProject::createNew(target, name.trimmed(), QStringLiteral("simple"));
    QString error;
    if (!project.save(&error)) {
        QMessageBox::warning(this, QStringLiteral("创建失败"), error);
        return;
    }
    selectProject(project.petJsonPath());
    refreshAll();
    openPetMaker(project.petJsonPath());
}

void PetControlCenterWindow::importPetProject()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("导入 pet.json"), QString(), QStringLiteral("pet.json (pet.json);;JSON (*.json)"));
    if (path.isEmpty()) return;
    PetProject project;
    QString error;
    if (!project.load(path, &error)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), error);
        return;
    }
    QStringList errors;
    QStringList warnings;
    if (!project.validate(&errors, &warnings)) {
        QMessageBox::warning(this, QStringLiteral("Import failed"), QStringLiteral("Project validation failed:\n%1").arg(errors.join('\n')));
        return;
    }
    QSettings settings;
    QStringList external = settings.value(QStringLiteral("pets/externalProjects")).toStringList();
    const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (!external.contains(clean)) {
        external.append(clean);
        settings.setValue(QStringLiteral("pets/externalProjects"), external);
    }
    selectProject(clean);
    refreshAll();
}

void PetControlCenterWindow::importPetpack()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("导入 petpack"), QString(), QStringLiteral("Pet Pack (*.petpack);;Zip (*.zip)"));
    if (path.isEmpty()) return;
    QString imported;
    QString error;
    const QString parent = QDir(m_registry.rootPath()).filePath(QStringLiteral("pets"));
    QDir().mkpath(parent);
    if (!PetProject::importPetpack(path, parent, &imported, &error)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), error);
        return;
    }
    selectProject(imported);
    refreshAll();
}

void PetControlCenterWindow::copyPetProject(const QString &path)
{
    PetProject source;
    QString error;
    if (!source.load(path, &error)) {
        QMessageBox::warning(this, QStringLiteral("复制失败"), error);
        return;
    }
    QDir petsRoot(QDir(m_registry.rootPath()).filePath(QStringLiteral("pets")));
    petsRoot.mkpath(QStringLiteral("."));
    const QString base = safeDirName(source.name + QStringLiteral("_copy"));
    QString target = petsRoot.filePath(base);
    int suffix = 2;
    while (QFileInfo::exists(target)) {
        target = petsRoot.filePath(QStringLiteral("%1_%2").arg(base).arg(suffix++));
    }
    if (!copyDirectory(source.projectDir, target, &error)) {
        QMessageBox::warning(this, QStringLiteral("复制失败"), error);
        return;
    }
    PetProject copy;
    if (!copy.load(QDir(target).filePath(QStringLiteral("pet.json")), &error)) {
        QMessageBox::warning(this, QStringLiteral("复制失败"), error);
        return;
    }
    copy.projectId.clear();
    copy.name += QStringLiteral(" 副本");
    if (!copy.save(&error)) {
        QMessageBox::warning(this, QStringLiteral("复制失败"), error);
        return;
    }
    selectProject(copy.petJsonPath());
    refreshAll();
}

void PetControlCenterWindow::deletePetProject(const PetProjectEntry &entry)
{
    if (entry.legacyProject) {
        QMessageBox::warning(this, QStringLiteral("不能删除"), QStringLiteral("内置示例桌宠不能删除。"));
        return;
    }
    if (m_manager->isPetRunning(entry.petJsonPath)) {
        QMessageBox::warning(this, QStringLiteral("不能删除"), QStringLiteral("请先停止正在运行的桌宠。"));
        return;
    }
    if (entry.externalProject) {
        QSettings settings;
        QStringList external = settings.value(QStringLiteral("pets/externalProjects")).toStringList();
        external.removeAll(entry.petJsonPath);
        settings.setValue(QStringLiteral("pets/externalProjects"), external);
        refreshAll();
        return;
    }
    const QString petsRoot = QDir(m_registry.rootPath()).filePath(QStringLiteral("pets"));
    const QString canonicalRoot = QFileInfo(petsRoot).canonicalFilePath();
    const QString canonicalProject = QFileInfo(entry.projectDirectory).canonicalFilePath();
    if (canonicalRoot.isEmpty() || canonicalProject.isEmpty() || !canonicalProject.startsWith(canonicalRoot + QDir::separator())) {
        QMessageBox::warning(this, QStringLiteral("不能删除"), QStringLiteral("项目路径不在受管理 pets 目录内。"));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("确认删除"), QStringLiteral("确定删除 %1？").arg(entry.displayName)) != QMessageBox::Yes) {
        return;
    }
    QDir(entry.projectDirectory).removeRecursively();
    if (m_selectedProjectPath == entry.petJsonPath) {
        m_selectedProjectPath.clear();
    }
    refreshAll();
}

void PetControlCenterWindow::openPetMaker(const QString &path)
{
    if (!m_editorWindow) {
        m_editorWindow = new EditorWindow();
        m_editorWindow->setAttribute(Qt::WA_DeleteOnClose);
    }
    const QString target = path.isEmpty() ? selectedPetPath() : path;
    if (!target.isEmpty()) {
        m_editorWindow->openProjectFile(target);
    }
    m_editorWindow->show();
    m_editorWindow->raise();
    m_editorWindow->activateWindow();
}

void PetControlCenterWindow::openActionMaterial()
{
    if (!m_actionWindow) {
        m_actionWindow = new ActionMaterialWindow(m_manager);
        m_actionWindow->setAttribute(Qt::WA_DeleteOnClose);
    }
    if (!selectedPetPath().isEmpty()) {
        m_actionWindow->loadProject(selectedPetPath());
    }
    m_actionWindow->show();
    m_actionWindow->raise();
}

void PetControlCenterWindow::openAiConsole()
{
    if (!m_aiConsoleWindow) {
        m_aiConsoleWindow = new AIConversationConsoleWindow(m_manager);
        m_aiConsoleWindow->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_aiConsoleWindow->show();
    m_aiConsoleWindow->raise();
}

void PetControlCenterWindow::selectProject(const QString &path)
{
    m_selectedProjectPath = path;
    refreshAiRole();
    refreshRuntimeSettings();
}

PetProject PetControlCenterWindow::selectedProject() const
{
    PetProject project;
    if (!selectedPetPath().isEmpty()) project.load(selectedPetPath());
    return project;
}

QString PetControlCenterWindow::selectedPetPath() const
{
    if (!m_selectedProjectPath.isEmpty()) return m_selectedProjectPath;
    const QVector<PetProjectEntry> entries = m_registry.entries();
    return entries.isEmpty() ? QString() : entries.first().petJsonPath;
}

void PetControlCenterWindow::setRuntimeCheckboxesEnabled(bool enabled)
{
    m_topMostCheck->setEnabled(enabled);
    m_lockedCheck->setEnabled(enabled);
    m_mousePassthroughCheck->setEnabled(enabled);
    m_patrolEnabledCheck->setEnabled(enabled);
}

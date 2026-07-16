#include "systemtraycontroller.h"

#include "runtimepetmanager.h"
#include "ui/theme/iconprovider.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QStyle>
#include <QSystemTrayIcon>

SystemTrayController::SystemTrayController(RuntimePetManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
{
    m_menu->setObjectName(QStringLiteral("desktopPetTrayMenu"));

    QAction *openCenter = m_menu->addAction(QStringLiteral("打开控制中心"));
    openCenter->setObjectName(QStringLiteral("trayOpenControlCenterAction"));
    openCenter->setIcon(IconProvider::icon(AppIcon::Home));
    connect(openCenter, &QAction::triggered, m_manager, &RuntimePetManager::controlCenterRequested);

    QAction *showAll = m_menu->addAction(QStringLiteral("显示全部桌宠"));
    showAll->setObjectName(QStringLiteral("trayShowAllPetsAction"));
    showAll->setIcon(IconProvider::icon(AppIcon::Pet));
    connect(showAll, &QAction::triggered, m_manager, &RuntimePetManager::showAllPets);

    m_petMenu = m_menu->addMenu(QStringLiteral("启动/停止桌宠"));
    m_petMenu->setObjectName(QStringLiteral("trayPetToggleMenu"));
    m_petMenu->setIcon(IconProvider::icon(AppIcon::Play));
    connect(m_petMenu, &QMenu::aboutToShow, this, &SystemTrayController::rebuildPetMenu);

    m_pausePatrolAction = m_menu->addAction(QStringLiteral("暂停全部巡逻"));
    m_pausePatrolAction->setObjectName(QStringLiteral("trayPausePatrolAction"));
    m_pausePatrolAction->setIcon(IconProvider::icon(AppIcon::Motion));
    m_pausePatrolAction->setCheckable(true);
    connect(m_pausePatrolAction, &QAction::toggled, this, [this](bool paused) {
        m_manager->setAllPatrolEnabled(!paused);
    });

    QAction *disablePassthrough = m_menu->addAction(QStringLiteral("关闭全部鼠标穿透"));
    disablePassthrough->setObjectName(QStringLiteral("trayDisablePassthroughAction"));
    disablePassthrough->setIcon(IconProvider::icon(AppIcon::Locate));
    connect(disablePassthrough, &QAction::triggered, m_manager, [this]() {
        m_manager->setAllMousePassthrough(false);
    });

    m_menu->addSeparator();
    QAction *about = m_menu->addAction(QStringLiteral("关于 Desktop Pet Maker"));
    about->setObjectName(QStringLiteral("trayAboutAction"));
    about->setIcon(QIcon(QStringLiteral(":/branding/app_icon.png")));
    connect(about, &QAction::triggered, m_manager, []() {
        QMessageBox box;
        box.setWindowTitle(QStringLiteral("关于 Desktop Pet Maker"));
        box.setWindowIcon(QIcon(QStringLiteral(":/branding/app_icon.png")));
        box.setIconPixmap(QIcon(QStringLiteral(":/branding/app_icon.png")).pixmap(64, 64));
        box.setText(QStringLiteral("<b>Desktop Pet Maker</b><br>桌宠工坊 · 1.0.0-beta"));
        box.setInformativeText(QStringLiteral("桌宠创作、运行与 AI 互动平台。<br><br>品牌图形与界面资源均为项目原创。"));
        box.exec();
    });
    m_menu->addSeparator();
    QAction *exit = m_menu->addAction(QStringLiteral("退出程序"));
    exit->setObjectName(QStringLiteral("trayExitAction"));
    exit->setIcon(IconProvider::icon(AppIcon::Stop));
    connect(exit, &QAction::triggered, m_manager, &RuntimePetManager::exitApplication);

    QIcon icon(QStringLiteral(":/branding/app_icon.png"));
    if (icon.isNull()) {
        icon = QApplication::windowIcon();
    }
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    m_trayIcon->setIcon(icon);
    m_trayIcon->setToolTip(QStringLiteral("Desktop Pet Maker"));
    m_trayIcon->setContextMenu(m_menu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                    QMetaObject::invokeMethod(m_manager, "controlCenterRequested", Qt::QueuedConnection);
                }
            });
}

SystemTrayController::~SystemTrayController()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
    delete m_menu;
    m_menu = nullptr;
}

bool SystemTrayController::isAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void SystemTrayController::show()
{
    if (isAvailable()) {
        m_trayIcon->show();
    }
}

bool SystemTrayController::controlCenterRequired(bool explicitlyRequested,
                                                 bool firstRun,
                                                 bool trayAvailable)
{
    return explicitlyRequested || firstRun || !trayAvailable;
}

void SystemTrayController::rebuildPetMenu()
{
    m_petMenu->clear();
    const QVector<PetEntry> pets = m_manager->availablePets();
    for (const PetEntry &pet : pets) {
        QAction *action = m_petMenu->addAction(pet.displayName);
        action->setCheckable(true);
        action->setChecked(m_manager->isPetRunning(pet.petJsonPath));
        connect(action, &QAction::triggered, this, [this, path = pet.petJsonPath](bool start) {
            if (start) {
                m_manager->startPet(path);
            } else {
                m_manager->stopPet(path);
            }
        });
    }
    if (pets.isEmpty()) {
        QAction *empty = m_petMenu->addAction(QStringLiteral("未发现桌宠项目"));
        empty->setEnabled(false);
    }
}

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>

#include "editorwindow.h"
#include "petcontrolcenterwindow.h"
#include "petproject.h"
#include "runtimepetmanager.h"
#include "runtimepetwindow.h"
#include "systemtraycontroller.h"
#include "ui/theme/apptheme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("DesktopPetMaker");
    QCoreApplication::setApplicationName("DesktopPetMaker");
    app.setWindowIcon(QIcon(QStringLiteral(":/branding/app_icon.png")));
    AppTheme::apply(app);

    QStringList args = QCoreApplication::arguments();
    const bool openControlCenter = args.removeAll(QStringLiteral("--control-center")) > 0;
    if (args.size() > 1) {
        QList<RuntimePetWindow *> windows;
        for (int i = 1; i < args.size(); ++i) {
            PetProject project;
            QString error;
            if (!project.load(args.at(i), &error)) {
                QMessageBox::warning(nullptr,
                                     QObject::tr("Open Pet Failed"),
                                     QObject::tr("Failed to open %1:\n%2").arg(args.at(i), error));
                continue;
            }

            auto *window = new RuntimePetWindow(project);
            window->setAttribute(Qt::WA_DeleteOnClose);
            if (!project.hasRuntimeAnchorScreen) {
                const QRect screen = QApplication::primaryScreen()
                    ? QApplication::primaryScreen()->availableGeometry()
                    : QRect(0, 0, 1536, 912);
                const int x = screen.left() + 80 + windows.size() * 320;
                window->placeOnGroundAtX(x);
            }
            window->show();
            window->raise();
            window->activateWindow();
            windows.append(window);
        }

        if (!windows.isEmpty()) {
            return app.exec();
        }
    }

    QApplication::setQuitOnLastWindowClosed(false);
    auto *manager = new RuntimePetManager(&app);
    auto *controlCenter = new PetControlCenterWindow(manager);
    controlCenter->hide();
    auto *tray = new SystemTrayController(manager, &app);
    tray->show();
    QObject::connect(manager, &RuntimePetManager::controlCenterRequested,
                     controlCenter, &PetControlCenterWindow::showCenter);
    QSettings settings;
    const bool firstRun = !settings.value(QStringLiteral("app/firstRunCompleted"), false).toBool();
    if (SystemTrayController::controlCenterRequired(openControlCenter, firstRun, tray->isAvailable())) {
        controlCenter->showCenter();
        settings.setValue(QStringLiteral("app/firstRunCompleted"), true);
    }
    manager->restoreStartupPets();

    return app.exec();
}

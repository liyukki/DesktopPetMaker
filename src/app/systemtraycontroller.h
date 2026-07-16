#ifndef SYSTEMTRAYCONTROLLER_H
#define SYSTEMTRAYCONTROLLER_H

#include <QObject>

class QAction;
class QMenu;
class QSystemTrayIcon;
class RuntimePetManager;

class SystemTrayController : public QObject
{
    Q_OBJECT

public:
    explicit SystemTrayController(RuntimePetManager *manager, QObject *parent = nullptr);
    ~SystemTrayController() override;

    bool isAvailable() const;
    void show();
    static bool controlCenterRequired(bool explicitlyRequested,
                                      bool firstRun,
                                      bool trayAvailable);

#ifdef DESKTOP_PET_TESTING
    QMenu *menuForTesting() const { return m_menu; }
#endif

private:
    void rebuildPetMenu();

    RuntimePetManager *m_manager {nullptr};
    QSystemTrayIcon *m_trayIcon {nullptr};
    QMenu *m_menu {nullptr};
    QMenu *m_petMenu {nullptr};
    QAction *m_pausePatrolAction {nullptr};
};

#endif // SYSTEMTRAYCONTROLLER_H

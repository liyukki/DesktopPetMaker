#ifndef PETCONTROLCENTERWINDOW_H
#define PETCONTROLCENTERWINDOW_H

#include <QMainWindow>
#include <QPointer>

#include "petproject.h"
#include "petprojectregistry.h"
#include "runtimepetmanager.h"

class QCheckBox;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTextEdit;
class QVBoxLayout;
class EditorWindow;
class ActionMaterialWindow;
class AIConversationConsoleWindow;

class PetControlCenterWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit PetControlCenterWindow(RuntimePetManager *manager, QWidget *parent = nullptr);

public slots:
    void showCenter();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    QWidget *createOverviewPage();
    QWidget *createMyPetsPage();
    QWidget *createActionsPage();
    QWidget *createAiConsolePage();
    QWidget *createAiRolePage();
    QWidget *createAiServicePage();
    QWidget *createRuntimeSettingsPage();
    QWidget *createAssetToolsPage();
    QWidget *createGeneralSettingsPage();

    void refreshAll();
    void refreshOverview();
    void refreshMyPets();
    void refreshAiService();
    void refreshAiRole();
    void refreshRuntimeSettings();
    void loadSelectedAiProfile();
    void addAiProfile();
    void copyAiProfile();
    void deleteAiProfile();
    void saveAiService();
    void testAiService();
    void saveAiRole();
    void applyRuntimeSettings();
    void applyGeneralSettings();
    void createPetProject();
    void importPetProject();
    void importPetpack();
    void copyPetProject(const QString &path);
    void deletePetProject(const PetProjectEntry &entry);
    void openPetMaker(const QString &path = QString());
    void openActionMaterial();
    void openAiConsole();
    void selectProject(const QString &path);
    PetProject selectedProject() const;
    QString selectedPetPath() const;
    void setRuntimeCheckboxesEnabled(bool enabled);
    QFrame *makePetCard(const PetProjectEntry &entry);

    RuntimePetManager *m_manager {nullptr};
    PetProjectRegistry m_registry;
    QString m_selectedProjectPath;
    QPointer<EditorWindow> m_editorWindow;
    QPointer<ActionMaterialWindow> m_actionWindow;
    QPointer<AIConversationConsoleWindow> m_aiConsoleWindow;

    QListWidget *m_navList {nullptr};
    QStackedWidget *m_pages {nullptr};

    QVBoxLayout *m_overviewLayout {nullptr};
    QLabel *m_runningCountLabel {nullptr};
    QVBoxLayout *m_myPetsLayout {nullptr};

    QListWidget *m_aiProfileList {nullptr};
    QLineEdit *m_profileNameEdit {nullptr};
    QComboBox *m_providerTypeCombo {nullptr};
    QComboBox *m_baseUrlCombo {nullptr};
    QLineEdit *m_apiKeyEdit {nullptr};
    QLineEdit *m_modelEdit {nullptr};
    QCheckBox *m_showApiKeyCheck {nullptr};
    QTextEdit *m_globalPromptEdit {nullptr};

    QLabel *m_rolePetLabel {nullptr};
    QLineEdit *m_roleNameEdit {nullptr};
    QTextEdit *m_rolePromptEdit {nullptr};
    QComboBox *m_roleProviderCombo {nullptr};
    QPushButton *m_saveRoleButton {nullptr};

    QCheckBox *m_topMostCheck {nullptr};
    QCheckBox *m_lockedCheck {nullptr};
    QCheckBox *m_mousePassthroughCheck {nullptr};
    QCheckBox *m_patrolEnabledCheck {nullptr};
    bool m_updatingRuntimeSettings {false};

    QCheckBox *m_restoreRunningPetsCheck {nullptr};
    QCheckBox *m_proactiveDefaultCheck {nullptr};
    QLineEdit *m_bubbleSecondsEdit {nullptr};
    QLineEdit *m_bubbleMaxCharsEdit {nullptr};
    QLineEdit *m_defaultMaxRespondersEdit {nullptr};
};

#endif // PETCONTROLCENTERWINDOW_H

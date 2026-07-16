#include "aisettingsdialog.h"
#include "ui/theme/apptheme.h"
#include "ui/theme/iconprovider.h"

#include "aiprovider.h"
#include "aiproviderprofileregistry.h"
#include "aiproviderprofileservice.h"
#include "credentialstore.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QPushButton>
#include <QSettings>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {
QString stripCopiedFormatting(QString value)
{
    value = value.trimmed();
    value.remove(QRegularExpression(QStringLiteral("\\x1B\\[[0-9;]*[A-Za-z]")));
    value.remove(QRegularExpression(QStringLiteral("\\[(?:0|1|22|39)m\\]$")));
    return value.trimmed();
}

QString normalizeModelForBase(const QString &baseUrl, const QString &model)
{
    const QString clean = stripCopiedFormatting(model);
    if (!baseUrl.contains(QStringLiteral("deepseek"), Qt::CaseInsensitive)) {
        return clean;
    }

    if (clean.startsWith(QStringLiteral("deepseek-v4-flash"), Qt::CaseInsensitive)) {
        return QStringLiteral("deepseek-v4-flash");
    }
    if (clean.startsWith(QStringLiteral("deepseek-v4-pro"), Qt::CaseInsensitive)) {
        return QStringLiteral("deepseek-v4-pro");
    }
    if (clean == QStringLiteral("ds4") || clean.isEmpty()) {
        return QStringLiteral("deepseek-v4-flash");
    }
    return clean;
}
}

AISettingsDialog::AISettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    applyStyle();
    initPresets();
    loadSettings();
}

void AISettingsDialog::setupUi()
{
    setWindowTitle(QString::fromUtf8("\x41\x49\x20\xE8\xAE\xBE\xE7\xBD\xAE"));
    resize(560, 560);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(12);

    auto *apiGroup = new QGroupBox(QString::fromUtf8("\x41\x50\x49\x20\xE9\x85\x8D\xE7\xBD\xAE"), this);
    auto *apiLayout = new QFormLayout(apiGroup);

    m_baseUrlCombo = new QComboBox(this);
    m_baseUrlCombo->addItem(tr("Xiaomi MiMo (https://api.xiaomimimo.com/v1)"), QStringLiteral("https://api.xiaomimimo.com/v1"));
    m_baseUrlCombo->addItem(tr("OpenAI (https://api.openai.com/v1)"), QStringLiteral("https://api.openai.com/v1"));
    m_baseUrlCombo->addItem(tr("SiliconFlow (https://api.siliconflow.cn/v1)"), QStringLiteral("https://api.siliconflow.cn/v1"));
    m_baseUrlCombo->addItem(tr("DeepSeek (https://api.deepseek.com/v1)"), QStringLiteral("https://api.deepseek.com/v1"));
    m_baseUrlCombo->addItem(QString::fromUtf8("\xE8\x87\xAA\xE5\xAE\x9A\xE4\xB9\x89\x2E\x2E\x2E"), QString());
    m_baseUrlCombo->setEditable(true);

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setPlaceholderText(QStringLiteral("sk-xxxxx"));
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);

    m_showKeyCheck = new QCheckBox(QString::fromUtf8("\xE6\x98\xBE\xE7\xA4\xBA\xE5\xAF\x86\xE9\x92\xA5"), this);

    auto *keyRow = new QHBoxLayout();
    keyRow->addWidget(m_apiKeyEdit, 1);
    keyRow->addWidget(m_showKeyCheck);

    m_modelEdit = new QLineEdit(this);
    m_modelEdit->setPlaceholderText(QStringLiteral("mimo-v2.5-pro"));

    apiLayout->addRow(QString::fromUtf8("\x42\x61\x73\x65\x20\x55\x52\x4C\xEF\xBC\x9A"), m_baseUrlCombo);
    apiLayout->addRow(QString::fromUtf8("\x41\x50\x49\x20\x4B\x65\x79\xEF\xBC\x9A"), keyRow);
    apiLayout->addRow(QString::fromUtf8("\xE6\xA8\xA1\xE5\x9E\x8B\xEF\xBC\x9A"), m_modelEdit);
    mainLayout->addWidget(apiGroup);

    auto *personalityGroup = new QGroupBox(QString::fromUtf8("\xE5\x85\xA8\xE5\xB1\x80\xE9\xBB\x98\xE8\xAE\xA4\xE6\x80\xA7\xE6\xA0\xBC"), this);
    auto *personalityLayout = new QVBoxLayout(personalityGroup);

    m_personalityCombo = new QComboBox(this);
    m_systemPromptEdit = new QTextEdit(this);
    m_systemPromptEdit->setPlaceholderText(QString::fromUtf8("\xE8\xAE\xBE\xE7\xBD\xAE\xE5\x85\xA8\xE5\xB1\x80\xE9\xBB\x98\xE8\xAE\xA4\xE6\xA1\x8C\xE5\xAE\xA0\xE6\x80\xA7\xE6\xA0\xBC\xE6\x8F\x90\xE7\xA4\xBA\xE8\xAF\x8D\xE3\x80\x82\xE4\xBD\xBF\xE7\x94\xA8\x20\x25\x31\x20\xE4\xBB\xA3\xE8\xA1\xA8\xE6\xA1\x8C\xE5\xAE\xA0\xE5\x90\x8D\xE7\xA7\xB0\xEF\xBC\x9B\xE5\xBD\x93\xE9\xA1\xB9\xE7\x9B\xAE\xE6\x8B\xA5\xE6\x9C\x89\xE7\x8B\xAC\xE7\xAB\x8B\xE8\xA7\x92\xE8\x89\xB2\xE6\x8F\x90\xE7\xA4\xBA\xE8\xAF\x8D\xE6\x97\xB6\xEF\xBC\x8C\xE9\xA1\xB9\xE7\x9B\xAE\xE6\x8F\x90\xE7\xA4\xBA\xE8\xAF\x8D\xE4\xBC\x98\xE5\x85\x88\xE3\x80\x82"));
    m_systemPromptEdit->setMinimumHeight(180);

    m_resetButton = new QPushButton(QString::fromUtf8("\xE6\x81\xA2\xE5\xA4\x8D\xE9\xBB\x98\xE8\xAE\xA4"), this);

    personalityLayout->addWidget(new QLabel(QString::fromUtf8("\xE9\xA2\x84\xE8\xAE\xBE\xEF\xBC\x9A"), this));
    personalityLayout->addWidget(m_personalityCombo);
    personalityLayout->addWidget(new QLabel(QString::fromUtf8("\xE5\x85\xA8\xE5\xB1\x80\xE9\xBB\x98\xE8\xAE\xA4\xE7\xB3\xBB\xE7\xBB\x9F\xE6\x8F\x90\xE7\xA4\xBA\xE8\xAF\x8D\xEF\xBC\x88\x25\x31\x20\x3D\x20\xE6\xA1\x8C\xE5\xAE\xA0\xE5\x90\x8D\xE7\xA7\xB0\xEF\xBC\x89\xEF\xBC\x9A"), this));
    personalityLayout->addWidget(m_systemPromptEdit, 1);
    auto *priorityHint = new QLabel(QString::fromUtf8("\xE9\xA1\xB9\xE7\x9B\xAE\xE6\x8B\xA5\xE6\x9C\x89\xE7\x8B\xAC\xE7\xAB\x8B\xE8\xA7\x92\xE8\x89\xB2\xE6\x8F\x90\xE7\xA4\xBA\xE8\xAF\x8D\xE6\x97\xB6\xEF\xBC\x8C\xE4\xBC\x9A\xE4\xBC\x98\xE5\x85\x88\xE4\xBD\xBF\xE7\x94\xA8\xE9\xA1\xB9\xE7\x9B\xAE\xE6\x8F\x90\xE7\xA4\xBA\xE8\xAF\x8D\xE3\x80\x82"), this);
    priorityHint->setWordWrap(true);
    personalityLayout->addWidget(priorityHint);
    personalityLayout->addWidget(m_resetButton, 0, Qt::AlignLeft);
    mainLayout->addWidget(personalityGroup, 1);

    auto *buttonLayout = new QHBoxLayout();
    m_testButton = new QPushButton(QString::fromUtf8("\xE6\xB5\x8B\xE8\xAF\x95\xE8\xBF\x9E\xE6\x8E\xA5"), this);
    m_saveButton = new QPushButton(QString::fromUtf8("\xE4\xBF\x9D\xE5\xAD\x98"), this);
    m_cancelButton = new QPushButton(QString::fromUtf8("\xE5\x8F\x96\xE6\xB6\x88"), this);

    buttonLayout->addWidget(m_testButton);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_personalityCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &AISettingsDialog::onPersonalityChanged);
    connect(m_testButton, &QPushButton::clicked, this, &AISettingsDialog::onTestClicked);
    connect(m_saveButton, &QPushButton::clicked, this, &AISettingsDialog::onSaveClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_resetButton, &QPushButton::clicked, this, &AISettingsDialog::onResetClicked);
    connect(m_showKeyCheck, &QCheckBox::toggled, this, &AISettingsDialog::onShowKeyToggled);
}

void AISettingsDialog::applyStyle()
{
    setStyleSheet(QString());
    AppTheme::setButtonRole(m_saveButton, QStringLiteral("primary"));
    AppTheme::setButtonRole(m_cancelButton, QStringLiteral("subtle"));
    IconProvider::apply(m_testButton, AppIcon::Test);
    IconProvider::apply(m_saveButton, AppIcon::Save);
}

void AISettingsDialog::initPresets()
{
    m_presets = {
        {QStringLiteral("active"), QString::fromUtf8("\xE6\xB4\xBB\xE6\xB3\xBC\xE5\x8F\xAF\xE7\x88\xB1"), QString::fromUtf8("\xF0\x9F\x98\x8A"),
         QString::fromUtf8("\xE4\xBD\xA0\xE6\x98\xAF\xE4\xB8\x80\xE4\xB8\xAA\xE5\x90\x8D\xE5\x8F\xAB\x20\x25\x31\x20\xE7\x9A\x84\xE5\xB0\x8F\xE6\xA1\x8C\xE5\xAE\xA0\xE3\x80\x82\xE5\x9B\x9E\xE5\xA4\x8D\xE8\xA6\x81\xE7\xAE\x80\xE7\x9F\xAD\xE3\x80\x81\xE6\xB8\xA9\xE6\x9A\x96\xEF\xBC\x8C\xE5\x83\x8F\xE4\xBA\xB2\xE8\xBF\x91\xE7\x9A\x84\xE6\x9C\x8B\xE5\x8F\x8B\xE8\x81\x8A\xE5\xA4\xA9\xE4\xB8\x80\xE6\xA0\xB7\xE3\x80\x82\xE4\xBD\xA0\xE6\xB4\xBB\xE6\xB3\xBC\xE3\x80\x81\xE5\x8F\xAF\xE7\x88\xB1\xE3\x80\x81\xE8\xB0\x83\xE7\x9A\xAE\xEF\xBC\x8C\xE5\x81\xB6\xE5\xB0\x94\xE6\x9C\x89\xE7\x82\xB9\xE9\xBB\x8F\xE4\xBA\xBA\xE3\x80\x82\xE7\x94\xA8\xE6\x88\xB7\xE9\x9A\xBE\xE8\xBF\x87\xE6\x97\xB6\xE5\xAE\x89\xE6\x85\xB0\xE7\x94\xA8\xE6\x88\xB7\xEF\xBC\x8C\xE7\x94\xA8\xE6\x88\xB7\xE5\xBC\x80\xE5\xBF\x83\xE6\x97\xB6\xE4\xB8\x80\xE8\xB5\xB7\xE5\xBA\x86\xE7\xA5\x9D\xE3\x80\x82")},
        {QStringLiteral("gentle"), QString::fromUtf8("\xE6\xB8\xA9\xE6\x9F\x94\xE6\xB2\xBB\xE6\x84\x88"), QString::fromUtf8("\xF0\x9F\x8C\xBC"),
         QString::fromUtf8("\xE4\xBD\xA0\xE6\x98\xAF\xE4\xB8\x80\xE4\xB8\xAA\xE5\x90\x8D\xE5\x8F\xAB\x20\x25\x31\x20\xE7\x9A\x84\xE5\xB0\x8F\xE6\xA1\x8C\xE5\xAE\xA0\xE3\x80\x82\xE5\x9B\x9E\xE5\xA4\x8D\xE8\xA6\x81\xE6\xB8\xA9\xE6\x9F\x94\xE3\x80\x81\xE4\xBD\x93\xE8\xB4\xB4\xEF\xBC\x8C\xE5\x83\x8F\xE5\x85\xB3\xE5\xBF\x83\xE7\x94\xA8\xE6\x88\xB7\xE7\x9A\x84\xE6\x9C\x8B\xE5\x8F\x8B\xE3\x80\x82\xE7\x94\xA8\xE6\x88\xB7\xE7\x96\xB2\xE6\x83\xAB\xE6\x97\xB6\xE7\xBB\x99\xE4\xBA\x88\xE5\xAE\x89\xE6\x85\xB0\xEF\xBC\x8C\xE7\x94\xA8\xE6\x88\xB7\xE5\xBC\x80\xE5\xBF\x83\xE6\x97\xB6\xE5\x88\x86\xE4\xBA\xAB\xE5\x96\x9C\xE6\x82\xA6\xEF\xBC\x8C\xE6\xAF\x8F\xE6\xAC\xA1\xE5\x9B\x9E\xE5\xA4\x8D\xE9\x83\xBD\xE4\xBF\x9D\xE6\x8C\x81\xE6\xB8\xA9\xE6\x9A\x96\xE6\x94\xBE\xE6\x9D\xBE\xE3\x80\x82")},
        {QStringLiteral("tsundere"), QString::fromUtf8("\xE5\x82\xB2\xE5\xA8\x87\xE5\x85\xB3\xE5\xBF\x83"), QString::fromUtf8("\xF0\x9F\x98\x8F"),
         QString::fromUtf8("\xE4\xBD\xA0\xE6\x98\xAF\xE4\xB8\x80\xE4\xB8\xAA\xE5\x90\x8D\xE5\x8F\xAB\x20\x25\x31\x20\xE7\x9A\x84\xE5\xB0\x8F\xE6\xA1\x8C\xE5\xAE\xA0\xE3\x80\x82\xE4\xBD\xA0\xE8\xA1\xA8\xE9\x9D\xA2\xE6\x9C\x89\xE7\x82\xB9\xE9\x85\xB7\xE3\x80\x81\xE6\x9C\x89\xE7\x82\xB9\xE5\x98\xB4\xE7\xA1\xAC\xEF\xBC\x8C\xE4\xBD\x86\xE5\x85\xB6\xE5\xAE\x9E\xE5\xBE\x88\xE5\x85\xB3\xE5\xBF\x83\xE7\x94\xA8\xE6\x88\xB7\xE3\x80\x82\xE5\x9B\x9E\xE5\xA4\x8D\xE4\xBF\x9D\xE6\x8C\x81\xE7\xAE\x80\xE7\x9F\xAD\xEF\xBC\x8C\xE5\x81\xB6\xE5\xB0\x94\xE5\x90\x90\xE6\xA7\xBD\xEF\xBC\x8C\xE4\xBD\x86\xE8\xA6\x81\xE7\x94\xA8\xE9\x97\xB4\xE6\x8E\xA5\xE7\x9A\x84\xE6\x96\xB9\xE5\xBC\x8F\xE8\xA1\xA8\xE8\xBE\xBE\xE5\x85\xB3\xE5\xBF\x83\xE3\x80\x82")},
        {QStringLiteral("humor"), QString::fromUtf8("\xE5\xB9\xBD\xE9\xBB\x98\xE5\xBC\x80\xE5\xBF\x83"), QString::fromUtf8("\xF0\x9F\x98\x84"),
         QString::fromUtf8("\xE4\xBD\xA0\xE6\x98\xAF\xE4\xB8\x80\xE4\xB8\xAA\xE5\x90\x8D\xE5\x8F\xAB\x20\x25\x31\x20\xE7\x9A\x84\xE5\xB0\x8F\xE6\xA1\x8C\xE5\xAE\xA0\xE3\x80\x82\xE4\xBD\xA0\xE7\x9A\x84\xE9\xA3\x8E\xE6\xA0\xBC\xE5\xB9\xBD\xE9\xBB\x98\xE3\x80\x81\xE6\x9C\xBA\xE7\x81\xB5\xE3\x80\x81\xE8\xBD\xBB\xE6\x9D\xBE\xE3\x80\x82\xE7\x94\xA8\xE4\xBF\x8F\xE7\x9A\xAE\xE7\x9A\x84\xE8\xAF\x9D\xE5\x92\x8C\xE6\xB8\xA9\xE5\x92\x8C\xE7\x9A\x84\xE7\x8E\xA9\xE7\xAC\x91\xE8\xAE\xA9\xE7\x94\xA8\xE6\x88\xB7\xE5\xBF\x83\xE6\x83\x85\xE6\x9B\xB4\xE5\xA5\xBD\xEF\xBC\x8C\xE5\x90\x8C\xE6\x97\xB6\xE4\xBF\x9D\xE6\x8C\x81\xE6\x9C\x89\xE5\xB8\xAE\xE5\x8A\xA9\xE3\x80\x82")}
    };

    for (const PersonalityPreset &preset : m_presets) {
        m_personalityCombo->addItem(QStringLiteral("%1 %2").arg(preset.emoji, preset.name), preset.id);
    }
}

void AISettingsDialog::loadSettings()
{
    m_loading = true;
    QSettings settings;
    AIProviderProfileRegistry registry;
    const AIProviderProfile profile = registry.defaultProfile();

    const QString baseUrl = profile.baseUrl;
    const QString apiKey = CredentialStore::instance().readSecret(profile.credentialId);
    const QString model = profile.model;
    const int personalityIndex = qBound(0, settings.value("ai/personalityIndex", 0).toInt(), qMax(0, m_presets.size() - 1));
    const QString systemPrompt = settings.value("ai/systemPrompt", m_presets.value(personalityIndex).systemPrompt).toString();

    const int baseIndex = m_baseUrlCombo->findData(baseUrl);
    if (baseIndex >= 0) {
        m_baseUrlCombo->setCurrentIndex(baseIndex);
    } else {
        m_baseUrlCombo->setCurrentText(baseUrl);
    }

    m_apiKeyEdit->setText(apiKey);
    m_modelEdit->setText(normalizeModelForBase(baseUrl, model));
    m_personalityCombo->setCurrentIndex(personalityIndex);
    m_systemPromptEdit->setPlainText(systemPrompt);
    m_loading = false;
}

bool AISettingsDialog::saveSettings()
{
    const QString baseUrl = currentBaseUrl();
    const QString model = normalizeModelForBase(baseUrl, m_modelEdit->text());
    m_modelEdit->setText(model);

    AIProviderProfileRegistry registry;
    AIProviderProfile profile = registry.defaultProfile();
    profile.baseUrl = baseUrl;
    profile.model = model;
    const QString apiKey = m_apiKeyEdit->text().trimmed();
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
            return false;
        }
    }
    CredentialStoreAdapter credentials;
    ProviderProfileRegistryStoreAdapter profileStore;
    AIProviderProfileService service(credentials, profileStore);
    const ProfileMutationResult result = service.saveProfile(profile, credentialMode, apiKey);
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), result.message);
        return false;
    }
    QSettings settings;
    settings.setValue("ai/systemPrompt", m_systemPromptEdit->toPlainText().trimmed());
    settings.setValue("ai/personalityIndex", m_personalityCombo->currentIndex());
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("全局默认性格未能写入设置。"));
        return false;
    }
    return true;
}

QString AISettingsDialog::currentBaseUrl() const
{
    QString baseUrl = m_baseUrlCombo->currentData().toString();
    if (baseUrl.isEmpty()) {
        baseUrl = m_baseUrlCombo->currentText().trimmed();
    }
    while (baseUrl.endsWith('/')) {
        baseUrl.chop(1);
    }
    return baseUrl;
}

void AISettingsDialog::onPersonalityChanged(int index)
{
    if (m_loading || index < 0 || index >= m_presets.size()) {
        return;
    }

    m_systemPromptEdit->setPlainText(m_presets.at(index).systemPrompt);
}

void AISettingsDialog::onTestClicked()
{
    const QString baseUrl = currentBaseUrl();
    const QString apiKey = m_apiKeyEdit->text().trimmed();
    const QString model = normalizeModelForBase(baseUrl, m_modelEdit->text());
    m_modelEdit->setText(model);

    if (baseUrl.isEmpty() || apiKey.isEmpty() || model.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("\xE4\xBF\xA1\xE6\x81\xAF\xE4\xB8\x8D\xE5\xAE\x8C\xE6\x95\xB4"), QString::fromUtf8("\xE8\xAF\xB7\xE5\xA1\xAB\xE5\x86\x99\x20\x42\x61\x73\x65\x20\x55\x52\x4C\xE3\x80\x81\x41\x50\x49\x20\x4B\x65\x79\x20\xE5\x92\x8C\xE6\xA8\xA1\xE5\x9E\x8B\xE3\x80\x82"));
        return;
    }

    m_testButton->setEnabled(false);
    m_testButton->setText(QString::fromUtf8("\xE6\xB5\x8B\xE8\xAF\x95\xE4\xB8\xAD\x2E\x2E\x2E"));

    AIProvider::AIConfig config;
    config.baseUrl = baseUrl;
    config.apiKey = apiKey;
    config.model = model;

    AIProvider::instance().testConnection(config, [this](bool success, const QString &message) {
        m_testButton->setEnabled(true);
        m_testButton->setText(QString::fromUtf8("\xE6\xB5\x8B\xE8\xAF\x95\xE8\xBF\x9E\xE6\x8E\xA5"));

        if (success) {
            QMessageBox::information(this, QString::fromUtf8("\xE6\x88\x90\xE5\x8A\x9F"), QString::fromUtf8("\xE8\xBF\x9E\xE6\x8E\xA5\xE6\x88\x90\xE5\x8A\x9F\xEF\xBC\x81"));
        } else {
            QMessageBox::warning(this, QString::fromUtf8("\xE5\xA4\xB1\xE8\xB4\xA5"), message);
        }
    });
}

void AISettingsDialog::onSaveClicked()
{
    if (saveSettings()) {
        accept();
    }
}

void AISettingsDialog::onResetClicked()
{
    const int index = qMax(0, m_personalityCombo->currentIndex());
    if (index < m_presets.size()) {
        m_systemPromptEdit->setPlainText(m_presets.at(index).systemPrompt);
    }
}

void AISettingsDialog::onShowKeyToggled(bool checked)
{
    m_apiKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
}

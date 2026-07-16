#include "journalwindow.h"
#include "ui/theme/apptheme.h"
#include "ui/theme/iconprovider.h"

#include "moodjournal.h"

#include <QCalendarWidget>
#include <QComboBox>
#include <QDate>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

JournalWindow::JournalWindow(QWidget *parent)
    : QDialog(parent)
    , m_currentDate(QDate::currentDate())
{
    setupUi();
    applyStyle();

    connect(m_prevButton, &QPushButton::clicked, this, &JournalWindow::onPrevDay);
    connect(m_nextButton, &QPushButton::clicked, this, &JournalWindow::onNextDay);
    connect(m_saveButton, &QPushButton::clicked, this, &JournalWindow::onSaveClicked);
    connect(m_moodCombo, &QComboBox::currentTextChanged, this, &JournalWindow::onMoodChanged);
    connect(m_contentEdit, &QTextEdit::textChanged, this, &JournalWindow::onContentChanged);
    connect(m_calendar, &QCalendarWidget::selectionChanged, this, &JournalWindow::onDateSelected);
    connect(&MoodJournal::instance(), &MoodJournal::entryUpdated, this, [this](const MoodJournal::JournalEntry &entry) {
        if (entry.date == m_currentDate && !m_updating) {
            loadEntry(m_currentDate);
        }
    });

    loadEntry(m_currentDate);
}

void JournalWindow::setupUi()
{
    setWindowTitle(tr("📔 心情日记"));
    resize(680, 580);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    m_calendar = new QCalendarWidget(this);
    m_calendar->setSelectedDate(m_currentDate);
    m_calendar->setGridVisible(false);
    root->addWidget(m_calendar, 0);

    auto *right = new QVBoxLayout();
    right->setSpacing(10);

    auto *dayRow = new QHBoxLayout();
    m_prevButton = new QPushButton(QStringLiteral("◀"), this);
    m_nextButton = new QPushButton(QStringLiteral("▶"), this);
    m_dateLabel = new QLabel(this);
    m_dateLabel->setAlignment(Qt::AlignCenter);
    dayRow->addWidget(m_prevButton);
    dayRow->addWidget(m_dateLabel, 1);
    dayRow->addWidget(m_nextButton);

    auto *moodRow = new QHBoxLayout();
    auto *moodLabel = new QLabel(tr("心情:"), this);
    m_moodCombo = new QComboBox(this);
    m_moodCombo->addItems(m_moods);
    moodRow->addWidget(moodLabel);
    moodRow->addWidget(m_moodCombo, 1);

    m_contentEdit = new QTextEdit(this);
    m_contentEdit->setPlaceholderText(tr("写下今天发生的事..."));
    m_contentEdit->setFrameShape(QFrame::NoFrame);

    m_statsLabel = new QLabel(this);
    m_eventsList = new QListWidget(this);
    m_saveButton = new QPushButton(tr("保存"), this);

    right->addLayout(dayRow);
    right->addLayout(moodRow);
    right->addWidget(m_contentEdit, 2);
    right->addWidget(new QLabel(tr("今日事件:"), this));
    right->addWidget(m_eventsList, 1);
    right->addWidget(m_statsLabel);
    right->addWidget(m_saveButton, 0, Qt::AlignCenter);
    root->addLayout(right, 1);
}

void JournalWindow::applyStyle()
{
    setStyleSheet(QString());
    AppTheme::setButtonRole(m_saveButton, QStringLiteral("primary"));
    IconProvider::apply(m_saveButton, AppIcon::Save);
}

void JournalWindow::onPrevDay()
{
    saveCurrentEntry();
    loadEntry(m_currentDate.addDays(-1));
}

void JournalWindow::onNextDay()
{
    saveCurrentEntry();
    loadEntry(m_currentDate.addDays(1));
}

void JournalWindow::onSaveClicked()
{
    saveCurrentEntry();
}

void JournalWindow::onMoodChanged(const QString &)
{
    if (!m_updating) {
        saveCurrentEntry();
    }
}

void JournalWindow::onContentChanged()
{
    if (!m_updating) {
        saveCurrentEntry();
    }
}

void JournalWindow::onDateSelected()
{
    if (m_updating) {
        return;
    }

    saveCurrentEntry();
    loadEntry(m_calendar->selectedDate());
}

void JournalWindow::loadEntry(const QDate &date)
{
    m_currentDate = date;
    updateDisplay();
}

void JournalWindow::updateDisplay()
{
    const MoodJournal::JournalEntry entry = MoodJournal::instance().entry(m_currentDate);

    m_updating = true;
    m_calendar->setSelectedDate(m_currentDate);
    m_dateLabel->setText(m_currentDate.toString(QStringLiteral("yyyy年M月d日")));

    int moodIndex = m_moodCombo->findText(entry.mood);
    if (moodIndex < 0) {
        moodIndex = m_moodCombo->findText(QStringLiteral("😐 中性"));
    }
    m_moodCombo->setCurrentIndex(qMax(0, moodIndex));
    m_contentEdit->setPlainText(entry.content);
    m_statsLabel->setText(tr("对话 %1 次    被点击 %2 次").arg(entry.chatCount).arg(entry.keypressCount));

    m_eventsList->clear();
    if (entry.events.isEmpty()) {
        m_eventsList->addItem(tr("• 暂无事件"));
    } else {
        for (const QString &event : entry.events) {
            m_eventsList->addItem(QStringLiteral("• %1").arg(event));
        }
    }
    m_updating = false;
}

void JournalWindow::saveCurrentEntry()
{
    m_updating = true;
    MoodJournal::instance().updateEntry(m_currentDate, m_moodCombo->currentText(), m_contentEdit->toPlainText());
    m_updating = false;
}

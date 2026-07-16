#ifndef JOURNALWINDOW_H
#define JOURNALWINDOW_H

#include <QDate>
#include <QDialog>
#include <QStringList>

class QCalendarWidget;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTextEdit;

class JournalWindow : public QDialog
{
    Q_OBJECT

public:
    explicit JournalWindow(QWidget *parent = nullptr);

private:
    void setupUi();
    void applyStyle();
    void onPrevDay();
    void onNextDay();
    void onSaveClicked();
    void onMoodChanged(const QString &mood);
    void onContentChanged();
    void onDateSelected();
    void loadEntry(const QDate &date);
    void updateDisplay();
    void saveCurrentEntry();

    QLabel *m_dateLabel {nullptr};
    QLabel *m_statsLabel {nullptr};
    QPushButton *m_prevButton {nullptr};
    QPushButton *m_nextButton {nullptr};
    QPushButton *m_saveButton {nullptr};
    QComboBox *m_moodCombo {nullptr};
    QTextEdit *m_contentEdit {nullptr};
    QListWidget *m_eventsList {nullptr};
    QCalendarWidget *m_calendar {nullptr};

    QDate m_currentDate;
    bool m_updating {false};
    QStringList m_moods {
        QStringLiteral("😊 开心"),
        QStringLiteral("😌 平静"),
        QStringLiteral("😐 中性"),
        QStringLiteral("😴 疲惫"),
        QStringLiteral("😢 难过"),
        QStringLiteral("😣 紧张")
    };
};

#endif // JOURNALWINDOW_H

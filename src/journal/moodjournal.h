#ifndef MOODJOURNAL_H
#define MOODJOURNAL_H

#include <QDate>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QTimer;

class MoodJournal : public QObject
{
    Q_OBJECT

public:
    struct JournalEntry {
        QDate date;
        QString mood;
        QString content;
        QStringList events;
        int chatCount {0};
        int keypressCount {0};
    };

    static MoodJournal &instance();

    JournalEntry todayEntry() const;
    JournalEntry entry(const QDate &date) const;
    QVector<JournalEntry> allEntries() const;
    QVector<JournalEntry> entriesInRange(const QDate &from, const QDate &to) const;
    void startJournalReminder();
    bool claimReminderForToday();

public slots:
    void updateTodayMood(const QString &mood);
    void updateTodayContent(const QString &content);
    void updateEntry(const QDate &date, const QString &mood, const QString &content);
    void addTodayEvent(const QString &event);
    void incrementChatCount();
    void setKeypressCount(int count);
    void save() const;
    void load();

signals:
    void entryUpdated(const MoodJournal::JournalEntry &entry);
    void journalReminderDue();

private:
    explicit MoodJournal(QObject *parent = nullptr);
    JournalEntry ensureEntry(const QDate &date);
    void storeEntry(const JournalEntry &entry);
    void checkJournalReminder();

    QMap<QDate, JournalEntry> m_entries;
    QString m_dataPath;
    QTimer *m_reminderTimer {nullptr};

    static constexpr const char *kJournalFile = "mood_journal.json";
};

Q_DECLARE_METATYPE(MoodJournal::JournalEntry)

#endif // MOODJOURNAL_H

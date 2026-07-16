#include "moodjournal.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QTime>
#include <QTimer>

namespace {

QString defaultMood()
{
    return QStringLiteral("😐 中性");
}

QJsonObject entryToJson(const MoodJournal::JournalEntry &entry)
{
    QJsonObject obj;
    obj.insert("mood", entry.mood);
    obj.insert("content", entry.content);
    obj.insert("chatCount", entry.chatCount);
    obj.insert("keypressCount", entry.keypressCount);

    QJsonArray events;
    for (const QString &event : entry.events) {
        events.append(event);
    }
    obj.insert("events", events);
    return obj;
}

MoodJournal::JournalEntry entryFromJson(const QDate &date, const QJsonObject &obj)
{
    MoodJournal::JournalEntry entry;
    entry.date = date;
    entry.mood = obj.value("mood").toString(defaultMood());
    entry.content = obj.value("content").toString();
    entry.chatCount = obj.value("chatCount").toInt(0);
    entry.keypressCount = obj.value("keypressCount").toInt(0);

    const QJsonArray events = obj.value("events").toArray();
    for (const QJsonValue &value : events) {
        const QString event = value.toString().trimmed();
        if (!event.isEmpty()) {
            entry.events.append(event);
        }
    }
    return entry;
}

} // namespace

MoodJournal &MoodJournal::instance()
{
    static MoodJournal journal;
    return journal;
}

MoodJournal::MoodJournal(QObject *parent)
    : QObject(parent)
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (basePath.isEmpty()) {
        basePath = QDir::home().filePath(QStringLiteral(".desktop_pet_maker"));
    }
    QDir().mkpath(basePath);
    m_dataPath = QDir(basePath).filePath(QString::fromLatin1(kJournalFile));
    load();
}

MoodJournal::JournalEntry MoodJournal::todayEntry() const
{
    return entry(QDate::currentDate());
}

MoodJournal::JournalEntry MoodJournal::entry(const QDate &date) const
{
    if (m_entries.contains(date)) {
        return m_entries.value(date);
    }

    JournalEntry entry;
    entry.date = date;
    entry.mood = defaultMood();
    return entry;
}

QVector<MoodJournal::JournalEntry> MoodJournal::allEntries() const
{
    QVector<JournalEntry> result;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        result.append(it.value());
    }
    return result;
}

QVector<MoodJournal::JournalEntry> MoodJournal::entriesInRange(const QDate &from, const QDate &to) const
{
    QVector<JournalEntry> result;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        if (it.key() >= from && it.key() <= to) {
            result.append(it.value());
        }
    }
    return result;
}

void MoodJournal::updateTodayMood(const QString &mood)
{
    JournalEntry entry = ensureEntry(QDate::currentDate());
    entry.mood = mood.trimmed().isEmpty() ? defaultMood() : mood.trimmed();
    storeEntry(entry);
}

void MoodJournal::updateTodayContent(const QString &content)
{
    JournalEntry entry = ensureEntry(QDate::currentDate());
    entry.content = content;
    storeEntry(entry);
}

void MoodJournal::updateEntry(const QDate &date, const QString &mood, const QString &content)
{
    JournalEntry entry = ensureEntry(date);
    entry.mood = mood.trimmed().isEmpty() ? defaultMood() : mood.trimmed();
    entry.content = content;
    storeEntry(entry);
}

void MoodJournal::addTodayEvent(const QString &event)
{
    const QString trimmed = event.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    JournalEntry entry = ensureEntry(QDate::currentDate());
    entry.events.append(trimmed);
    storeEntry(entry);
}

void MoodJournal::incrementChatCount()
{
    JournalEntry entry = ensureEntry(QDate::currentDate());
    ++entry.chatCount;
    entry.events.append(QStringLiteral("聊天消息 %1").arg(entry.chatCount));
    storeEntry(entry);
}

void MoodJournal::setKeypressCount(int count)
{
    JournalEntry entry = ensureEntry(QDate::currentDate());
    entry.keypressCount = qMax(0, count);
    storeEntry(entry);
}

void MoodJournal::startJournalReminder()
{
    if (!m_reminderTimer) {
        m_reminderTimer = new QTimer(this);
        m_reminderTimer->setInterval(60000);
        connect(m_reminderTimer, &QTimer::timeout, this, [this]() {
            checkJournalReminder();
        });
        m_reminderTimer->start();
    }

    checkJournalReminder();
}

bool MoodJournal::claimReminderForToday()
{
    const QDate today = QDate::currentDate();
    if (QTime::currentTime() < QTime(22, 0)) {
        return false;
    }

    QSettings settings;
    const QDate lastReminderDate = QDate::fromString(settings.value(QStringLiteral("journal/lastReminderDate")).toString(),
                                                    Qt::ISODate);
    if (lastReminderDate == today) {
        return false;
    }

    settings.setValue(QStringLiteral("journal/lastReminderDate"), today.toString(Qt::ISODate));
    return true;
}

void MoodJournal::save() const
{
    QJsonObject entriesObj;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        entriesObj.insert(it.key().toString(Qt::ISODate), entryToJson(it.value()));
    }

    QJsonObject root;
    root.insert("entries", entriesObj);

    QFile file(m_dataPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void MoodJournal::load()
{
    QFile file(m_dataPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    m_entries.clear();
    const QJsonObject entriesObj = doc.object().value("entries").toObject();
    for (const QString &dateText : entriesObj.keys()) {
        const QDate date = QDate::fromString(dateText, Qt::ISODate);
        if (!date.isValid()) {
            continue;
        }
        m_entries.insert(date, entryFromJson(date, entriesObj.value(dateText).toObject()));
    }
}

MoodJournal::JournalEntry MoodJournal::ensureEntry(const QDate &date)
{
    JournalEntry entry = this->entry(date);
    if (!entry.date.isValid()) {
        entry.date = date;
    }
    return entry;
}

void MoodJournal::storeEntry(const JournalEntry &entry)
{
    if (!entry.date.isValid()) {
        return;
    }

    m_entries.insert(entry.date, entry);
    save();
    emit entryUpdated(entry);
}

void MoodJournal::checkJournalReminder()
{
    const QDate today = QDate::currentDate();
    if (QTime::currentTime() < QTime(22, 0)) {
        return;
    }

    QSettings settings;
    const QDate lastReminderDate = QDate::fromString(settings.value(QStringLiteral("journal/lastReminderDate")).toString(),
                                                    Qt::ISODate);
    if (lastReminderDate != today) {
        emit journalReminderDue();
    }
}

#include <QtTest>

#include "petproject.h"

#include <QProcess>
#include <QTemporaryDir>

namespace {
QString psQuote(QString value)
{
    value.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("'%1'").arg(value);
}

bool makeZip(const QString &path, const QString &entry, qint64 size = 1, qint32 attributes = 0)
{
    const QString script = QStringLiteral(
        "Add-Type -AssemblyName System.IO.Compression;"
        "$f=[IO.File]::Open(%1,[IO.FileMode]::Create);"
        "$z=[IO.Compression.ZipArchive]::new($f,[IO.Compression.ZipArchiveMode]::Create);"
        "try{$e=$z.CreateEntry(%2,[IO.Compression.CompressionLevel]::Optimal);$e.ExternalAttributes=%4;"
        "$s=$e.Open();try{$b=New-Object byte[] 8192;[int64]$r=%3;while($r-gt 0){$n=[Math]::Min($r,$b.Length);$s.Write($b,0,$n);$r-=$n}}finally{$s.Dispose()}}"
        "finally{$z.Dispose();$f.Dispose()}")
        .arg(psQuote(path), psQuote(entry), QString::number(size), QString::number(attributes));
    return QProcess::execute(QStringLiteral("powershell.exe"),
                             {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                              QStringLiteral("-Command"), script}) == 0;
}

bool makeZipWithEntryCount(const QString &path, int count)
{
    const QString script = QStringLiteral(
        "Add-Type -AssemblyName System.IO.Compression;"
        "$f=[IO.File]::Open(%1,[IO.FileMode]::Create);"
        "$z=[IO.Compression.ZipArchive]::new($f,[IO.Compression.ZipArchiveMode]::Create);"
        "try{for($i=0;$i-lt %2;$i++){$e=$z.CreateEntry(('f{0:D4}.txt'-f $i));$s=$e.Open();$s.WriteByte(1);$s.Dispose()}}"
        "finally{$z.Dispose();$f.Dispose()}")
        .arg(psQuote(path)).arg(count);
    return QProcess::execute(QStringLiteral("powershell.exe"),
                             {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                              QStringLiteral("-Command"), script}) == 0;
}

class RejectingArchiveRunner final : public IArchiveCommandRunner
{
public:
    QString failure {QStringLiteral("injected archive rejection")};
    int calls {0};
    bool run(const QString &, QString *errorMessage) override
    {
        ++calls;
        if (errorMessage) *errorMessage = failure;
        return false;
    }
};
}

class PetpackSecurityTests : public QObject
{
    Q_OBJECT
private slots:
    void cleanup() { PetProject::setArchiveCommandRunnerForTesting(nullptr); }

    void maliciousEntry_data()
    {
        QTest::addColumn<QString>("entry");
        QTest::addColumn<qint64>("size");
        QTest::addColumn<qint32>("attributes");
        QTest::newRow("traversal") << QStringLiteral("../escape.exe") << qint64(1) << qint32(0);
        QTest::newRow("absolute") << QStringLiteral("/absolute/pet.json") << qint64(1) << qint32(0);
        QTest::newRow("drive") << QStringLiteral("C:/pet/pet.json") << qint64(1) << qint32(0);
        QTest::newRow("unc") << QStringLiteral("//server/share/pet.json") << qint64(1) << qint32(0);
        QTest::newRow("script") << QStringLiteral("assets/install.ps1") << qint64(1) << qint32(0);
        QTest::newRow("deep") << QStringLiteral("a/b/c/d/e/f/g/h/i/j/k/l/m/pet.json") << qint64(1) << qint32(0);
        QTest::newRow("long") << (QString(241, QLatin1Char('a')) + QStringLiteral(".txt")) << qint64(1) << qint32(0);
        QTest::newRow("symlink") << QStringLiteral("assets/link.txt") << qint64(1) << qint32(0xA0000000u);
        QTest::newRow("compression-bomb") << QStringLiteral("assets/bomb.txt") << qint64(2 * 1024 * 1024) << qint32(0);
        QTest::newRow("oversized") << QStringLiteral("assets/oversized.txt") << qint64(64LL * 1024 * 1024 + 1) << qint32(0);
    }

    void maliciousEntry()
    {
        QFETCH(QString, entry);
        QFETCH(qint64, size);
        QFETCH(qint32, attributes);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString archive = dir.filePath(QStringLiteral("attack.petpack"));
        QVERIFY(makeZip(archive, entry, size, attributes));
        QString error;
        QVERIFY2(!PetProject::validatePetpackArchiveForTesting(archive, &error), qPrintable(error));
    }

    void archiveRunnerFailuresPropagate()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString archive = dir.filePath(QStringLiteral("fixture.petpack"));
        QFile file(archive);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("zip"), qint64(3));
        file.close();

        RejectingArchiveRunner runner;
        PetProject::setArchiveCommandRunnerForTesting(&runner);
        const QStringList failures {
            QStringLiteral("No PowerShell executable found"),
            QStringLiteral("pwsh and fallback failed"),
            QStringLiteral("archive command timed out")
        };
        for (const QString &failure : failures) {
            runner.failure = failure;
            QString error;
            QVERIFY(!PetProject::validatePetpackArchiveForTesting(archive, &error));
            QVERIFY(error.contains(failure));
        }
        QCOMPARE(runner.calls, failures.size());
    }

    void fileCountLimitAndExtractedJunctionAreRejected()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString error;
        const QString archive = dir.filePath(QStringLiteral("too-many.petpack"));
        QVERIFY(makeZipWithEntryCount(archive, 4097));
        QVERIFY(!PetProject::validatePetpackArchiveForTesting(archive, &error));

        const QString root = dir.filePath(QStringLiteral("extract"));
        const QString outside = dir.filePath(QStringLiteral("outside"));
        const QString junction = QDir(root).filePath(QStringLiteral("escape"));
        QVERIFY(QDir().mkpath(root));
        QVERIFY(QDir().mkpath(outside));
        const QString command = QStringLiteral("New-Item -ItemType Junction -Path %1 -Target %2|Out-Null")
                                    .arg(psQuote(junction), psQuote(outside));
        QCOMPARE(QProcess::execute(QStringLiteral("powershell.exe"),
                                   {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                                    QStringLiteral("-Command"), command}), 0);
        error.clear();
        QVERIFY(!PetProject::validateExtractedPetpackTreeForTesting(root, &error));
    }
};

QTEST_MAIN(PetpackSecurityTests)
#include "petpack_security_tests.moc"

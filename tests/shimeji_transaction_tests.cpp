#include <QtTest>

#include "shimejiimportwizard.h"
#include "test_support.h"

class FailingFileSystem final : public IAssetTransactionFileSystem
{
public:
    enum class Failure { Rename, Cover, Save };
    explicit FailingFileSystem(Failure failure) : m_failure(failure) {}

    bool renameDirectory(const QString &from, const QString &to) override
    {
        return m_failure != Failure::Rename && QDir().rename(from, to);
    }
    bool removeTree(const QString &path) override { return !QFileInfo::exists(path) || QDir(path).removeRecursively(); }
    bool updateAutoCover(PetProject &project, QString *errorMessage) override
    {
        if (m_failure == Failure::Cover) {
            if (errorMessage) *errorMessage = QStringLiteral("Injected cover failure");
            return false;
        }
        return project.updateAutoCover(errorMessage);
    }
    bool saveProject(PetProject &project, QString *errorMessage) override
    {
        if (m_failure == Failure::Save) {
            if (errorMessage) *errorMessage = QStringLiteral("Injected save failure");
            return false;
        }
        return project.save(errorMessage);
    }

private:
    Failure m_failure;
};

class ShimejiTransactionTests : public QObject
{
    Q_OBJECT
private slots:
    void cleanup() { ShimejiImportWizard::setAssetTransactionFileSystemForTesting(nullptr); }
    void malformedXmlReportsLineAndColumn()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("actions.xml");
        QVERIFY(TestSupport::writeUtf8(path, QByteArrayLiteral("<Actions><Action Name='Broken'>")));
        QStringList unsupported;
        const auto actions = ShimejiImportWizard::parseActions(path, &unsupported);
        QVERIFY(actions.isEmpty());
        const QString error = unsupported.join(QLatin1Char('\n'));
        QVERIFY(error.contains(QStringLiteral("XML_ERROR line")));
        QVERIFY(error.contains(QStringLiteral("column"), Qt::CaseInsensitive));
    }

    void transactionFailureRollsBackProjectCoverAndStaging_data()
    {
        QTest::addColumn<int>("failure");
        QTest::newRow("rename") << int(FailingFileSystem::Failure::Rename);
        QTest::newRow("cover") << int(FailingFileSystem::Failure::Cover);
        QTest::newRow("save") << int(FailingFileSystem::Failure::Save);
    }

    void transactionFailureRollsBackProjectCoverAndStaging()
    {
        QFETCH(int, failure);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PetProject project;
        project.name = QStringLiteral("ShimejiFixture");
        project.projectDir = dir.path();
        project.canvasSize = QSize(20, 24);
        project.anchor = QPoint(10, 24);
        QString error;
        QVERIFY(project.save(&error));
        const QByteArray beforeJson = TestSupport::readBytes(project.petJsonPath());
        const QString source = TestSupport::writePng(dir.filePath("pose.png"), QSize(20, 24));
        QVERIFY(!source.isEmpty());
        ShimejiActionImport action;
        action.name = QStringLiteral("Wave");
        action.type = QStringLiteral("Stay");
        action.poses.append({QStringLiteral("pose.png"), QPoint(10, 24), {}, 125});
        FailingFileSystem failing(static_cast<FailingFileSystem::Failure>(failure));
        ShimejiImportWizard::setAssetTransactionFileSystemForTesting(&failing);
        const ShimejiBatchImportResult result = ShimejiImportWizard::importPackageToProject(project, dir.path(), {action});
        QVERIFY(!result.ok);
        QCOMPARE(TestSupport::readBytes(project.petJsonPath()), beforeJson);
        QVERIFY(!QFileInfo::exists(dir.filePath("assets/shimeji_wave")));
        const QDir staging(dir.filePath(".asset_staging"));
        QVERIFY(staging.entryList({QStringLiteral("shimeji_batch_*")}, QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    }
};

QTEST_MAIN(ShimejiTransactionTests)
#include "shimeji_transaction_tests.moc"

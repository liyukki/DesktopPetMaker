#include <QtTest>

#include "petproject.h"

class FormalAssetTests : public QObject
{
    Q_OBJECT
private slots:
    void releasePetsHaveReadableRoleFrames()
    {
        QDir root(QCoreApplication::applicationDirPath());
        root.cdUp();
        root.cdUp();
        const QStringList projects {QStringLiteral("pet_animated/pet.json"), QStringLiteral("pet_animated_1/pet.json")};
        QStringList available;
        for (const QString &relative : projects) {
            if (QFileInfo::exists(root.filePath(relative))) available.append(relative);
        }
        if (available.isEmpty()) {
            QSKIP("Optional release pet projects are not included in the public source package.", 0);
        }
        for (const QString &relative : available) {
            PetProject project;
            QString error;
            QVERIFY2(project.load(root.filePath(relative), &error), qPrintable(error));
            for (auto it = project.actions.cbegin(); it != project.actions.cend(); ++it) {
                QVERIFY2(!it->frames.isEmpty(), qPrintable(it.key()));
                for (const PetFrame &frame : it->frames) {
                    QImage image(project.absolutePathFor(frame.path));
                    QVERIFY2(!image.isNull(), qPrintable(project.absolutePathFor(frame.path)));
                }
            }
        }
    }
};

QTEST_MAIN(FormalAssetTests)
#include "formal_asset_tests.moc"

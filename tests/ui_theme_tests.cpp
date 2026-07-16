#include <QtTest>

#include <QApplication>
#include <QPushButton>
#include <QWidget>

#include "ui/theme/apptheme.h"
#include "ui/theme/iconprovider.h"
#include "ui/theme/themeconstants.h"

class UiThemeTests : public QObject
{
    Q_OBJECT

private slots:
    void themeDefinesCoreTokensAndStyles()
    {
        QVERIFY(QString::fromLatin1(ThemeConstants::AppBackground).startsWith(QLatin1Char('#')));
        QVERIFY(QString::fromLatin1(ThemeConstants::Accent).startsWith(QLatin1Char('#')));
        const QString style = AppTheme::styleSheet();
        QVERIFY(style.contains(QStringLiteral("buttonRole=\"primary\"")));
        QVERIFY(style.contains(QStringLiteral("role=\"navigation\"")));
        QVERIFY(!style.contains(QStringLiteral("@ACCENT@")));
    }

    void themeRolesAndIconsApplyToRealWidgets()
    {
        QWidget card;
        QPushButton button;
        AppTheme::setRole(&card, QStringLiteral("card"));
        AppTheme::setButtonRole(&button, QStringLiteral("primary"));
        IconProvider::apply(&button, AppIcon::Play);
        QCOMPARE(card.property("role").toString(), QStringLiteral("card"));
        QCOMPARE(button.property("buttonRole").toString(), QStringLiteral("primary"));
        QVERIFY(!button.icon().isNull());
    }

    void applicationThemeCanBeApplied()
    {
        AppTheme::apply(*qApp);
        QVERIFY(qApp->styleSheet().contains(QString::fromLatin1(ThemeConstants::Accent)));
        QCOMPARE(qApp->font().styleHint(), QFont::SansSerif);
    }
};

QTEST_MAIN(UiThemeTests)
#include "ui_theme_tests.moc"

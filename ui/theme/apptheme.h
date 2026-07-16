#ifndef APPTHEME_H
#define APPTHEME_H

#include <QString>

class QApplication;
class QWidget;
class QAbstractButton;

class AppTheme
{
public:
    static void apply(QApplication &app);
    static QString styleSheet();
    static void setRole(QWidget *widget, const QString &role);
    static void setButtonRole(QAbstractButton *button, const QString &role);

private:
    static void refreshStyle(QWidget *widget);
};

#endif // APPTHEME_H

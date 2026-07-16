#ifndef ICONPROVIDER_H
#define ICONPROVIDER_H

#include <QIcon>

class QAbstractButton;

enum class AppIcon
{
    Home,
    Pet,
    Motion,
    Chat,
    Role,
    Cloud,
    Runtime,
    Tools,
    Settings,
    Play,
    Stop,
    Add,
    Delete,
    Edit,
    Refresh,
    Import,
    Copy,
    Locate,
    Save,
    Test
};

class IconProvider
{
public:
    static QIcon icon(AppIcon name, int size = 18);
    static void apply(QAbstractButton *button, AppIcon name, int size = 18);
};

#endif // ICONPROVIDER_H

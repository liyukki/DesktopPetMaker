#include "apptheme.h"

#include "themeconstants.h"

#include <QAbstractButton>
#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyle>
#include <QWidget>

namespace {
QColor themeColor(const char *value)
{
    return QColor(QString::fromLatin1(value));
}

QString expandedStyleSheet()
{
    QString style = QStringLiteral(R"(
* {
    font-family: "Segoe UI", "Microsoft YaHei UI", sans-serif;
    font-size: 10pt;
    color: @PRIMARY_TEXT@;
    selection-background-color: @ACCENT@;
    selection-color: @SURFACE@;
}
QMainWindow, QDialog, QWidget#appRoot {
    background: @APP_BACKGROUND@;
}
QWidget[role="appHeader"] {
    background: @SURFACE@;
    border-bottom: 1px solid @BORDER@;
}
QWidget[role="sidebar"] {
    background: @SECONDARY_BACKGROUND@;
    border-right: 1px solid @BORDER@;
}
QWidget[role="card"], QFrame[role="card"], QGroupBox {
    background: @SURFACE@;
    border: 1px solid @BORDER@;
    border-radius: 8px;
}
QWidget[role="subtleCard"], QFrame[role="subtleCard"] {
    background: @ELEVATED_SURFACE@;
    border: 1px solid @DIVIDER@;
    border-radius: 6px;
}
QLabel[role="brandTitle"] {
    font-size: 15pt;
    font-weight: 700;
}
QLabel[role="pageTitle"] {
    font-size: 18pt;
    font-weight: 700;
}
QLabel[role="sectionTitle"] {
    font-size: 12pt;
    font-weight: 650;
}
QLabel[role="muted"], QLabel[role="caption"] {
    color: @SECONDARY_TEXT@;
}
QLabel[role="metric"] {
    font-size: 21pt;
    font-weight: 700;
    color: @ACCENT_PRESSED@;
}
QLabel[role="status"] {
    border-radius: 6px;
    padding: 3px 8px;
    font-size: 9pt;
    font-weight: 600;
}
QLabel[status="success"] {
    color: @SUCCESS@;
    background: @SUCCESS_SOFT@;
}
QLabel[status="warning"] {
    color: @WARNING@;
    background: @WARNING_SOFT@;
}
QLabel[status="error"] {
    color: @ERROR@;
    background: @ERROR_SOFT@;
}
QLabel[status="info"] {
    color: @INFO@;
    background: @INFO_SOFT@;
}
QLabel[status="neutral"] {
    color: @SECONDARY_TEXT@;
    background: @SECONDARY_BACKGROUND@;
}
QPushButton, QToolButton {
    min-height: 34px;
    padding: 0 13px;
    background: @SURFACE@;
    border: 1px solid @BORDER@;
    border-radius: 6px;
    font-weight: 600;
}
QPushButton:hover, QToolButton:hover {
    background: @HOVER@;
    border-color: @FOCUS_RING@;
}
QPushButton:pressed, QToolButton:pressed {
    background: @SELECTED@;
}
QPushButton:focus, QToolButton:focus, QLineEdit:focus, QTextEdit:focus,
QPlainTextEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
    border: 2px solid @FOCUS_RING@;
}
QPushButton[buttonRole="primary"] {
    color: @SURFACE@;
    background: @ACCENT@;
    border-color: @ACCENT@;
}
QPushButton[buttonRole="primary"]:hover {
    background: @ACCENT_HOVER@;
    border-color: @ACCENT_HOVER@;
}
QPushButton[buttonRole="primary"]:pressed {
    background: @ACCENT_PRESSED@;
    border-color: @ACCENT_PRESSED@;
}
QPushButton[buttonRole="danger"] {
    color: @ERROR@;
    background: @ERROR_SOFT@;
    border-color: @ERROR_SOFT@;
}
QPushButton[buttonRole="subtle"] {
    background: transparent;
    border-color: transparent;
}
QPushButton[buttonRole="coral"] {
    color: @SURFACE@;
    background: @CORAL@;
    border-color: @CORAL@;
}
QPushButton:disabled, QToolButton:disabled {
    color: @DISABLED_TEXT@;
    background: @SECONDARY_BACKGROUND@;
    border-color: @DIVIDER@;
}
QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    min-height: 32px;
    background: @SURFACE@;
    border: 1px solid @BORDER@;
    border-radius: 6px;
    padding: 3px 8px;
}
QTextEdit, QPlainTextEdit {
    padding: 8px;
}
QComboBox::drop-down {
    width: 28px;
    border: 0;
}
QListWidget, QTreeWidget, QTableWidget {
    background: @SURFACE@;
    border: 1px solid @BORDER@;
    border-radius: 6px;
    outline: 0;
    padding: 4px;
}
QTableView {
    gridline-color: @DIVIDER@;
    alternate-background-color: @ELEVATED_SURFACE@;
}
QHeaderView::section {
    background: @SECONDARY_BACKGROUND@;
    border: 0;
    border-right: 1px solid @DIVIDER@;
    border-bottom: 1px solid @BORDER@;
    padding: 7px;
    font-weight: 600;
}
QListWidget::item, QTreeWidget::item {
    min-height: 34px;
    border-radius: 5px;
    padding: 4px 8px;
}
QListWidget::item:hover, QTreeWidget::item:hover {
    background: @HOVER@;
}
QListWidget::item:selected, QTreeWidget::item:selected {
    color: @ACCENT_PRESSED@;
    background: @SELECTED@;
}
QListWidget[role="navigation"] {
    background: transparent;
    border: 0;
    padding: 7px;
}
QListWidget[role="navigation"]::item {
    min-height: 42px;
    margin: 2px 0;
    padding-left: 12px;
}
QListWidget[role="navigation"]::item:selected {
    border-left: 3px solid @ACCENT@;
    font-weight: 650;
}
QScrollArea {
    background: transparent;
    border: 0;
}
QScrollArea > QWidget > QWidget {
    background: transparent;
}
QSplitter::handle {
    background: @DIVIDER@;
    width: 1px;
    height: 1px;
}
QGroupBox {
    margin-top: 12px;
    padding: 14px 12px 12px 12px;
    font-weight: 650;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 5px;
}
QCheckBox {
    spacing: 8px;
    min-height: 28px;
}
QRadioButton {
    spacing: 8px;
    min-height: 28px;
}
QRadioButton::indicator {
    width: 17px;
    height: 17px;
    border-radius: 9px;
}
QRadioButton::indicator:unchecked {
    background: @SURFACE@;
    border: 1px solid @BORDER@;
}
QRadioButton::indicator:checked {
    background: @ACCENT@;
    border: 4px solid @ACCENT_SOFT@;
}
QTabWidget::pane {
    background: @SURFACE@;
    border: 1px solid @BORDER@;
    border-radius: 6px;
}
QTabBar::tab {
    min-height: 32px;
    padding: 0 14px;
    color: @SECONDARY_TEXT@;
    background: @SECONDARY_BACKGROUND@;
    border: 1px solid @BORDER@;
    border-bottom: 0;
}
QTabBar::tab:selected {
    color: @ACCENT_PRESSED@;
    background: @SURFACE@;
    font-weight: 600;
}
QSlider::groove:horizontal {
    height: 4px;
    background: @DIVIDER@;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    width: 16px;
    margin: -6px 0;
    background: @ACCENT@;
    border-radius: 8px;
}
QProgressBar {
    min-height: 8px;
    color: transparent;
    background: @SECONDARY_BACKGROUND@;
    border: 0;
    border-radius: 4px;
}
QProgressBar::chunk {
    background: @ACCENT@;
    border-radius: 4px;
}
QScrollBar:vertical {
    width: 11px;
    background: transparent;
    margin: 2px;
}
QScrollBar::handle:vertical {
    min-height: 28px;
    background: @BORDER@;
    border-radius: 5px;
}
QScrollBar::handle:vertical:hover {
    background: @FOCUS_RING@;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    height: 0;
    background: transparent;
}
QCheckBox::indicator {
    width: 17px;
    height: 17px;
}
QCheckBox::indicator:unchecked {
    background: @SURFACE@;
    border: 1px solid @BORDER@;
    border-radius: 4px;
}
QCheckBox::indicator:checked {
    background: @ACCENT@;
    border: 1px solid @ACCENT@;
    border-radius: 4px;
}
QMenu {
    background: @SURFACE@;
    border: 1px solid @BORDER@;
    padding: 6px;
}
QMenu::item {
    min-height: 28px;
    padding: 5px 28px 5px 24px;
    border-radius: 5px;
}
QMenu::item:selected {
    background: @SELECTED@;
}
QToolTip {
    color: @PRIMARY_TEXT@;
    background: @SURFACE@;
    border: 1px solid @BORDER@;
    padding: 5px;
}
QStatusBar {
    background: @SURFACE@;
    border-top: 1px solid @DIVIDER@;
}
)");

    const QList<QPair<QString, QString>> tokens {
        {QStringLiteral("@APP_BACKGROUND@"), QString::fromLatin1(ThemeConstants::AppBackground)},
        {QStringLiteral("@SECONDARY_BACKGROUND@"), QString::fromLatin1(ThemeConstants::SecondaryBackground)},
        {QStringLiteral("@SURFACE@"), QString::fromLatin1(ThemeConstants::Surface)},
        {QStringLiteral("@ELEVATED_SURFACE@"), QString::fromLatin1(ThemeConstants::ElevatedSurface)},
        {QStringLiteral("@BORDER@"), QString::fromLatin1(ThemeConstants::Border)},
        {QStringLiteral("@DIVIDER@"), QString::fromLatin1(ThemeConstants::Divider)},
        {QStringLiteral("@PRIMARY_TEXT@"), QString::fromLatin1(ThemeConstants::PrimaryText)},
        {QStringLiteral("@SECONDARY_TEXT@"), QString::fromLatin1(ThemeConstants::SecondaryText)},
        {QStringLiteral("@DISABLED_TEXT@"), QString::fromLatin1(ThemeConstants::DisabledText)},
        {QStringLiteral("@ACCENT@"), QString::fromLatin1(ThemeConstants::Accent)},
        {QStringLiteral("@ACCENT_HOVER@"), QString::fromLatin1(ThemeConstants::AccentHover)},
        {QStringLiteral("@ACCENT_PRESSED@"), QString::fromLatin1(ThemeConstants::AccentPressed)},
        {QStringLiteral("@SELECTED@"), QString::fromLatin1(ThemeConstants::Selected)},
        {QStringLiteral("@HOVER@"), QString::fromLatin1(ThemeConstants::Hover)},
        {QStringLiteral("@FOCUS_RING@"), QString::fromLatin1(ThemeConstants::FocusRing)},
        {QStringLiteral("@CORAL@"), QString::fromLatin1(ThemeConstants::Coral)},
        {QStringLiteral("@SUCCESS@"), QString::fromLatin1(ThemeConstants::Success)},
        {QStringLiteral("@SUCCESS_SOFT@"), QString::fromLatin1(ThemeConstants::SuccessSoft)},
        {QStringLiteral("@WARNING@"), QString::fromLatin1(ThemeConstants::Warning)},
        {QStringLiteral("@WARNING_SOFT@"), QString::fromLatin1(ThemeConstants::WarningSoft)},
        {QStringLiteral("@ERROR@"), QString::fromLatin1(ThemeConstants::Error)},
        {QStringLiteral("@ERROR_SOFT@"), QString::fromLatin1(ThemeConstants::ErrorSoft)},
        {QStringLiteral("@INFO@"), QString::fromLatin1(ThemeConstants::Info)},
        {QStringLiteral("@INFO_SOFT@"), QString::fromLatin1(ThemeConstants::InfoSoft)}
    };
    for (const auto &token : tokens) {
        style.replace(token.first, token.second);
    }
    return style;
}
}

void AppTheme::apply(QApplication &app)
{
    QFont font(QStringLiteral("Segoe UI"));
    font.setStyleHint(QFont::SansSerif);
    font.setPointSize(10);
    app.setFont(font);
    app.setStyle(QStringLiteral("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, themeColor(ThemeConstants::AppBackground));
    palette.setColor(QPalette::WindowText, themeColor(ThemeConstants::PrimaryText));
    palette.setColor(QPalette::Base, themeColor(ThemeConstants::Surface));
    palette.setColor(QPalette::AlternateBase, themeColor(ThemeConstants::SecondaryBackground));
    palette.setColor(QPalette::Text, themeColor(ThemeConstants::PrimaryText));
    palette.setColor(QPalette::Button, themeColor(ThemeConstants::Surface));
    palette.setColor(QPalette::ButtonText, themeColor(ThemeConstants::PrimaryText));
    palette.setColor(QPalette::Highlight, themeColor(ThemeConstants::Accent));
    palette.setColor(QPalette::HighlightedText, themeColor(ThemeConstants::Surface));
    palette.setColor(QPalette::Disabled, QPalette::Text, themeColor(ThemeConstants::DisabledText));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, themeColor(ThemeConstants::DisabledText));
    app.setPalette(palette);
    app.setStyleSheet(styleSheet());
}

QString AppTheme::styleSheet()
{
    static const QString style = expandedStyleSheet();
    return style;
}

void AppTheme::setRole(QWidget *widget, const QString &role)
{
    if (!widget) return;
    widget->setProperty("role", role);
    refreshStyle(widget);
}

void AppTheme::setButtonRole(QAbstractButton *button, const QString &role)
{
    if (!button) return;
    button->setProperty("buttonRole", role);
    refreshStyle(button);
}

void AppTheme::refreshStyle(QWidget *widget)
{
    if (!widget || !widget->style()) return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

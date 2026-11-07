#include "window.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTimer>
#include <QStyleFactory>
#include <QPalette>
#include <QIcon>
#include <QSettings>
#ifdef __GLIBC__
#include <malloc.h>
#endif

int main(int argc, char **argv)
{
#ifdef __GLIBC__
    // glibc raises its mmap threshold after large frees, so freed page images stayed in the
    // heap (≈20 MiB after zooming). A fixed threshold returns them to the system.
    mallopt(M_MMAP_THRESHOLD, 128 * 1024);
#endif
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    app.setApplicationName("Folitex");
    app.setApplicationVersion("0.3.0");
    app.setOrganizationName("Folitex");
    // Wayland app_id must match folitex.desktop so launchers and docks show the right icon.
    app.setDesktopFileName("folitex");
    app.setWindowIcon(QIcon(":/folitex.svg"));
    app.setStyle(QStyleFactory::create("Fusion"));
    // Colors follow the author's VS Code setup: Dark Modern with muted workbench.colorCustomizations.
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#0E1013")); // gap between rounded panels
    palette.setColor(QPalette::WindowText, QColor("#ADB7C4"));
    palette.setColor(QPalette::Base, QColor("#16181B"));
    palette.setColor(QPalette::AlternateBase, QColor("#1D2024"));
    palette.setColor(QPalette::Text, QColor("#D2D7DE"));
    palette.setColor(QPalette::Button, QColor("#1C2025"));
    palette.setColor(QPalette::ButtonText, QColor("#D2D7DE"));
    palette.setColor(QPalette::Highlight, QColor("#35434F"));
    palette.setColor(QPalette::HighlightedText, QColor("#E0E5EC"));
    palette.setColor(QPalette::PlaceholderText, QColor("#68727F"));
    palette.setColor(QPalette::ToolTipBase, QColor("#1B1F24"));
    palette.setColor(QPalette::ToolTipText, QColor("#D2D7DE"));
    app.setPalette(palette);
    QFont ui = app.font(); ui.setPointSizeF(10); app.setFont(ui);
    // Spin-box arrows are hidden with width:0, not NoButtons: styled NoButtons spin boxes collapse the text field (Qt 6.12).
    app.setStyleSheet(R"(
        QToolButton, QPushButton { border:0; border-radius:4px; padding:4px 8px; background:transparent; }
        QPushButton:hover, QToolButton:hover { background:#20252B; }
        QPushButton#primary { background:#4F6F6B; color:#E0E5EC; padding:4px 12px; }
        QPushButton#primary:hover { background:#5C807B; }
        QComboBox, QSpinBox, QLineEdit { background:#1C2025; border:1px solid #303842; border-radius:4px; padding:3px 6px; }
        QSpinBox QLineEdit { background:transparent; border:0; padding:0; }
        QComboBox:focus, QSpinBox:focus, QLineEdit:focus { border-color:#6F9692; }
        QComboBox QAbstractItemView { background:#1B1F24; border:1px solid #303842; selection-background-color:#29333B; }
        QToolButton::menu-indicator { image:none; width:0; }
        QTabWidget::pane { border:0; }
        QTabBar { background:#16181B; }
        QTabBar::tab { background:#16181B; color:#8793A2; height:35px; padding:0 8px 0 4px; border:0; border-right:1px solid #22272E; border-bottom:1px solid transparent; }
        QTabBar::tab:selected { background:#1D2024; color:#D2D7DE; border-bottom-color:#7DA6A0; }
        QTabBar::tab:hover:!selected { color:#ADB7C4; }
        QSplitter::handle { background:#0E1013; }
        QStatusBar { background:#0E1013; color:#ADB7C4; border:0; min-height:22px; max-height:22px; }
        QStatusBar QLabel { font-size:12px; }
        QWidget#pdfBar { background:#16181B; border-bottom:1px solid #22272E; }
        QWidget#findWidget { background:#1B1F24; border:1px solid #303842; border-top:0; border-radius:0 0 4px 4px; }
        QWidget#findWidget QToolButton { padding:2px 5px; }
        QWidget#findWidget QToolButton:checked, QWidget#findWidget QToolButton:checked:hover { background:#29333B; border:1px solid #6F9692; }
        QWidget#findWidget QLabel { color:#ADB7C4; padding:0 4px; }
        QWidget#findWidget QToolButton#replaceToggle:checked { background:transparent; border:0; }
        QWidget#pdfFindBar { background:#16181B; border-bottom:1px solid #22272E; }
        QWidget#pdfBar QToolButton { min-width:22px; padding:3px 6px; }
        QTabWidget#editorTabs { background:#16181B; }
        QTabBar::scroller { width:0; }
        QTabBar::tear { image:none; width:0; border:0; }
        QTabBar QToolButton#ScrollLeftButton, QTabBar QToolButton#ScrollRightButton { width:0; border:0; padding:0; }
        QWidget#buildCorner QComboBox { background:transparent; border:0; padding:2px 4px; }
        QWidget#buildCorner QComboBox:hover { background:#20252B; }
        QPushButton#editorAction { background:transparent; border:0; border-radius:4px; padding:3px; }
        QPushButton#editorAction:hover { background:#20252B; }
        QWidget#activity { background:#0E1013; }
        QToolButton#activityButton { border:0; border-radius:0; border-left:2px solid transparent; padding:0; }
        QToolButton#activityButton:hover { background:transparent; }
        QToolButton#activityButton:checked { border-left-color:#7DA6A0; background:transparent; }
        QWidget#side { background:#121417; }
        QLabel#sideTitle { font-size:13px; font-weight:bold; color:#9AA6B5; padding-left:16px; min-height:35px; max-height:35px; }
        QStatusBar::item { border:0; }
        QStatusBar QLabel { padding:0 6px; }
        QWidget#pdfBar QSpinBox { background:transparent; border:0; padding:1px 2px; }
        QWidget#pdfBar QSpinBox::up-button, QWidget#pdfBar QSpinBox::down-button { width:0; border:0; }
        QWidget#pdfBar QSpinBox:hover { background:#20252B; }
        QTreeView { background:#121417; border:0; outline:0; }
        QTreeView::item { height:22px; }
        QTreeView::branch:has-children:closed { image:url(:/chevron-right.svg); }
        QTreeView::branch:has-children:open { image:url(:/chevron-down.svg); }
        QToolButton#tabClose { border:0; border-radius:4px; padding:0; margin-right:2px; min-width:20px; max-width:20px; min-height:20px; max-height:20px; }
        QToolButton#tabClose:hover { background:#29333B; }
        QLabel#tabHint { color:#68727F; padding:0 4px 0 0; margin:0; }
        QTreeView::item:hover { background:#20252B; }
        QTreeView::item:selected { background:#29333B; color:#E0E5EC; }
        QPlainTextEdit { background:#16181B; border:0; padding:4px 6px; }
        QScrollBar:vertical { background:transparent; width:8px; margin:0; }
        QScrollBar::handle:vertical { background:rgba(120, 135, 153, 0.2); min-height:28px; }
        QScrollBar::handle:vertical:hover { background:rgba(120, 135, 153, 0.4); }
        QScrollBar:horizontal { background:transparent; height:8px; margin:0; }
        QScrollBar::handle:horizontal { background:rgba(120, 135, 153, 0.2); min-width:28px; }
        QScrollBar::handle:horizontal:hover { background:rgba(120, 135, 153, 0.4); }
        QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }
        QScrollBar::add-page, QScrollBar::sub-page { background:transparent; }
        QMenuBar { background:transparent; spacing:0; }
        QMenuBar::item { background:transparent; padding:4px 8px; border-radius:4px; color:#ADB7C4; }
        QMenuBar::item:selected, QMenuBar::item:pressed { background:#20252B; }
        QMenu { background:#1B1F24; color:#D2D7DE; border:1px solid #303842; border-radius:4px; padding:4px; }
        QMenu::item { padding:5px 24px; border-radius:3px; }
        QMenu::item:selected { background:#29333B; color:#E0E5EC; }
        QMenu::separator { height:1px; background:#303842; margin:4px 0; }
        QToolTip { background:#1B1F24; color:#D2D7DE; border:1px solid #303842; }
        QToolButton#rootButton { font-weight:bold; color:#ADB7C4; text-align:left; padding:3px 4px; border-radius:0; }
        QToolButton#rootButton:hover { background:transparent; }
)");
    // One-time move of settings saved under the former name.
    if (QSettings settings; settings.allKeys().isEmpty())
    {
        QSettings former("TeXLite", "TeXLite");
        for (const auto &key : former.allKeys()) settings.setValue(key, former.value(key));
    }
    QCommandLineParser parser;
    parser.setApplicationDescription("Native minimal LaTeX editor and tiled PDF viewer");
    parser.addHelpOption(); parser.addVersionOption();
    parser.addPositionalArgument("files", "TeX or PDF documents", "[files…]");
    QCommandLineOption quitAfter("quit-after", "Exit after N milliseconds (offscreen testing).", "milliseconds");
    QCommandLineOption screenshot("screenshot", "Save a window screenshot after rendering.", "path");
    QCommandLineOption pdfOnly("pdf-only", "Start with the source editor hidden.");
    parser.addOption(quitAfter); parser.addOption(screenshot); parser.addOption(pdfOnly);
    parser.process(app);
    Window window;
    window.openWorkspace(QSettings().value("workspace").toString());
    window.setWordWrap(QSettings().value("wordWrap", false).toBool());
    // Saved here rather than in Window so tests never touch the user's settings.
    QObject::connect(&app, &QApplication::aboutToQuit, [&window] { QSettings().setValue("wordWrap", window.wordWrap()); });
    for (const auto &file : parser.positionalArguments())
    {
        if (file.endsWith(".pdf", Qt::CaseInsensitive)) window.openPdf(QFileInfo(file).absoluteFilePath());
        else window.openSource(QFileInfo(file).absoluteFilePath());
    }
    if (parser.isSet(pdfOnly))
        for (auto *tabs : window.findChildren<QTabWidget *>()) tabs->hide();
    window.show();
    if (parser.isSet(screenshot))
        QTimer::singleShot(2500, &window, [&window, path = parser.value(screenshot)] { window.grab().save(path); });
    if (parser.isSet(quitAfter))
    {
        bool ok = false; int ms = parser.value(quitAfter).toInt(&ok);
        if (ok && ms > 0) QTimer::singleShot(ms, &app, &QApplication::quit);
    }
    return app.exec();
}

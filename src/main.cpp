#include "window.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTimer>
#include <QStyleFactory>
#include <QPalette>
#include <QIcon>
#include <QSettings>

int main(int argc, char **argv)
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    app.setApplicationName("Folitex");
    app.setApplicationVersion("0.2.0");
    app.setOrganizationName("Folitex");
    // Wayland app_id must match folitex.desktop so launchers and docks show the right icon.
    app.setDesktopFileName("folitex");
    app.setWindowIcon(QIcon(":/folitex.svg"));
    app.setStyle(QStyleFactory::create("Fusion"));
    // Colors follow VS Code "Dark Modern".
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#181818"));
    palette.setColor(QPalette::WindowText, QColor("#CCCCCC"));
    palette.setColor(QPalette::Base, QColor("#1F1F1F"));
    palette.setColor(QPalette::AlternateBase, QColor("#2B2B2B"));
    palette.setColor(QPalette::Text, QColor("#CCCCCC"));
    palette.setColor(QPalette::Button, QColor("#313131"));
    palette.setColor(QPalette::ButtonText, QColor("#CCCCCC"));
    palette.setColor(QPalette::Highlight, QColor("#264F78"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor("#989898"));
    palette.setColor(QPalette::ToolTipBase, QColor("#202020"));
    palette.setColor(QPalette::ToolTipText, QColor("#CCCCCC"));
    app.setPalette(palette);
    QFont ui = app.font(); ui.setPointSizeF(10); app.setFont(ui);
    // Spin-box arrows are hidden with width:0, not NoButtons: styled NoButtons spin boxes collapse the text field (Qt 6.12).
    app.setStyleSheet(R"(
        QToolButton, QPushButton { border:0; border-radius:4px; padding:4px 8px; background:transparent; }
        QPushButton:hover, QToolButton:hover { background:#2B2B2B; }
        QPushButton#primary { background:#0078D4; color:white; padding:4px 12px; }
        QPushButton#primary:hover { background:#026EC1; }
        QComboBox, QSpinBox, QLineEdit { background:#313131; border:1px solid #3C3C3C; border-radius:4px; padding:3px 6px; }
        QSpinBox QLineEdit { background:transparent; border:0; padding:0; }
        QComboBox:focus, QSpinBox:focus, QLineEdit:focus { border-color:#0078D4; }
        QComboBox QAbstractItemView { background:#1F1F1F; border:1px solid #3C3C3C; selection-background-color:#0078D4; }
        QToolButton::menu-indicator { image:none; width:0; }
        QTabWidget::pane { border:0; }
        QTabBar::close-button { image:url(:/close.svg); subcontrol-position:right; border-radius:3px; }
        QTabBar::close-button:hover { background:#3C3C3C; }
        QTabBar { background:#181818; }
        QTabBar::tab { background:#181818; color:#9D9D9D; height:35px; padding:0 12px; border-right:1px solid #2B2B2B;
                       border-top:1px solid transparent; border-bottom:1px solid #2B2B2B; }
        QTabBar::tab:selected { background:#1F1F1F; color:#FFFFFF; border-top-color:#0078D4; border-bottom-color:#1F1F1F; }
        QTabBar::tab:hover:!selected { background:#1F1F1F; }
        QSplitter::handle { background:#2B2B2B; width:1px; height:1px; }
        QStatusBar { background:#181818; color:#CCCCCC; border-top:1px solid #2B2B2B; min-height:22px; max-height:22px; }
        QStatusBar QLabel { font-size:12px; }
        QWidget#pdfBar { background:#181818; border-bottom:1px solid #2B2B2B; }
        QWidget#pdfBar QToolButton { min-width:22px; padding:3px 6px; }
        QWidget#buildCorner QComboBox { background:transparent; border:0; padding:2px 4px; }
        QWidget#buildCorner QComboBox:hover { background:#2B2B2B; }
        QPushButton#editorAction { background:transparent; border:0; border-radius:4px; padding:3px; }
        QPushButton#editorAction:hover { background:#2B2B2B; }
        QWidget#activity { background:#181818; border-right:1px solid #2B2B2B; }
        QToolButton#activityButton { border:0; border-radius:0; border-left:2px solid transparent; padding:0; }
        QToolButton#activityButton:hover { background:transparent; }
        QToolButton#activityButton:checked { border-left-color:#0078D4; background:transparent; }
        QWidget#side { background:#181818; border-right:1px solid #2B2B2B; }
        QLabel#sideTitle { font-size:11px; color:#CCCCCC; padding-left:20px; min-height:35px; max-height:35px; }
        QStatusBar::item { border:0; }
        QStatusBar QLabel { padding:0 6px; }
        QWidget#pdfBar QSpinBox { background:transparent; border:0; padding:1px 2px; }
        QWidget#pdfBar QSpinBox::up-button, QWidget#pdfBar QSpinBox::down-button { width:0; border:0; }
        QWidget#pdfBar QSpinBox:hover { background:#2B2B2B; }
        QTreeView { background:#181818; border:0; outline:0; }
        QTreeView::item { height:22px; }
        QTreeView::item:hover { background:#2A2D2E; }
        QTreeView::item:selected { background:#04395E; }
        QPlainTextEdit { background:#181818; border:0; border-top:1px solid #2B2B2B; }
        QScrollBar:vertical { background:transparent; width:10px; margin:0; }
        QScrollBar::handle:vertical { background:#79797966; min-height:28px; }
        QScrollBar::handle:vertical:hover { background:#646464B3; }
        QScrollBar:horizontal { background:transparent; height:10px; margin:0; }
        QScrollBar::handle:horizontal { background:#79797966; min-width:28px; }
        QScrollBar::handle:horizontal:hover { background:#646464B3; }
        QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }
        QScrollBar::add-page, QScrollBar::sub-page { background:transparent; }
        QMenuBar { background:transparent; spacing:0; }
        QMenuBar::item { background:transparent; padding:4px 8px; border-radius:4px; color:#CCCCCC; }
        QMenuBar::item:selected, QMenuBar::item:pressed { background:#2B2B2B; }
        QMenu { background:#1F1F1F; border:1px solid #454545; border-radius:4px; padding:4px; }
        QMenu::item { padding:5px 24px; border-radius:3px; }
        QMenu::item:selected { background:#0078D4; color:white; }
        QMenu::separator { height:1px; background:#454545; margin:4px 0; }
        QToolTip { background:#202020; color:#CCCCCC; border:1px solid #454545; }
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

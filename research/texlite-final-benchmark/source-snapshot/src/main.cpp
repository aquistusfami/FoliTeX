#include "window.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTimer>
#include <QStyleFactory>
#include <QPalette>

int main(int argc, char **argv)
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    app.setApplicationName("TeXLite");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("TeXLite");
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#16181B"));
    palette.setColor(QPalette::WindowText, QColor("#D2D7DE"));
    palette.setColor(QPalette::Base, QColor("#16181B"));
    palette.setColor(QPalette::AlternateBase, QColor("#20252B"));
    palette.setColor(QPalette::Text, QColor("#D2D7DE"));
    palette.setColor(QPalette::Button, QColor("#20252B"));
    palette.setColor(QPalette::ButtonText, QColor("#D2D7DE"));
    palette.setColor(QPalette::Highlight, QColor("#35434F"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor("#8793A2"));
    app.setPalette(palette);
    QFont ui = app.font(); ui.setPointSizeF(10); app.setFont(ui);
    app.setStyleSheet(R"(
        QToolButton, QPushButton, QComboBox, QSpinBox, QLineEdit { border:1px solid #303842; border-radius:5px; padding:5px 8px; }
        QPushButton:hover, QToolButton:hover { background:#29333B; }
        QComboBox { min-width:85px; }
        QTabWidget::pane { border:0; }
        QTabBar::tab { background:#16181B; padding:9px 13px; border-bottom:2px solid transparent; color:#8793A2; }
        QTabBar::tab:selected { color:#D2D7DE; border-bottom-color:#7DA6A0; }
        QSplitter::handle { background:#2A3037; width:1px; height:1px; }
        QScrollBar:vertical { background:transparent; width:7px; margin:0; }
        QScrollBar::handle:vertical { background:#46515F; border-radius:3px; min-height:28px; }
        QScrollBar:horizontal { background:transparent; height:7px; margin:0; }
        QScrollBar::handle:horizontal { background:#46515F; border-radius:3px; min-width:28px; }
        QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }
        QScrollBar::add-page, QScrollBar::sub-page { background:transparent; }
        QMenu { border:1px solid #303842; padding:5px; }
        QMenu::item { padding:7px 24px; }
        QMenu::item:selected { background:#29333B; }
    )");
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

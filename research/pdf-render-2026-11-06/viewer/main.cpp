#include "pdfview.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QScrollBar>
#include <QFile>
#include <cstdio>
static qint64 rss()
{
    QFile f("/proc/self/status"); f.open(QIODevice::ReadOnly);
    for (auto line : f.readAll().split('\n')) if (line.startsWith("VmRSS:")) return line.mid(6).trimmed().split(' ')[0].toLongLong() / 1024;
    return 0;
}
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    PdfView view; view.resize(900, 1000); view.show();
    auto wait = [&](const char *label)
    {
        QElapsedTimer t; t.start(); quint64 r0 = view.renderRequests();
        do { app.processEvents(QEventLoop::AllEvents, 5); view.viewport()->repaint(); } while (!(view.renderingIdle() && view.cachedTiles() > 0 && t.elapsed() > 200) && t.elapsed() < 60000);
        printf("%-28s %6lld ms  renders=%3llu  cache=%6d KiB  rss=%lld MiB\n", label, t.elapsed() - 200, view.renderRequests() - r0, view.cacheKiB(), rss());
    };
    view.open(argv[1]); wait("open page 1");
    view.goToPage(5); wait("jump to page 5 (images)");
    for (int i = 0; i < 6; ++i) { view.setZoom(1 + 0.2 * (i + 1)); app.processEvents(); view.viewport()->repaint(); QElapsedTimer s; s.start(); while (s.elapsed() < 40) app.processEvents(); }
    wait("6 zoom steps -> 220%");
    view.verticalScrollBar()->setValue(view.verticalScrollBar()->value() + 1500); wait("scroll 1500 px at 220%");
    view.fitWidth(); view.goToPage(12); wait("fit width, page 12");
}

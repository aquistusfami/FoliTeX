#include "window.h"
#include <QApplication>
#include <QFile>
#include <QScrollBar>
#include <QTest>
#include <malloc.h>
#include <QTextCursor>

static QString mem()
{
    QFile f("/proc/self/smaps_rollup"); f.open(QIODevice::ReadOnly);
    QString out; long rss = 0, pss = 0, anon = 0;
    for (auto line : f.readAll().split('\n'))
    {
        auto parts = line.simplified().split(' ');
        if (parts.size() < 2) continue;
        if (parts[0] == "Rss:") rss = parts[1].toLong();
        if (parts[0] == "Pss:") pss = parts[1].toLong();
        if (parts[0] == "Anonymous:") anon = parts[1].toLong();
    }
    struct mallinfo2 m = mallinfo2();
    return QString("RSS %1  PSS %2  anon %3  heap-in-use %4  heap-free %5 MiB")
        .arg(rss / 1024.0, 6, 'f', 1).arg(pss / 1024.0, 6, 'f', 1).arg(anon / 1024.0, 6, 'f', 1)
        .arg(m.uordblks / 1048576.0, 6, 'f', 1).arg(m.fordblks / 1048576.0, 6, 'f', 1);
}
static void settle(int ms) { QTest::qWait(ms); }

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("FolitexMem");
    Window window;
    window.show();
    settle(1500);
    printf("%-28s %s\n", "startup (empty tab)", qPrintable(mem()));
    window.openSource(argv[1]);
    settle(1500);
    printf("%-28s %s\n", "+ report.tex", qPrintable(mem()));
    window.openPdf(argv[2]);
    auto *pdf = window.pdfView();
    QTest::qWaitFor([&] { return pdf->pageCount() > 0 && pdf->renderingIdle() && pdf->cachedTiles() > 0; }, 20000);
    settle(500);
    printf("%-28s %s  cache %d KiB\n", "+ PDF first view", qPrintable(mem()), pdf->cacheKiB());
    auto *bar = pdf->verticalScrollBar();
    while (bar->value() < bar->maximum())
    {
        bar->setValue(bar->value() + pdf->viewport()->height() / 2);
        QTest::qWaitFor([&] { return pdf->renderingIdle(); }, 20000);
        QTest::qWait(30);
    }
    settle(500);
    printf("%-28s %s  cache %d KiB\n", "after scrolling all pages", qPrintable(mem()), pdf->cacheKiB());
    for (int i = 0; i < 4; ++i) { pdf->setZoom(pdf->zoom() * 1.25); QTest::qWait(400); QTest::qWaitFor([&] { return pdf->renderingIdle(); }, 20000); }
    settle(500);
    printf("%-28s %s  cache %d KiB\n", "after zoom x2.4", qPrintable(mem()), pdf->cacheKiB());
    pdf->fitWidth(); QTest::qWait(400);
    auto *editor = window.currentEditor();
    editor->moveCursor(QTextCursor::End);
    editor->insertPlainText("\n\\usepackage{");
    QTest::keyClick(editor, Qt::Key_Space, Qt::ControlModifier);
    settle(1000);
    printf("%-28s %s\n", "package completion", qPrintable(mem()));
    if (argc > 3) { window.openSource(argv[3]); settle(4000); printf("%-28s %s\n", "+ 5,003-line file", qPrintable(mem())); }
    malloc_trim(0);
    printf("%-28s %s\n", "after malloc_trim", qPrintable(mem()));
    return 0;
}

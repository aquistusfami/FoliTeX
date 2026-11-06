#include <poppler-qt6.h>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <cstdio>
int main(int, char **argv)
{
    QFile f(argv[1]); f.open(QIODevice::ReadOnly);
    auto doc = Poppler::Document::loadFromData(f.readAll());
    doc->setRenderHint(Poppler::Document::Antialiasing); doc->setRenderHint(Poppler::Document::TextAntialiasing);
    double dpi = atof(argv[2]);
    for (int p = 0; p < doc->numPages(); ++p)
    {
        auto page = doc->page(p);
        QSizeF s = page->pageSizeF(); int W = qCeil(s.width() * dpi / 72), H = qCeil(s.height() * dpi / 72);
        QElapsedTimer t; t.start();
        page->renderToImage(dpi, dpi, 0, 0, W, H); qint64 full = t.elapsed(); t.restart();
        int n = 0; for (int y = 0; y < H; y += 512) for (int x = 0; x < W; x += 512, ++n) page->renderToImage(dpi, dpi, x, y, std::min(512, W - x), std::min(512, H - y));
        qint64 tiles = t.elapsed(); t.restart();
        int m = 0; for (int y = 0; y < H; y += 1024, ++m) page->renderToImage(dpi, dpi, 0, y, W, std::min(1024, H - y));
        qint64 strips = t.elapsed();
        printf("p%-2d %dx%d full=%lldms tiles(%d)=%lldms strips(%d)=%lldms\n", p + 1, W, H, full, n, tiles, m, strips);
    }
}

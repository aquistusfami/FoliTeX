#include "pdfview.h"
#include "editor.h"
#include <QFile>
#include <QPainter>
#include <QScrollBar>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>
#include <cmath>
#include <algorithm>

QString Tile::key() const
{
    return QString::number(generation) + '/' + QString::number(page) + '/' + QString::number(dpi1000)
        + '/' + QString::number(pixels.x()) + '/' + QString::number(pixels.y());
}

void PdfRenderer::open(const QString &path, int generation)
{
    m_page.reset();
    m_document.reset();
    m_generation = generation;
    m_pageNumber = -1;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        emit loaded(generation, {}, "Không mở được PDF: " + file.errorString());
        return;
    }
    // A snapshot lets latexmk replace/rebuild its output without changing an in-flight render.
    // Poppler copies these bytes; do not retain a second whole-document copy.
    m_document = Poppler::Document::loadFromData(file.readAll());
    if (!m_document || m_document->isLocked())
    {
        emit loaded(generation, {}, "PDF chưa hợp lệ hoặc cần mật khẩu.");
        return;
    }
    m_document->setRenderHint(Poppler::Document::Antialiasing, true);
    m_document->setRenderHint(Poppler::Document::TextAntialiasing, true);
    QList<QSizeF> sizes;
    for (int i = 0; i < m_document->numPages(); ++i)
    {
        auto page = m_document->page(i);
        if (!page)
        {
            emit loaded(generation, {}, "Không đọc được một trang PDF.");
            return;
        }
        sizes.append(page->pageSizeF());
    }
    emit loaded(generation, sizes, {});
}

void PdfRenderer::render(Tile tile)
{
    QImage image;
    if (m_document && tile.generation == m_generation)
    {
        if (m_pageNumber != tile.page)
        {
            m_page = m_document->page(tile.page);
            m_pageNumber = tile.page;
        }
        if (m_page)
        {
            double dpi = tile.dpi1000 / 1000.0;
            image = m_page->renderToImage(dpi, dpi, tile.pixels.x(), tile.pixels.y(), tile.pixels.width(), tile.pixels.height());
        }
    }
    emit rendered(tile, image);
}

PdfView::PdfView(QWidget *parent) : QAbstractScrollArea(parent), m_renderer(new PdfRenderer)
{
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    setObjectName("pdfView");
    qRegisterMetaType<Tile>();
    qRegisterMetaType<QList<QSizeF>>();
    m_renderer->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_renderer, &QObject::deleteLater);
    connect(this, &PdfView::loadRequested, m_renderer, &PdfRenderer::open);
    connect(this, &PdfView::tileRequested, m_renderer, &PdfRenderer::render);
    connect(m_renderer, &PdfRenderer::loaded, this, [this](int generation, QList<QSizeF> sizes, const QString &error)
    {
        if (generation != m_generation) return;
        if (!error.isEmpty()) { m_error = error; viewport()->update(); emit loadFailed(error); return; }
        m_sizes = sizes;
        layoutPages();
        emit documentReady(m_sizes.size());
        viewport()->update();
    });
    connect(m_renderer, &PdfRenderer::rendered, this, &PdfView::tileFinished);
    new SmoothScroll(this);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { emit pageChanged(currentPage()); });
    m_thread.start();
}

void PdfView::tileFinished(Tile tile, QImage image)
{
        m_busy = false;
        if (tile.generation == m_generation && tile.dpi1000 == m_dpi1000 && !image.isNull())
        {
            int cost = int((image.sizeInBytes() + 1023) / 1024);
            m_cache.insert(tile.key(), new QImage(std::move(image)), cost);
        }
        else if (tile.generation == m_generation && tile.dpi1000 == m_dpi1000 && image.isNull())
        {
            m_failedPages.insert(tile.page);
            emit loadFailed(QString("Không render được trang %1; mở lại PDF để thử lại.").arg(tile.page + 1));
        }
        viewport()->update();
        // paintEvent refreshes the visible request list before another tile is queued.
}

PdfView::~PdfView()
{
    m_thread.quit();
    m_thread.wait();
}

void PdfView::open(const QString &path)
{
    m_path = path;
    m_error.clear();
    ++m_generation;
    m_sizes.clear();
    m_pages.clear();
    m_pending.clear();
    m_cache.clear();
    m_failedPages.clear();
    emit loadRequested(path, m_generation);
    viewport()->update();
}

QPointF PdfView::offset() const
{
    double dpr = devicePixelRatioF();
    return {qRound(horizontalScrollBar()->value() * dpr) / dpr,
            qRound(verticalScrollBar()->value() * dpr) / dpr};
}

double PdfView::renderDensity(QSize viewport, double nativeDpr)
{
    // Bound the visible tile working set, including edge tiles, at 128 MiB.
    // Normal displays keep native resolution. Only unusually large surfaces need a density limit.
    constexpr double padding = 1024;
    constexpr double maxPixels = (128.0 * 1024 * 1024 - 2 * 1024 * 1024) / 4;
    double w = std::max(1, viewport.width()), h = std::max(1, viewport.height());
    double a = w * h, b = padding * (w + h), c = padding * padding - maxPixels;
    double maximum = (-b + std::sqrt(b * b - 4 * a * c)) / (2 * a);
    return std::min(nativeDpr, maximum);
}

void PdfView::layoutPages()
{
    m_pages.clear();
    m_pending.clear();
    if (m_sizes.isEmpty()) return;
    double largest = 1;
    for (const auto &size : m_sizes) largest = std::max(largest, size.width());
    m_scale = std::max(32, viewport()->width() - 48) / largest * m_zoom;
    double dpr = devicePixelRatioF();
    m_renderDpr = renderDensity(viewport()->size(), dpr);
    double workingBytes = (viewport()->width() * m_renderDpr + 1024)
        * (viewport()->height() * m_renderDpr + 1024) * 4 + 2 * 1024 * 1024;
    m_cache.setMaxCost(std::clamp(qCeil(workingBytes / 1024), 32 * 1024, 128 * 1024));
    m_dpi1000 = qRound(72 * m_scale * m_renderDpr * 1000);
    double width = std::max(double(viewport()->width()), largest * m_scale + 48);
    double y = 24;
    for (const auto &size : m_sizes)
    {
        double pageWidth = qRound(size.width() * m_scale * dpr) / dpr;
        double pageHeight = qRound(size.height() * m_scale * dpr) / dpr;
        double x = qRound((width - pageWidth) / 2 * dpr) / dpr;
        y = qRound(y * dpr) / dpr;
        m_pages.append(QRectF(x, y, pageWidth, pageHeight));
        y += pageHeight + 16;
    }
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setRange(0, std::max(0, int(std::ceil(y + 8 - viewport()->height()))));
    verticalScrollBar()->setSingleStep(24);
    horizontalScrollBar()->setPageStep(viewport()->width());
    horizontalScrollBar()->setRange(0, std::max(0, int(std::ceil(width - viewport()->width()))));
    viewport()->update();
}

void PdfView::paintEvent(QPaintEvent *)
{
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), QColor("#22262C"));
    if (m_pages.isEmpty())
    {
        painter.setPen(QColor("#9AA6B5"));
        painter.drawText(viewport()->rect().adjusted(24, 24, -24, -24), Qt::AlignCenter,
            !m_error.isEmpty() ? m_error : m_path.isEmpty() ? "PDF sẽ hiện ở đây sau khi biên dịch.\nCtrl+B để build · Ctrl+Enter để tìm vị trí PDF" : "Đang mở PDF…");
        return;
    }
    QPointF scroll = offset();
    QRectF visible(scroll, QSizeF(viewport()->size()));
    double dpr = m_renderDpr;
    m_pending.clear();
    constexpr int tileSize = 512;
    auto first = std::lower_bound(m_pages.cbegin(), m_pages.cend(), visible.top(),
        [](const QRectF &page, double y) { return page.bottom() < y; });
    for (int number = int(first - m_pages.cbegin()); number < m_pages.size(); ++number)
    {
        const auto &page = m_pages[number];
        if (page.top() > visible.bottom()) break;
        if (!page.intersects(visible)) continue;
        QRectF onScreen = page.translated(-scroll);
        painter.fillRect(onScreen, Qt::white);
        if (m_failedPages.contains(number))
        {
            painter.setPen(QColor("#555555"));
            painter.drawText(onScreen.intersected(QRectF(viewport()->rect())), Qt::AlignCenter,
                "Không render được trang này.");
            continue;
        }
        QRectF local = page.intersected(visible).translated(-page.topLeft());
        int maxWidth = qCeil(m_sizes[number].width() * m_dpi1000 / 72000.0);
        int maxHeight = qCeil(m_sizes[number].height() * m_dpi1000 / 72000.0);
        int startX = std::max(0, int(local.left() * dpr) / tileSize * tileSize);
        int startY = std::max(0, int(local.top() * dpr) / tileSize * tileSize);
        int endX = std::min(maxWidth, qCeil(local.right() * dpr));
        int endY = std::min(maxHeight, qCeil(local.bottom() * dpr));
        for (int y = startY; y < endY; y += tileSize)
            for (int x = startX; x < endX; x += tileSize)
            {
                Tile tile{m_generation, number, m_dpi1000,
                    QRect(x, y, std::min(tileSize, maxWidth - x), std::min(tileSize, maxHeight - y))};
                auto *image = m_cache.object(tile.key());
                if (!image) { m_pending.append(tile); continue; }
                QRectF target(onScreen.topLeft() + QPointF(x / dpr, y / dpr),
                    QSizeF(image->width() / dpr, image->height() / dpr));
                // Native pixel density: no low-resolution full-page bitmap enlarged at fractional scale.
                painter.drawImage(target, *image);
            }
        painter.setPen(QColor("#333942"));
        painter.drawRect(onScreen);
    }
    if (!m_busy && !m_pending.isEmpty()) QTimer::singleShot(0, this, &PdfView::nextTile);
}

void PdfView::nextTile()
{
    if (m_busy) return;
    for (const auto &tile : m_pending)
    {
        if (tile.generation != m_generation || tile.dpi1000 != m_dpi1000 || m_cache.contains(tile.key())) continue;
        m_busy = true;
        ++m_renderRequests;
        emit tileRequested(tile);
        return;
    }
}

void PdfView::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    layoutPages();
}

void PdfView::scrollContentsBy(int, int) { viewport()->update(); }

bool PdfView::event(QEvent *event)
{
    bool result = QAbstractScrollArea::event(event);
    if (event->type() == QEvent::DevicePixelRatioChange) layoutPages();
    return result;
}

void PdfView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier))
    {
        setZoom(m_zoom * std::pow(1.12, event->angleDelta().y() / 120.0));
        event->accept();
    }
    else QAbstractScrollArea::wheelEvent(event);
}

void PdfView::setZoom(double zoom)
{
    double fraction = verticalScrollBar()->maximum() > 0
        ? double(verticalScrollBar()->value()) / verticalScrollBar()->maximum() : 0;
    m_zoom = std::clamp(zoom, 0.25, 6.0);
    layoutPages();
    verticalScrollBar()->setValue(qRound(fraction * verticalScrollBar()->maximum()));
}

void PdfView::fitWidth() { setZoom(1); }

void PdfView::goToPage(int page, double yPoints)
{
    if (page < 1 || page > m_pages.size()) return;
    verticalScrollBar()->setValue(qRound(m_pages[page - 1].top() + yPoints * m_scale - 32));
}

int PdfView::currentPage() const
{
    double y = verticalScrollBar()->value() + viewport()->height() / 3.0;
    for (int i = 0; i < m_pages.size(); ++i) if (m_pages[i].bottom() >= y) return i + 1;
    return std::max(1, int(m_pages.size()));
}

void PdfView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QPointF point = event->position() + offset();
    for (int i = 0; i < m_pages.size(); ++i)
        if (m_pages[i].contains(point))
        {
            QPointF local = (point - m_pages[i].topLeft()) / m_scale;
            emit sourceRequested(i + 1, local.x(), local.y());
            break;
        }
    QAbstractScrollArea::mouseDoubleClickEvent(event);
}

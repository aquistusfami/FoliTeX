#include "pdfview.h"
#include "editor.h"
#include <QFile>
#include <QPainter>
#include <QScrollBar>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QMenu>
#include <QClipboard>
#include <QGuiApplication>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <cmath>
#include <algorithm>

// Poppler decodes every embedded image again on each render call, so a page is rendered in
// as few calls as possible: whole when it fits wholePagePixels, else in large tiles.
// Measured on an image-heavy page: 1 call 0.87 s, 15 tiles of 512 px 10.3 s.
constexpr int tileSize = 1024;
constexpr double wholePagePixels = 8e6; // 32 MiB at ARGB32

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
        emit loaded(generation, {}, "Cannot open PDF: " + file.errorString());
        return;
    }
    // A snapshot lets latexmk replace/rebuild its output without changing an in-flight render.
    // Poppler copies these bytes; do not retain a second whole-document copy.
    m_document = Poppler::Document::loadFromData(file.readAll());
    if (!m_document || m_document->isLocked())
    {
        emit loaded(generation, {}, "Invalid or password-protected PDF.");
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
            emit loaded(generation, {}, "Cannot read a PDF page.");
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

void PdfRenderer::inspect(int generation, int number)
{
    PdfPageText text;
    if (m_document && generation == m_generation)
        if (auto page = m_document->page(number))
        {
            for (const auto &box : page->textList())
                text.words.append({box->boundingBox(), box->text(), box->hasSpaceAfter()});
            QSizeF size = page->pageSizeF();
            for (const auto &link : page->links())
            {
                // Poppler gives normalized areas, sometimes with negative height.
                QRectF area = link->linkArea().normalized();
                PdfLink entry;
                entry.area = QRectF(area.x() * size.width(), area.y() * size.height(),
                    area.width() * size.width(), area.height() * size.height());
                if (link->linkType() == Poppler::Link::Browse)
                    entry.url = static_cast<Poppler::LinkBrowse *>(link.get())->url();
                else if (link->linkType() == Poppler::Link::Goto)
                {
                    auto destination = static_cast<Poppler::LinkGoto *>(link.get())->destination();
                    if (destination.pageNumber() <= 0 && !destination.destinationName().isEmpty())
                        if (auto named = m_document->linkDestination(destination.destinationName())) destination = *named;
                    entry.page = destination.pageNumber();
                    if (auto target = m_document->page(entry.page - 1); target && destination.isChangeTop())
                        entry.top = destination.top() * target->pageSizeF().height();
                }
                if (entry.page > 0 || !entry.url.isEmpty()) text.links.append(entry);
            }
        }
    emit inspected(generation, number, text);
}

// shortcut: searches every page in one call, which delays tile rendering on long documents; split per page if that shows.
void PdfRenderer::search(int generation, const QString &query, int flags)
{
    QList<PdfMatch> matches;
    if (m_document && generation == m_generation)
        for (int i = 0; i < m_document->numPages(); ++i)
            if (auto page = m_document->page(i))
                for (const QRectF &box : page->search(query, Poppler::Page::SearchFlags(flags)))
                    matches.append({i, box});
    emit searched(generation, query, flags, matches);
}

PdfView::PdfView(QWidget *parent) : QAbstractScrollArea(parent), m_renderer(new PdfRenderer)
{
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    setObjectName("pdfView");
    qRegisterMetaType<Tile>();
    qRegisterMetaType<QList<QSizeF>>();
    qRegisterMetaType<PdfPageText>();
    qRegisterMetaType<QList<PdfMatch>>();
    viewport()->setMouseTracking(true);
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
    connect(this, &PdfView::textRequested, m_renderer, &PdfRenderer::inspect);
    connect(m_renderer, &PdfRenderer::inspected, this, [this](int generation, int page, const PdfPageText &text)
    {
        if (generation != m_generation) return;
        m_textRequested.remove(page);
        m_text.insert(page, new PdfPageText(text));
    });
    connect(this, &PdfView::searchRequested, m_renderer, &PdfRenderer::search);
    connect(m_renderer, &PdfRenderer::searched, this, [this](int generation, const QString &query, int flags, const QList<PdfMatch> &matches)
    {
        if (generation != m_generation || query != m_query || flags != m_searchFlags) return;
        m_searching = false;
        m_matches = matches;
        // Start at the page in view, as other viewers do.
        int page = currentPage() - 1;
        int first = int(std::lower_bound(m_matches.cbegin(), m_matches.cend(), page,
            [](const PdfMatch &match, int number) { return match.page < number; }) - m_matches.cbegin());
        m_match = m_matches.isEmpty() ? -1 : (m_searchBackwards ? first - 1 + m_matches.size() : first) % m_matches.size();
        showMatch();
    });
    // Zoom and resize steps reuse scaled tiles; one sharp render follows when the steps stop.
    m_dpiTimer.setSingleShot(true);
    m_dpiTimer.setInterval(150);
    connect(&m_dpiTimer, &QTimer::timeout, this, &PdfView::applyDpi);
    // Pages far away are re-rendered on demand, so their tiles need not stay in memory.
    m_trimTimer.setSingleShot(true);
    m_trimTimer.setInterval(5000);
    connect(&m_trimTimer, &QTimer::timeout, this, &PdfView::trimCache);
    // A wheel notch moves about a tenth of a page; touchpad travel is doubled for long documents.
    new SmoothScroll(this, 120, 2);
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
            emit loadFailed(QString("Cannot render page %1; reopen the PDF to retry.").arg(tile.page + 1));
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
    m_previousDpi1000 = 0;
    m_text.clear();
    m_textRequested.clear();
    m_selectionPage = m_pressPage = -1;
    clearSearch();
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
    constexpr double padding = 2 * tileSize;
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
    double largestPage = 1;
    for (const auto &size : m_sizes) largestPage = std::max(largestPage, size.width() * size.height());
    double pagePixels = std::min(wholePagePixels, largestPage * std::pow(m_scale * m_renderDpr, 2));
    double workingBytes = std::max((viewport()->width() * m_renderDpr + 2 * tileSize)
        * (viewport()->height() * m_renderDpr + 2 * tileSize), 2 * pagePixels) * 4 + 2 * 1024 * 1024;
    m_cache.setMaxCost(std::clamp(qCeil(workingBytes / 1024), 32 * 1024, 128 * 1024));
    m_targetDpi1000 = qRound(72 * m_scale * m_renderDpr * 1000);
    if (m_cache.isEmpty()) applyDpi();
    else if (m_targetDpi1000 != m_dpi1000) m_dpiTimer.start();
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

void PdfView::applyDpi()
{
    m_dpiTimer.stop();
    if (m_targetDpi1000 == m_dpi1000) return;
    m_previousDpi1000 = m_dpi1000;
    m_dpi1000 = m_targetDpi1000;
    m_pending.clear();
    m_trimTimer.start();
    viewport()->update();
}

void PdfView::trimCache()
{
    QRectF visible(offset(), QSizeF(viewport()->size()));
    int first = -1, last = -1;
    for (int i = 0; i < m_pages.size(); ++i)
        if (m_pages[i].intersects(visible)) { if (first < 0) first = i; last = i; }
    for (const QString &key : m_cache.keys())
    {
        // Key: generation/page/dpi1000/x/y (Tile::key).
        QStringList parts = key.split('/');
        int page = parts.value(1).toInt();
        bool keep = parts.value(0).toInt() == m_generation && parts.value(2).toInt() == m_dpi1000
            && first >= 0 && page >= first - 2 && page <= last + 2;
        if (!keep) m_cache.remove(key);
    }
    m_previousDpi1000 = 0; // its tiles are gone; nothing to draw scaled any more
}

void PdfView::paintEvent(QPaintEvent *)
{
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), QColor("#16181B"));
    if (m_pages.isEmpty())
    {
        painter.setPen(QColor("#8793A2"));
        painter.drawText(viewport()->rect().adjusted(24, 24, -24, -24), Qt::AlignCenter,
            !m_error.isEmpty() ? m_error : m_path.isEmpty() ? "The PDF appears here after building.\nF5 to build · F7 to jump to the PDF" : "Opening PDF…");
        return;
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPointF scroll = offset();
    QRectF visible(scroll, QSizeF(viewport()->size()));
    bool settled = !m_dpiTimer.isActive();
    m_pending.clear();
    // Draws cached tiles of one resolution; returns false if any visible tile is missing.
    auto drawTiles = [&](int number, const QRectF &onScreen, int dpi1000, bool request)
    {
        // Logical pixels per tile pixel; exact device ratio when the tiles match the layout.
        double factor = settled && dpi1000 == m_dpi1000 ? 1 / m_renderDpr : m_scale * 72000.0 / dpi1000;
        QRectF local = m_pages[number].intersected(visible).translated(-m_pages[number].topLeft());
        int maxWidth = qCeil(m_sizes[number].width() * dpi1000 / 72000.0);
        int maxHeight = qCeil(m_sizes[number].height() * dpi1000 / 72000.0);
        bool whole = double(maxWidth) * maxHeight <= wholePagePixels;
        int step = whole ? std::max(maxWidth, maxHeight) : tileSize;
        int startX = whole ? 0 : std::max(0, int(local.left() / factor) / step * step);
        int startY = whole ? 0 : std::max(0, int(local.top() / factor) / step * step);
        int endX = whole ? maxWidth : std::min(maxWidth, qCeil(local.right() / factor));
        int endY = whole ? maxHeight : std::min(maxHeight, qCeil(local.bottom() / factor));
        bool complete = true;
        for (int y = startY; y < endY; y += step)
            for (int x = startX; x < endX; x += step)
            {
                Tile tile{m_generation, number, dpi1000,
                    QRect(x, y, std::min(step, maxWidth - x), std::min(step, maxHeight - y))};
                auto *image = m_cache.object(tile.key());
                if (!image)
                {
                    complete = false;
                    if (request) m_pending.append(tile);
                    continue;
                }
                QRectF target(onScreen.topLeft() + QPointF(x, y) * factor, QSizeF(image->size()) * factor);
                // Native pixel density when settled: no low-resolution bitmap enlarged at fractional scale.
                painter.drawImage(target, *image);
            }
        return complete;
    };
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
                "Cannot render this page.");
            continue;
        }
        painter.save();
        painter.setClipRect(onScreen);
        // Until the current resolution is ready, the previous one stands in, scaled.
        if (!drawTiles(number, onScreen, m_dpi1000, settled) && m_previousDpi1000)
        {
            drawTiles(number, onScreen, m_previousDpi1000, false);
            drawTiles(number, onScreen, m_dpi1000, false);
        }
        if (number == m_selectionPage)
            if (auto *text = m_text.object(number))
                for (int i = std::min(m_selectionFrom, m_selectionTo); i <= std::max(m_selectionFrom, m_selectionTo) && i < text->words.size(); ++i)
                {
                    const QRectF &box = text->words[i].box;
                    painter.fillRect(QRectF(onScreen.topLeft() + box.topLeft() * m_scale, box.size() * m_scale), QColor(0, 120, 212, 90));
                }
        auto [from, to] = std::equal_range(m_matches.cbegin(), m_matches.cend(), PdfMatch{number, {}},
            [](const PdfMatch &a, const PdfMatch &b) { return a.page < b.page; });
        for (auto match = from; match != to; ++match)
            painter.fillRect(QRectF(onScreen.topLeft() + match->box.topLeft() * m_scale, match->box.size() * m_scale),
                match - m_matches.cbegin() == m_match ? QColor(255, 140, 0, 150) : QColor(255, 210, 0, 90));
        painter.restore();
        painter.setPen(QColor("#22272E"));
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
    m_trimTimer.start();
}

void PdfView::scrollContentsBy(int, int)
{
    m_trimTimer.start();
    viewport()->update();
}

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

void PdfView::find(const QString &query, bool backwards, Poppler::Page::SearchFlags flags)
{
    if (query.isEmpty() || m_pages.isEmpty()) return;
    if (query != m_query || int(flags) != m_searchFlags)
    {
        m_query = query;
        m_searchFlags = int(flags);
        m_matches.clear();
        m_match = -1;
        m_searching = true;
        m_searchBackwards = backwards;
        emit searchRequested(m_generation, query, m_searchFlags);
        viewport()->update();
        return;
    }
    if (m_searching || m_matches.isEmpty()) return;
    m_match = (m_match + (backwards ? -1 : 1) + m_matches.size()) % m_matches.size();
    showMatch();
}

void PdfView::clearSearch()
{
    m_query.clear();
    m_matches.clear();
    m_match = -1;
    m_searching = false;
    emit matchChanged(-1, -1);
    viewport()->update();
}

void PdfView::showMatch()
{
    emit matchChanged(m_match, m_matches.size());
    viewport()->update();
    if (m_match < 0) return;
    const auto &match = m_matches[m_match];
    QPointF center = m_pages[match.page].topLeft() + match.box.center() * m_scale;
    verticalScrollBar()->setValue(qRound(center.y() - viewport()->height() / 3.0));
    horizontalScrollBar()->setValue(qRound(center.x() - viewport()->width() / 2.0));
}

int PdfView::currentPage() const
{
    double y = verticalScrollBar()->value() + viewport()->height() / 3.0;
    for (int i = 0; i < m_pages.size(); ++i) if (m_pages[i].bottom() >= y) return i + 1;
    return std::max(1, int(m_pages.size()));
}

int PdfView::pageAt(QPointF position, QPointF *local) const
{
    QPointF point = position + offset();
    for (int i = 0; i < m_pages.size(); ++i)
        if (m_pages[i].contains(point))
        {
            if (local) *local = (point - m_pages[i].topLeft()) / m_scale;
            return i;
        }
    return -1;
}

// Text and links are extracted on the renderer thread on first use; nullptr until they arrive.
const PdfPageText *PdfView::pageText(int page)
{
    if (auto *text = m_text.object(page)) return text;
    if (!m_textRequested.contains(page)) { m_textRequested.insert(page); emit textRequested(m_generation, page); }
    return nullptr;
}

int PdfView::wordAt(const PdfPageText &text, QPointF local, bool nearest) const
{
    int best = -1;
    double distance = 1e18;
    for (int i = 0; i < text.words.size(); ++i)
    {
        const QRectF &box = text.words[i].box;
        if (box.contains(local)) return i;
        double dx = std::max({box.left() - local.x(), 0.0, local.x() - box.right()});
        double dy = std::max({box.top() - local.y(), 0.0, local.y() - box.bottom()});
        // Lines matter more than columns when the pointer is between words.
        double d = dx * dx + 16 * dy * dy;
        if (nearest && d < distance) { distance = d; best = i; }
    }
    return best;
}

// shortcut: no context if the page text was never extracted (no hover first); the jump then stays line-level.
void PdfView::requestSource(int page, QPointF local)
{
    QStringList words;
    int clicked = -1;
    if (const auto *text = pageText(page))
    {
        int i = wordAt(*text, local, false);
        for (int j = std::max(0, i - 4); i >= 0 && j <= std::min(int(text->words.size()) - 1, i + 4); ++j)
        {
            if (j == i) clicked = words.size();
            words << text->words[j].text;
        }
    }
    emit sourceRequested(page + 1, local.x(), local.y(), words, clicked);
}

QString PdfView::selectedText() const
{
    auto *text = m_text.object(m_selectionPage);
    if (!text || m_selectionFrom < 0) return {};
    QString result;
    int last = std::min(int(text->words.size()) - 1, std::max(m_selectionFrom, m_selectionTo));
    for (int i = std::min(m_selectionFrom, m_selectionTo); i <= last; ++i)
    {
        const auto &word = text->words[i];
        result += word.text;
        if (i == last) break;
        if (text->words[i + 1].box.top() > word.box.center().y()) result += '\n';
        else if (word.space) result += ' ';
    }
    return result;
}

void PdfView::copySelection() const
{
    QString text = selectedText();
    if (!text.isEmpty()) QGuiApplication::clipboard()->setText(text);
}

void PdfView::followLink(const PdfLink &link)
{
    if (!link.url.isEmpty()) QDesktopServices::openUrl(QUrl(link.url));
    else goToPage(link.page, link.top);
}

void PdfView::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    QPointF local;
    int page = pageAt(event->position(), &local);
    if (event->button() != Qt::LeftButton || page < 0) return QAbstractScrollArea::mousePressEvent(event);
    // Ctrl+click is inverse search, as in TeXstudio.
    if (event->modifiers() & Qt::ControlModifier) { requestSource(page, local); return; }
    m_pressPage = page;
    m_pressPosition = event->position();
    m_dragging = false;
    m_selectionPage = -1;
    pageText(page);
    viewport()->update();
}

void PdfView::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->buttons() & Qt::LeftButton) && m_pressPage >= 0)
    {
        if (!m_dragging && (event->position() - m_pressPosition).manhattanLength() < QApplication::startDragDistance()) return;
        m_dragging = true;
        auto *text = pageText(m_pressPage);
        if (!text) return;
        QPointF start = (m_pressPosition + offset() - m_pages[m_pressPage].topLeft()) / m_scale;
        QPointF end = (event->position() + offset() - m_pages[m_pressPage].topLeft()) / m_scale;
        m_selectionPage = m_pressPage;
        m_selectionFrom = wordAt(*text, start, true);
        m_selectionTo = wordAt(*text, end, true);
        viewport()->update();
        return;
    }
    QPointF local;
    int page = pageAt(event->position(), &local);
    auto *text = page >= 0 ? pageText(page) : nullptr;
    Qt::CursorShape shape = Qt::ArrowCursor;
    if (text)
    {
        for (const auto &link : text->links) if (link.area.contains(local)) shape = Qt::PointingHandCursor;
        if (shape == Qt::ArrowCursor && wordAt(*text, local, false) >= 0) shape = Qt::IBeamCursor;
    }
    viewport()->setCursor(shape);
}

void PdfView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_dragging && m_pressPage >= 0)
        if (auto *text = m_text.object(m_pressPage))
        {
            QPointF local = (m_pressPosition + offset() - m_pages[m_pressPage].topLeft()) / m_scale;
            for (const auto &link : text->links)
                if (link.area.contains(local)) { followLink(link); break; }
        }
    m_pressPage = -1;
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void PdfView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QPointF local;
    int page = pageAt(event->position(), &local);
    if (auto *text = page >= 0 ? pageText(page) : nullptr)
    {
        m_selectionPage = page;
        m_selectionFrom = m_selectionTo = wordAt(*text, local, false);
        viewport()->update();
    }
    // Not forwarded: QWidget's default turns a double-click into a press, which clears the selection.
    event->accept();
}

void PdfView::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy)) { copySelection(); return; }
    QAbstractScrollArea::keyPressEvent(event);
}

void PdfView::contextMenuEvent(QContextMenuEvent *event)
{
    QPointF local;
    int page = pageAt(event->pos(), &local);
    QMenu menu(this);
    auto *jump = menu.addAction("Jump to Source\tCtrl+Click", this, [this, page, local] { requestSource(page, local); });
    jump->setEnabled(page >= 0);
    auto *copy = menu.addAction("Copy", this, &PdfView::copySelection);
    copy->setShortcut(QKeySequence::Copy);
    copy->setEnabled(!selectedText().isEmpty());
    menu.addSeparator();
    menu.addAction("Zoom In", this, [this] { setZoom(m_zoom * 1.25); });
    menu.addAction("Zoom Out", this, [this] { setZoom(m_zoom / 1.25); });
    menu.addAction("Fit Width", this, &PdfView::fitWidth)->setShortcut(QKeySequence("Ctrl+0"));
    menu.addSeparator();
    menu.addAction("Open in External Viewer", this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(m_path)); })->setEnabled(!m_path.isEmpty());
    menu.exec(event->globalPos());
}

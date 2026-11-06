#pragma once
#include <QAbstractScrollArea>
#include <QCache>
#include <QThread>
#include <QImage>
#include <QSet>
#include <QTimer>
#include <poppler-qt6.h>
#include <memory>

struct Tile
{
    int generation = 0;
    int page = 0;
    int dpi1000 = 72000;
    QRect pixels;
    QString key() const;
};
Q_DECLARE_METATYPE(Tile)

struct PdfWord
{
    QRectF box; // points, top-left origin
    QString text;
    bool space = false;
};

struct PdfLink
{
    QRectF area; // points
    int page = 0; // 1-based target for internal links
    double top = 0; // points from the target page top
    QString url;
};

struct PdfPageText
{
    QList<PdfWord> words;
    QList<PdfLink> links;
};
Q_DECLARE_METATYPE(PdfPageText)

class PdfRenderer : public QObject
{
    Q_OBJECT
public slots:
    void open(const QString &path, int generation);
    void render(Tile tile);
    void inspect(int generation, int page);
signals:
    void inspected(int generation, int page, PdfPageText text);
    void loaded(int generation, QList<QSizeF> sizes, QString error);
    void rendered(Tile tile, QImage image);
private:
    std::unique_ptr<Poppler::Document> m_document;
    std::unique_ptr<Poppler::Page> m_page;
    int m_generation = 0;
    int m_pageNumber = -1;
};

class PdfView : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit PdfView(QWidget *parent = nullptr);
    ~PdfView() override;
    void open(const QString &path);
    void fitWidth();
    void setZoom(double zoom);
    void goToPage(int page, double yPoints = 0);
    int pageCount() const { return m_sizes.size(); }
    int currentPage() const;
    int cachedTiles() const { return m_cache.size(); }
    int cacheKiB() const { return m_cache.totalCost(); }
    int cacheLimitKiB() const { return m_cache.maxCost(); }
    bool renderingIdle() const { return !m_busy && m_pending.isEmpty(); }
    quint64 renderRequests() const { return m_renderRequests; }
    static double renderDensity(QSize viewport, double nativeDpr);
    double zoom() const { return m_zoom; }
    QString selectedText() const;
    void copySelection() const;
    QString path() const { return m_path; }
signals:
    void loadRequested(QString path, int generation);
    void tileRequested(Tile tile);
    void textRequested(int generation, int page);
    void documentReady(int pageCount);
    void loadFailed(QString error);
    void sourceRequested(int page, double xPoints, double yPoints);
    void pageChanged(int page);
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int, int) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;
private slots:
    void tileFinished(Tile tile, QImage image);
private:
    void layoutPages();
    void applyDpi();
    void nextTile();
    QPointF offset() const;
    int pageAt(QPointF position, QPointF *local = nullptr) const;
    const PdfPageText *pageText(int page);
    int wordAt(const PdfPageText &text, QPointF local, bool nearest) const;
    void followLink(const PdfLink &link);
    QThread m_thread;
    PdfRenderer *m_renderer;
    QList<QSizeF> m_sizes;
    QList<QRectF> m_pages;
    QList<Tile> m_pending;
    QCache<QString, QImage> m_cache{32 * 1024};
    QString m_path;
    QString m_error;
    bool m_busy = false;
    int m_generation = 0;
    int m_dpi1000 = 72000;
    int m_targetDpi1000 = 72000;
    int m_previousDpi1000 = 0;
    QTimer m_dpiTimer;
    double m_zoom = 1;
    double m_scale = 1;
    double m_renderDpr = 1;
    quint64 m_renderRequests = 0;
    QSet<int> m_failedPages;
    // shortcut: text and links of the 8 most recent pages; selection stays within one page, extend if multi-page copy is needed.
    QCache<int, PdfPageText> m_text{8};
    QSet<int> m_textRequested;
    int m_selectionPage = -1;
    int m_selectionFrom = -1;
    int m_selectionTo = -1;
    int m_pressPage = -1;
    QPointF m_pressPosition;
    bool m_dragging = false;
};

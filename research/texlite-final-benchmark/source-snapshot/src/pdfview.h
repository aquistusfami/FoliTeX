#pragma once
#include <QAbstractScrollArea>
#include <QCache>
#include <QThread>
#include <QImage>
#include <QSet>
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

class PdfRenderer : public QObject
{
    Q_OBJECT
public slots:
    void open(const QString &path, int generation);
    void render(Tile tile);
signals:
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
    QString path() const { return m_path; }
signals:
    void loadRequested(QString path, int generation);
    void tileRequested(Tile tile);
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
    bool event(QEvent *event) override;
private slots:
    void tileFinished(Tile tile, QImage image);
private:
    void layoutPages();
    void nextTile();
    QPointF offset() const;
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
    double m_zoom = 1;
    double m_scale = 1;
    double m_renderDpr = 1;
    quint64 m_renderRequests = 0;
    QSet<int> m_failedPages;
};

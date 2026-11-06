#pragma once
#include <QMainWindow>
#include <functional>
#include <QHash>
class QLineEdit;
#include <QProcess>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QSplitter>
#include <QSpinBox>
#include <QFileSystemWatcher>
#include <QTimer>
#include "editor.h"
#include "pdfview.h"

class BuildRunner : public QObject
{
    Q_OBJECT
public:
    explicit BuildRunner(QObject *parent = nullptr);
    ~BuildRunner() override;
    bool start(const QString &source, const QString &engine);
    bool running() const { return m_process.state() != QProcess::NotRunning; }
    void stop();
    static QString rootFile(const QString &source);
    static QString pdfPath(const QString &source);
signals:
    void output(QString text);
    void finished(bool success, QString pdf);
private:
    QProcess m_process;
    QString m_pdf;
    bool m_cancelRequested = false;
    QHash<qint64, quint64> m_cancelGroups;
    void finishCancellation(qint64 pid);
};

class Window : public QMainWindow
{
    Q_OBJECT
public:
    explicit Window();
    bool openSource(const QString &path);
    void openPdf(const QString &path);
    Editor *currentEditor() const;
    PdfView *pdfView() const { return m_pdf; }
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    Editor *newDocument(const QString &text = {});
    bool save(Editor *editor, bool saveAs = false);
    bool mayClose(Editor *editor);
    void closeTab(int index);
    void build();
    void setTabTitle(Editor *editor);
    void forwardSync();
    void inverseSync(int page, double x, double y);
    void syncProcess(QStringList arguments, std::function<void(QString)> complete);
    QTabWidget *m_tabs;
    PdfView *m_pdf;
    QPlainTextEdit *m_log;
    QComboBox *m_engine;
    QPushButton *m_build;
    QLabel *m_status;
    QLabel *m_pageCount;
    QSpinBox *m_page;
    QSplitter *m_horizontal;
    QSplitter *m_vertical;
    QWidget *m_findBar;
    QLineEdit *m_find;
    BuildRunner m_runner;
    QFileSystemWatcher m_watcher;
    QTimer m_reload;
    int m_pendingPage = 0;
    double m_pendingY = 0;
};

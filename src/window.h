#pragma once
#include <QMainWindow>
#include <functional>
#include <QHash>
#include <QSet>
class QLineEdit;
class QTreeWidget;
class QTreeView;
class QFileSystemModel;
class QStackedWidget;
class QToolButton;
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
    QTreeWidget *outlineView() const { return m_outline; }
    QTreeView *filesView() const { return m_files; }
    void openWorkspace(const QString &directory);
    void setWordWrap(bool on);
    bool wordWrap() const { return m_wordWrap; }
    QString workspace() const { return m_workspace; }
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
    void reloadSource(Editor *editor);
    void watchSource(const QString &path);
    void refreshOutline();
    void showSide(QWidget *page, bool toggle = true);
    void syncOutline();
    void syncProcess(QStringList arguments, std::function<void(QString)> complete);
    QTabWidget *m_tabs;
    PdfView *m_pdf;
    QWidget *m_pdfPane;
    QPlainTextEdit *m_log;
    QComboBox *m_engine;
    QPushButton *m_build;
    QIcon m_runIcon;
    QAction *m_wrapAction;
    bool m_wordWrap = false;
    QIcon m_stopIcon;
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
    QFileSystemWatcher m_sourceWatcher;
    QTimer m_sourceReload;
    QSet<QString> m_changedSources;
    QTreeWidget *m_outline;
    QTimer m_outlineTimer;
    QWidget *m_side;
    QLabel *m_sideTitle;
    QStackedWidget *m_sideStack;
    QWidget *m_noFolder;
    QToolButton *m_filesButton;
    QToolButton *m_outlineButton;
    QTreeView *m_files;
    QFileSystemModel *m_fileModel;
    QString m_workspace;
    int m_pendingPage = 0;
    double m_pendingY = 0;
};

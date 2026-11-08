#pragma once
#include <QMainWindow>
#include <functional>
#include <QHash>
#include <QSet>
class QLineEdit;
class QCheckBox;
class QTreeWidget;
class QTreeView;
class QFileSystemModel;
class QStackedWidget;
class QToolButton;
class QMenuBar;
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
#include <QDateTime>
#include "editor.h"
#include "pdfview.h"

// One error or warning from a LaTeX log, located in a source file of the project.
struct Problem
{
    QString file; // absolute path
    int line = 0; // 1-based; 0 when the log gives no line
    bool error = true;
    QString message;
};

class BuildRunner : public QObject
{
    Q_OBJECT
public:
    explicit BuildRunner(QObject *parent = nullptr);
    ~BuildRunner() override;
    bool start(const QString &source, const QString &engine);
    bool running() const { return m_process.state() != QProcess::NotRunning; }
    bool cancelled() const { return m_cancelRequested; }
    void stop();
    static QString rootFile(const QString &source);
    static QString pdfPath(const QString &source);
    // Errors (file:line: from -file-line-error), LaTeX/package warnings and over/underfull boxes.
    static QList<Problem> parseLog(const QString &log, const QString &directory);
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
    ~Window() override;
    bool openSource(const QString &path);
    void openPdf(const QString &path);
    Editor *currentEditor() const;
    PdfView *pdfView() const { return m_pdf; }
    QTreeWidget *outlineView() const { return m_outline; }
    QTreeView *filesView() const { return m_files; }
    void openWorkspace(const QString &directory);
    void setWordWrap(bool on);
    bool wordWrap() const { return m_wordWrap; }
    void setMenuBarShown(bool on);
    bool menuBarShown() const { return m_menuBarShown; }
    QString workspace() const { return m_workspace; }
    // Open files, cursors and the PDF; main.cpp persists them so tests never touch the user's settings.
    QVariantMap session() const;
    void restoreSession(const QVariantMap &session);
    QStringList recentFiles() const { return m_recent; }
    void setRecentFiles(const QStringList &files) { m_recent = files; }
    bool buildOnSave() const { return m_buildOnSaveAction->isChecked(); }
    void setBuildOnSave(bool on) { m_buildOnSaveAction->setChecked(on); }
    void addRecent(const QString &path);
    QFont editorFont() const { return m_editorFont; }
    void setEditorFont(const QFont &font);
    QString engine() const { return m_engine->currentText(); }
    void setEngine(const QString &name) { if (m_engine->findText(name) >= 0) m_engine->setCurrentText(name); }
    void showSettings();
    // Built-in document templates, then the user's *.tex in templatesFolder(); text uses U+2022 placeholders.
    static QList<QPair<QString, QString>> builtinTemplates();
    static QString templatesFolder();
    Editor *newFromTemplate(const QString &text);
    // Full-screen slide show of the PDF from the page in view.
    PdfView *present();
protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    QTextDocument::FindFlags findFlags() const;
    void findInEditor(bool backwards);
    void updateFindCount();
    void placeFind();
    void updateTabButtons();
    void updateTabHints();
    void setProblems(const QList<Problem> &problems, const QString &directory);
    void applyProblems(Editor *editor);
    void goToProblem(int index);
    Editor *newDocument(const QString &text = {});
    bool save(Editor *editor, bool saveAs = false);
    bool mayClose(Editor *editor);
    void closeTab(int index);
    void build();
    void setTabTitle(Editor *editor);
    void forwardSync();
    void inverseSync(int page, double x, double y, const QStringList &words, int clicked);
    void reloadSource(Editor *editor);
    void watchSource(const QString &path);
    void refreshOutline();
    void showSide(QWidget *page, bool toggle = true);
    void syncOutline();
    void syncProcess(QStringList arguments, std::function<void(QString)> complete);
    void countWords();
    QTabWidget *m_tabs;
    QStringList m_recent;
    QFont m_editorFont;
    QString m_lastRoot; // root of the last build; what saving a .bib or .sty rebuilds
    QAction *m_buildOnSaveAction;
    PdfView *m_pdf;
    QWidget *m_pdfPane;
    QPlainTextEdit *m_log;
    QTabWidget *m_panel; // Problems and Output under the editor
    QTreeWidget *m_problemsView;
    QToolButton *m_errorCount;
    QToolButton *m_warningCount;
    QList<Problem> m_problems;
    int m_problemIndex = -1;
    QDateTime m_buildStarted;
    QDateTime m_problemsTime; // when m_problems were read
    QComboBox *m_engine;
    QPushButton *m_build;
    QIcon m_runIcon;
    QAction *m_wrapAction;
    bool m_wordWrap = false;
    QWidget *m_topBar;
    QMenuBar *m_menuBar;
    QAction *m_menuBarAction;
    bool m_menuBarShown = true;
    bool m_altAlone = false;
    bool m_viaShortcut = false; // an action is being triggered by its key, not from the menu
    QIcon m_stopIcon;
    QLabel *m_status;
    QLabel *m_pageCount;
    QSpinBox *m_page;
    QSplitter *m_horizontal;
    QSplitter *m_vertical;
    QWidget *m_findBar;
    QLineEdit *m_find;
    QWidget *m_replaceRow;
    QLineEdit *m_replace;
    QToolButton *m_replaceToggle;
    QToolButton *m_findCase;
    QToolButton *m_findWord;
    QLabel *m_findCount;
    QWidget *m_pdfFindBar;
    QLineEdit *m_pdfFind;
    QLabel *m_pdfMatches;
    QCheckBox *m_pdfCase;
    QCheckBox *m_pdfWord;
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
    QToolButton *m_rootButton;
    QTreeView *m_files;
    QFileSystemModel *m_fileModel;
    QString m_workspace;
    int m_hoverTab = -1;
    int m_tabScroll = 0; // wheel travel not yet turned into a tab step
    int m_pendingPage = 0;
    double m_pendingY = 0;
};

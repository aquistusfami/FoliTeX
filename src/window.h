#pragma once
#include <QMainWindow>
#include <functional>
#include <QHash>
#include <QPointer>
#include <QSet>
class QLineEdit;
class QCheckBox;
class QTreeWidget;
class QTreeView;
class QFileSystemModel;
class QStackedWidget;
class QToolButton;
class QMenuBar;
class Palette;
class Editor;
#include <QProcess>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
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
    bool darkPdf() const { return m_pdf->darkPages(); }
    void setDarkPdf(bool on) { m_darkButton->setChecked(on); }
    void setMenuBarShown(bool on);
    bool menuBarShown() const { return m_menuBarShown; }
    QString workspace() const { return m_workspace; }
    // Open files, cursors and the PDF; main.cpp persists them so tests never touch the user's settings.
    QVariantMap session() const;
    void restoreSession(const QVariantMap &session);
    QStringList recentFiles() const { return m_recent; }
    void setRecentFiles(const QStringList &files) { m_recent = files; delete m_welcome; updateWelcome(); } // the start actions list them
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
    // Explorer: a new file or folder with a free default name, renamed inline; returns its path.
    QString newEntry(const QString &directory, bool folder);
    // Moves a file or folder to the trash (after asking) and closes its unmodified tabs.
    bool deleteEntry(const QString &path, bool ask = true);
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
    void entryRenamed(const QString &from, const QString &to);
    void explorerMenu(const QPoint &position);
    QString m_newFile; // created from the explorer, opened once its name is confirmed
    QToolButton *m_collapseButton;
    QSet<QString> m_expandedFolders; // Collapse Folders is enabled only while one is open
    QString m_pendingRename; // created on disk, renamed inline once the model lists it
    void startRename(const QString &path);
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
    QToolButton *m_darkButton;
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
    // Navigation history: a jump of more than 10 lines, or another tab, leaves a place to go back to.
    struct Place { QPointer<Editor> editor; int line = 0; };
    QList<Place> m_back, m_forward;
    Place m_here;
    bool m_navigating = false;
    QToolButton *m_backButton;
    QToolButton *m_forwardButton;
    Palette *m_palette;
    QList<QPointer<QWidget>> m_zenHidden; // what distraction-free mode hid, shown again by the next toggle
    void toggleZen();
    void placeZen();
    bool m_panelCollapsed = false;
    void setPanelCollapsed(bool on); // only the Problems | Output strip while there is nothing to report
    QWidget *m_pdfBar;
    void setPdfLoaded(bool loaded);
    class QSpinBox *m_zoomBox;
    double m_pdfScale = 0;
    QMenu *m_templatesMenu;
    QPointer<QWidget> m_welcome; // start actions over an empty Untitled tab
    void updateWelcome();
    void placeWelcome();
    void openRecent(const QString &path);
    QLabel *m_zenHint = nullptr;
    class QHBoxLayout *m_row; // holds the splitter; centres it in focus mode
    QWidget *m_problemsEmpty;
    void noteLocation();
    void navigate(bool forward);
    void showPalette(bool files);
    QLabel *m_position; // right of the status bar: cursor and line ending of the current tab
    void updatePosition();
    QLabel *m_pageCount;
    QSpinBox *m_page;
    QSplitter *m_horizontal;
    QSplitter *m_vertical = nullptr;
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

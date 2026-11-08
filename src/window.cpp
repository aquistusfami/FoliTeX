#include "window.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QMenu>
#include <QMenuBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QCheckBox>
#include <QLineEdit>
#include <QShortcut>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QSignalBlocker>
#include <QSet>
#include <QStringDecoder>
#include <QInputDialog>
#include <QStatusBar>
#include <QTabBar>
#include <QTreeWidget>
#include <QHeaderView>
#include <QTreeView>
#include <QFileSystemModel>
#include <QUrl>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QDialog>
#include <QAbstractFileIconProvider>
#include <QSettings>
#include <QStackedWidget>
#include <QImageReader>
#include <QBuffer>
#ifdef Q_OS_UNIX
#include <unistd.h>
#include <signal.h>
#endif

BuildRunner::BuildRunner(QObject *parent) : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, &QProcess::started, this, [this] { if (m_cancelRequested) stop(); });
#ifdef Q_OS_UNIX
    m_process.setChildProcessModifier([] { ::setsid(); });
#endif
    connect(&m_process, &QProcess::readyRead, this, [this] { emit output(QString::fromLocal8Bit(m_process.readAll())); });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int exit, QProcess::ExitStatus status)
    {
        emit output(QString::fromLocal8Bit(m_process.readAll()));
        emit finished(status == QProcess::NormalExit && exit == 0 && QFileInfo::exists(m_pdf), m_pdf);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error)
    {
        if (error != QProcess::FailedToStart) return;
        emit output("Cannot start latexmk: " + m_process.errorString());
        emit finished(false, m_pdf);
    });
}

static quint64 processStartTime(qint64 pid)
{
#ifdef Q_OS_UNIX
    QFile stat(QString("/proc/%1/stat").arg(pid));
    if (!stat.open(QIODevice::ReadOnly)) return 0;
    QByteArray text = stat.readAll();
    auto fields = text.mid(text.lastIndexOf(')') + 2).split(' ');
    return fields.size() > 19 ? fields[19].toULongLong() : 0;
#else
    Q_UNUSED(pid);
    return 0;
#endif
}

void BuildRunner::finishCancellation(qint64 pid)
{
    if (pid <= 0 || !m_cancelGroups.contains(pid)) return;
    quint64 originalStart = m_cancelGroups.take(pid);
#ifdef Q_OS_UNIX
    quint64 currentStart = processStartTime(pid);
    // An exited leader may leave compiler children alive. A reused leader PID is not ours.
    if (currentStart && currentStart != originalStart) return;
    ::kill(-pid_t(pid), SIGKILL);
#else
    Q_UNUSED(originalStart);
    if (running() && m_process.processId() == pid) m_process.kill();
#endif
}

BuildRunner::~BuildRunner()
{
    if (running() && m_process.processId() > 0)
        m_cancelGroups.insert(m_process.processId(), processStartTime(m_process.processId()));
    for (qint64 pid : m_cancelGroups.keys()) finishCancellation(pid);
    if (running()) m_process.waitForFinished(1000);
}

QString BuildRunner::rootFile(const QString &source)
{
    QString root = QFileInfo(source).absoluteFilePath();
    QSet<QString> visited;
    for (int depth = 0; depth < 12; ++depth)
    {
        if (visited.contains(root)) return {};
        visited.insert(root);
        QFile file(root);
        if (!file.open(QIODevice::ReadOnly)) return {};
        QString head = QString::fromUtf8(file.read(16384));
        static const QRegularExpression magic(R"(^\s*%\s*!\s*TeX\s+root\s*=\s*(.+?)\s*$)",
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
        auto match = magic.match(head);
        if (!match.hasMatch()) return root;
        root = QDir(QFileInfo(root).absolutePath()).absoluteFilePath(match.captured(1).trimmed());
        root = QDir::cleanPath(root);
    }
    return {};
}

QString BuildRunner::pdfPath(const QString &source)
{
    QFileInfo info(source);
    return info.absolutePath() + "/.folitex-build/" + info.completeBaseName() + ".pdf";
}

QList<Problem> BuildRunner::parseLog(const QString &log, const QString &directory)
{
    static const QRegularExpression error(R"(^(\.{0,2}/[^:]*|[^:\s]+\.(?:tex|ltx|sty|cls|bbl)):(\d+): (.*)$)");
    static const QRegularExpression context(R"(^l\.\d+ (.*)$)");
    static const QRegularExpression warning(R"(^(?:LaTeX|Package (\S+)|Class (\S+)) Warning: (.*)$)");
    static const QRegularExpression inputLine(R"(on input line (\d+))");
    static const QRegularExpression box(R"(^((?:Over|Under)full \\[hv]box \([^)]*\))(?:.*? lines? (\d+))?)");
    QString base = QDir(directory).absolutePath() + '/';
    // "(" + path in the log opens a file and ")" closes it; other parentheses are pushed as "".
    QStringList files;
    QString root;
    auto current = [&]
    {
        for (auto it = files.crbegin(); it != files.crend(); ++it)
            if (it->startsWith(base)) return *it; // packages report against the document that loads them
        return root;
    };
    QList<Problem> problems;
    auto add = [&](const Problem &problem)
    {
        for (const auto &known : problems)
            if (known.file == problem.file && known.line == problem.line && known.message == problem.message) return;
        if (!problem.file.isEmpty()) problems << problem;
    };
    const QStringList lines = log.split('\n');
    for (int i = 0; i < lines.size(); ++i)
    {
        const QString text = lines[i].trimmed();
        QRegularExpressionMatch match;
        if ((match = error.match(text)).hasMatch())
        {
            Problem problem{QDir::cleanPath(QDir(directory).absoluteFilePath(match.captured(1))), match.captured(2).toInt(), true, match.captured(3).trimmed()};
            // An error inside a package (e.g. a bad option while geometry.sty loads) is reported in
            // the system file; it belongs to the project file that loaded it.
            if (!problem.file.startsWith(base))
                problem = {current(), 0, true, QFileInfo(problem.file).fileName() + ": " + problem.message};
            // TeX shows the offending source on the "l.<line>" line that follows.
            for (int j = i + 1; j < lines.size() && j <= i + 40 && !error.match(lines[j]).hasMatch(); ++j)
                if (auto source = context.match(lines[j]); source.hasMatch())
                {
                    if (QString code = source.captured(1).trimmed(); !code.isEmpty()) problem.message += "  " + code;
                    break;
                }
            add(problem);
        }
        else if ((match = warning.match(text)).hasMatch())
        {
            QString package = match.hasCaptured(1) ? match.captured(1) : match.captured(2);
            QString message = match.captured(3);
            // Package warnings continue on lines that start with "(package)".
            while (!package.isEmpty() && i + 1 < lines.size() && lines[i + 1].startsWith('(' + package + ')'))
                message += ' ' + lines[++i].mid(package.size() + 2).trimmed();
            auto at = inputLine.match(message);
            add({current(), at.hasMatch() ? at.captured(1).toInt() : 0, false, (package.isEmpty() ? "" : package + ": ") + message.simplified()});
        }
        else if ((match = box.match(text)).hasMatch())
            add({current(), match.captured(2).toInt(), false, match.captured(1)});
        for (int at = 0; at < text.size(); ++at)
        {
            if (text[at] == ')' && !files.isEmpty()) files.removeLast();
            if (text[at] != '(') continue;
            int end = at + 1;
            while (end < text.size() && !text[end].isSpace() && text[end] != '(' && text[end] != ')') ++end;
            QString name = text.mid(at + 1, end - at - 1);
            QString path = name.isEmpty() ? QString() : QDir::cleanPath(QDir(directory).absoluteFilePath(name));
            bool file = !path.isEmpty() && QFileInfo(path).isFile();
            files << (file ? path : QString());
            if (file && root.isEmpty() && path.startsWith(base)) root = path;
        }
    }
    return problems;
}

bool BuildRunner::start(const QString &source, const QString &engine)
{
    if (running()) return false;
    m_cancelRequested = false;
    QString root = rootFile(source);
    if (root.isEmpty())
    {
        emit output("Root file not found, or TeX root directives form a cycle.");
        emit finished(false, {});
        return false;
    }
    m_pdf = pdfPath(root);
    m_process.setWorkingDirectory(QFileInfo(root).absolutePath());
    // Unwrapped log lines (TeX folds at 79 characters by default), so messages parse whole.
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("max_print_line", "10000");
    environment.insert("error_line", "254");
    environment.insert("half_error_line", "238");
    m_process.setProcessEnvironment(environment);
    QString flag = engine == "LuaLaTeX" ? "-lualatex" : engine == "XeLaTeX" ? "-xelatex" : "-pdf";
    // Light PDF compression: images that cannot be copied as-is (e.g. PNGs with alpha) are
    // re-deflated on every run, at level 9 by default. Level 1 cut a screenshot-heavy report
    // from 3.0 s to 1.2 s per rebuild for ~12 % more PDF size; documents may still set their own.
    QString compression = engine == "LuaLaTeX" ? R"(\pdfvariable compresslevel=1)"
                        : engine == "XeLaTeX" ? R"(\special{dvipdfmx:config z 1})" : R"(\pdfcompresslevel=1)";
    // Arguments are passed directly, so spaces, Unicode and shell metacharacters stay filenames.
    m_process.start("latexmk", {flag, "-interaction=nonstopmode", "-file-line-error", "-synctex=1", "-usepretex=" + compression,
        "-outdir=.folitex-build", "./" + QFileInfo(root).fileName()});
    return true;
}

void BuildRunner::stop()
{
    if (!running()) return;
    m_cancelRequested = true;
    if (m_process.state() == QProcess::Starting) return;
    qint64 pid = m_process.processId();
    if (pid <= 0) return;
    m_cancelGroups.insert(pid, processStartTime(pid));
#ifdef Q_OS_UNIX
    if (pid > 0) ::kill(-pid_t(pid), SIGTERM);
#else
    m_process.terminate();
#endif
    QTimer::singleShot(1500, this, [this, pid] { finishCancellation(pid); });
}

// Renders an SVG icon whose strokes use currentColor; 3x density stays sharp at fractional scales.
static QPixmap svgPixmap(const char *svg, const char *color, int size)
{
    QByteArray data = themed(QString::fromUtf8(svg).replace("currentColor", color)).toUtf8();
    QBuffer buffer(&data);
    QImageReader reader(&buffer, "svg");
    reader.setScaledSize(QSize(size * 3, size * 3));
    QPixmap pixmap = QPixmap::fromImage(reader.read());
    pixmap.setDevicePixelRatio(3);
    return pixmap;
}

// Glyphs and colours in the style of VS Code's Seti icon theme (drawn for FoliTeX, not copied).
static QIcon fileIcon(const QString &fileName)
{
    static QHash<QString, QIcon> icons;
    QString suffix = QFileInfo(fileName).suffix().toLower();
    if (auto found = icons.constFind(suffix); found != icons.cend()) return *found;
    static const char *tex = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M3 3.5h10v2.2H9.2V13H6.8V5.7H3z" fill="currentColor"/></svg>)";
    static const char *bib = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M4 2.5h8v9H5.6A1.6 1.6 0 0 0 4 13.1zM5.6 12.3H12v1.2H5.6a.6.6 0 0 1 0-1.2z" fill="currentColor"/></svg>)";
    static const char *style = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="currentColor"><rect x="2.5" y="4" width="11" height="1.4" rx=".7"/><rect x="2.5" y="7.3" width="11" height="1.4" rx=".7"/><rect x="2.5" y="10.6" width="11" height="1.4" rx=".7"/><circle cx="5.5" cy="4.7" r="1.7"/><circle cx="10.5" cy="8" r="1.7"/><circle cx="7" cy="11.3" r="1.7"/></svg>)";
    static const char *pdf = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M4 2h5.5L12.5 5v9H4z" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linejoin="round"/><path d="M6 8.5h4.5M6 11h3" stroke="currentColor" stroke-width="1.3"/></svg>)";
    static const char *image = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><rect x="2.5" y="3.5" width="11" height="9" rx="1" fill="none" stroke="currentColor" stroke-width="1.3"/><path d="M3.5 11.5l3-3.5 2.2 2.3 1.6-1.5 2.2 2.7z" fill="currentColor"/></svg>)";
    static const char *other = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><path d="M3.5 5h9M3.5 8h9M3.5 11h6"/></svg>)";
    static const QHash<QString, std::pair<const char *, const char *>> kinds = {
        {"tex", {tex, "#8DC149"}}, {"ltx", {tex, "#8DC149"}}, {"bib", {bib, "#CBCB41"}},
        {"sty", {style, "#A074C4"}}, {"cls", {style, "#A074C4"}}, {"pdf", {pdf, "#CC3E44"}},
        {"png", {image, "#A074C4"}}, {"jpg", {image, "#A074C4"}}, {"jpeg", {image, "#A074C4"}}, {"svg", {image, "#A074C4"}}};
    auto [svg, color] = kinds.value(suffix, {other, "#68727F"});
    QIcon icon(svgPixmap(svg, color, 16));
    icons.insert(suffix, icon);
    return icon;
}

namespace
{
// Rounded panels (VS Code 1.145): paints the gap colour outside a rounded rectangle and a 1 px
// border on top of a panel. Its mask keeps it to the corners and the edge, so the panel's own
// square painting (editor viewport, PDF pages) stays untouched and clicks pass through.
class RoundedCorners : public QWidget
{
public:
    explicit RoundedCorners(QWidget *panel) : QWidget(panel)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        panel->installEventFilter(this);
        place();
    }
protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::Resize) place();
        else if (event->type() == QEvent::ChildAdded) raise(); // stay above widgets added later
        return false;
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath outside, inside;
        outside.addRect(rect());
        inside.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
        painter.fillPath(outside.subtracted(inside), themeColor("#0E1013"));
        painter.setPen(themeColor("#22272E"));
        painter.drawPath(inside);
    }
private:
    static constexpr int radius = 6;
    void place()
    {
        QRect area = parentWidget()->rect();
        setGeometry(area);
        QRegion mask = QRegion(area) - QRegion(area.adjusted(1, 1, -1, -1));
        for (QPoint corner : {area.topLeft(), QPoint(area.right() - radius, area.top()),
                              QPoint(area.left(), area.bottom() - radius), QPoint(area.right() - radius, area.bottom() - radius)})
            mask += QRect(corner, QSize(radius + 1, radius + 1));
        setMask(mask);
        raise();
    }
};

// No folder icons, only chevrons, like VS Code's explorer.
class FileIconProvider : public QAbstractFileIconProvider
{
public:
    QIcon icon(IconType) const override { return {}; }
    QIcon icon(const QFileInfo &info) const override { return info.isDir() ? QIcon() : fileIcon(info.fileName()); }
};
}

Window::Window() : m_runner(this)
{
    setWindowTitle("FoliTeX");
    resize(1280, 820);
    setMinimumSize(680, 420);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_topBar = new QWidget;
    auto *bar = new QHBoxLayout(m_topBar);
    bar->setContentsMargins(8, 2, 8, 0);
    auto *menuBar = new QMenuBar;
    m_menuBar = menuBar;
    menuBar->setNativeMenuBar(false);
    QMenu *menu = menuBar->addMenu("&File");
    auto add = [this, &menu](const QString &title, const QKeySequence &keys, auto handler)
    {
        auto *action = menu->addAction(title);
        action->setShortcut(keys);
        addAction(action);
        connect(action, &QAction::triggered, this, handler);
        return action;
    };
    auto insert = [this](const QString &text) { return [this, text] { if (auto *editor = currentEditor()) editor->insertTemplate(text); }; };
    auto find = [this](bool backwards)
    {
        if (m_pdfFindBar->isVisible() && m_pdfPane->isAncestorOf(QApplication::focusWidget())) emit m_pdfFind->returnPressed();
        else findInEditor(backwards);
    };
    auto showFind = [this](bool replace)
    {
        // Ctrl+F inside the PDF pane searches the PDF, elsewhere the current source.
        if (!replace && m_pdfPane->isAncestorOf(QApplication::focusWidget()))
        {
            m_pdfFindBar->show();
            m_pdfFind->setFocus();
            m_pdfFind->selectAll();
            return;
        }
        auto *editor = currentEditor();
        if (!editor) return;
        if (editor->textCursor().hasSelection() && !editor->textCursor().selectedText().contains(QChar::ParagraphSeparator))
            m_find->setText(editor->textCursor().selectedText());
        m_replaceToggle->setChecked(replace);
        m_findBar->show();
        placeFind();
        updateFindCount();
        m_find->setFocus();
        m_find->selectAll();
    };
    add("New document", QKeySequence::New, [this] { newDocument(); });
    auto *templates = menu->addMenu("New from template");
    connect(templates, &QMenu::aboutToShow, this, [this, templates]
    {
        templates->clear();
        for (const auto &[name, text] : builtinTemplates())
            templates->addAction(name, this, [this, text] { newFromTemplate(text); });
        const auto mine = QDir(templatesFolder()).entryInfoList({"*.tex"}, QDir::Files, QDir::Name);
        if (!mine.isEmpty()) templates->addSeparator();
        for (const auto &info : mine)
            templates->addAction(info.completeBaseName(), this, [this, path = info.absoluteFilePath()]
            {
                QFile file(path);
                if (file.open(QIODevice::ReadOnly)) newFromTemplate(QString::fromUtf8(file.readAll()));
            });
        templates->addSeparator();
        templates->addAction("Open templates folder", this, [] 
        {
            QDir().mkpath(templatesFolder());
            QDesktopServices::openUrl(QUrl::fromLocalFile(templatesFolder()));
        });
    });
    templates->addAction("Article")->setVisible(false); // a submenu with no actions never opens
    add("Open TeX or PDF…", QKeySequence::Open, [this]
    {
        QString path = QFileDialog::getOpenFileName(this, "Open document", m_workspace, "TeX / PDF (*.tex *.bib *.sty *.cls *.pdf);;All files (*)");
        if (path.isEmpty()) return;
        if (path.endsWith(".pdf", Qt::CaseInsensitive)) openPdf(path); else openSource(path);
    });
    // Ctrl+Shift+E is \emph in TeXstudio, so the folder takes VS Code's Ctrl+K Ctrl+O.
    auto *openFolder = add("Open folder…", QKeySequence("Ctrl+K, Ctrl+O"), [this]
    {
        QString directory = QFileDialog::getExistingDirectory(this, "Open folder", m_workspace);
        if (directory.isEmpty()) return;
        openWorkspace(directory);
        QSettings().setValue("workspace", m_workspace);
        addRecent(m_workspace);
    });
    auto *recent = menu->addMenu("Open recent");
    recent->addAction("No recent files")->setEnabled(false);
    connect(recent, &QMenu::aboutToShow, this, [this, recent]
    {
        recent->clear();
        for (const auto &path : m_recent)
            recent->addAction(QDir::toNativeSeparators(path), this, [this, path]
            {
                if (QFileInfo(path).isDir()) { openWorkspace(path); addRecent(path); QSettings().setValue("workspace", m_workspace); }
                else if (QFileInfo(path).suffix().compare("pdf", Qt::CaseInsensitive) == 0) openPdf(path);
                else openSource(path);
            });
        if (m_recent.isEmpty()) recent->addAction("No recent files")->setEnabled(false);
        else { recent->addSeparator(); recent->addAction("Clear recent", this, [this] { m_recent.clear(); }); }
    });
    menu->addSeparator();
    add("Save", QKeySequence::Save, [this]
    {
        if (save(currentEditor()) && m_buildOnSaveAction->isChecked() && !m_runner.running()) build();
    });
    add("Save as…", QKeySequence::SaveAs, [this] { save(currentEditor(), true); });
    add("Close file", QKeySequence::Close, [this] { closeTab(m_tabs->currentIndex()); });
    menu->addSeparator();
    add("Settings…", QKeySequence("Ctrl+,"), [this] { showSettings(); });
    menu = menuBar->addMenu("&Edit");
    add("Undo", QKeySequence::Undo, [this] { if (auto *editor = currentEditor()) editor->undoText(); });
    add("Redo", QKeySequence::Redo, [this] { if (auto *editor = currentEditor()) editor->redoText(); });
    menu->addSeparator();
    add("Find", QKeySequence::Find, [showFind] { showFind(false); });
    add("Replace", QKeySequence("Ctrl+H"), [showFind] { showFind(true); })->setShortcuts({QKeySequence("Ctrl+H"), QKeySequence("Ctrl+R")});
    add("Find next", QKeySequence("F3"), [find] { find(false); });
    add("Find previous", QKeySequence("Shift+F3"), [find] { find(true); });
    add("Go to line…", QKeySequence("Ctrl+G"), [this]
    {
        auto *editor = currentEditor(); if (!editor) return;
        bool ok = false;
        int line = QInputDialog::getInt(this, "Go to line", "Line:", editor->lineNumber(), 1,
            editor->document()->blockCount(), 1, &ok);
        if (ok) editor->goToLine(line);
    });
    menu->addSeparator();
    add("Toggle comment", QKeySequence("Ctrl+/"), [this] { if (currentEditor()) currentEditor()->toggleComment(); });
    menu->addSeparator();
    add("Fold", QKeySequence("Ctrl+Shift+["), [this] { if (auto *editor = currentEditor()) editor->foldAtCursor(); });
    add("Unfold", QKeySequence("Ctrl+Shift+]"), [this]
    {
        auto *editor = currentEditor();
        if (!editor) return;
        // The fold whose first line holds the cursor, else the nearest folded line above.
        for (QTextBlock block = editor->textCursor().block(); block.isValid(); block = block.previous())
            if (editor->isFolded(block.blockNumber())) { editor->toggleFold(block.blockNumber()); return; }
    });
    add("Unfold all", QKeySequence("Ctrl+K, Ctrl+J"), [this] { if (auto *editor = currentEditor()) editor->unfoldAll(); });
    menu = menuBar->addMenu("&Insert");
    add("Bold", QKeySequence("Ctrl+B"), insert("\\textbf{\u2022}"));
    add("Italic", QKeySequence("Ctrl+I"), insert("\\textit{\u2022}"));
    add("Underline", QKeySequence("Ctrl+U"), insert("\\underline{\u2022}"));
    // `…' around the selection, which stays selected, so a second press gives ``…''.
    add("Quotes `\u2026' (twice for ``\u2026'')", QKeySequence("Ctrl+'"), [this]
    {
        auto *editor = currentEditor();
        if (!editor) return;
        auto cursor = editor->textCursor();
        QString inner = cursor.selectedText().replace(QChar::ParagraphSeparator, '\n');
        int start = cursor.selectionStart();
        cursor.insertText("`" + inner + "'");
        cursor.setPosition(start + 1);
        cursor.setPosition(start + 1 + int(inner.size()), QTextCursor::KeepAnchor);
        editor->setTextCursor(cursor);
    });
    add("Emphasis", QKeySequence("Ctrl+Shift+E"), insert("\\emph{\u2022}"));
    add("Typewriter", QKeySequence("Ctrl+Shift+T"), insert("\\texttt{\u2022}"));
    add("Small caps", QKeySequence("Ctrl+Shift+C"), insert("\\textsc{\u2022}"));
    add("Sans serif", QKeySequence("Ctrl+Shift+A"), insert("\\textsf{\u2022}"));
    add("New line \\\\", QKeySequence("Ctrl+Return"), insert("\\\\\n"));
    menu->addSeparator();
    add("Inline math", QKeySequence("Ctrl+Shift+M"), insert("$\u2022$"));
    add("Display math", QKeySequence("Alt+Shift+M"), insert("\\[\n\u2022\n\\]"));
    add("Equation", QKeySequence("Ctrl+Shift+N"), insert("\\begin{equation}\n\u2022\n\\end{equation}"));
    add("Subscript", QKeySequence("Ctrl+Shift+D"), insert("_{\u2022}"));
    add("Superscript", QKeySequence("Ctrl+Shift+U"), insert("^{\u2022}"));
    add("Fraction \\frac", QKeySequence("Alt+Shift+F"), insert("\\frac{\u2022}{\u2022}"));
    add("Fraction \\dfrac", QKeySequence("Ctrl+Shift+F"), insert("\\dfrac{\u2022}{\u2022}"));
    add("Square root", QKeySequence("Ctrl+Shift+Q"), insert("\\sqrt{\u2022}"));
    add("\\left", QKeySequence("Ctrl+Shift+L"), insert("\\left "));
    add("\\right", QKeySequence("Ctrl+Shift+R"), insert("\\right "));
    menu->addSeparator();
    add("Item", QKeySequence("Ctrl+Shift+I"), insert("\\item "));
    add("Environment…", QKeySequence("Ctrl+E"), [this] { if (auto *editor = currentEditor()) editor->insertEnvironment(); });
    add("Close environment", QKeySequence("Alt+Return"), [this] { if (auto *editor = currentEditor()) editor->closeEnvironment(); });
    menu = menuBar->addMenu("&Build");
    add("Build / stop", QKeySequence("F5"), [this] { build(); })->setShortcuts({QKeySequence("F5"), QKeySequence("F6")});
    add("Source → PDF", QKeySequence("F7"), [this] { forwardSync(); });
    add("Next problem", QKeySequence("F8"), [this] { if (!m_problems.isEmpty()) goToProblem((m_problemIndex + 1) % m_problems.size()); });
    add("Previous problem", QKeySequence("Shift+F8"), [this]
    {
        if (!m_problems.isEmpty()) goToProblem((m_problemIndex - 1 + m_problems.size()) % m_problems.size());
    });
    menu->addSeparator();
    m_buildOnSaveAction = add("Build on save", QKeySequence(), [] {});
    m_buildOnSaveAction->setCheckable(true);
    add("Word count", QKeySequence(), [this] { countWords(); });
    menu = menuBar->addMenu("&View");
    add("Toggle side bar", QKeySequence("Ctrl+Shift+O"), [this] { showSide(m_outlineButton->isChecked() ? m_outline : m_files); });
    add("Toggle PDF", QKeySequence("Ctrl+\\"), [this] { m_pdfPane->setVisible(!m_pdfPane->isVisible()); });
    add("Toggle panel", QKeySequence("Ctrl+J"), [this] { m_panel->setVisible(!m_panel->isVisible()); });
    menu->addSeparator();
    m_wrapAction = add("Word wrap", QKeySequence("Alt+Z"), [this](bool on) { setWordWrap(on); });
    m_wrapAction->setCheckable(true);
    // Like VS Code's menuBarVisibility "toggle": hidden, a lone Alt shows it until used.
    m_menuBarAction = add("Menu bar", QKeySequence(), [this](bool on) { setMenuBarShown(on); });
    m_menuBarAction->setCheckable(true);
    m_menuBarAction->setChecked(true);
    // Choosing an item hides a revealed menu bar again; a keyboard shortcut (Ctrl+S) is not a choice.
    connect(menuBar, &QMenuBar::triggered, this, [this]
    {
        if (!m_menuBarShown && !m_viaShortcut) m_topBar->hide();
        m_viaShortcut = false;
    });
    menu->addSeparator();
    add("Zoom in PDF", QKeySequence::ZoomIn, [this] { m_pdf->setZoom(m_pdf->zoom() * 1.25); })->setShortcuts({QKeySequence("Ctrl+="), QKeySequence::ZoomIn});
    add("Zoom out PDF", QKeySequence::ZoomOut, [this] { m_pdf->setZoom(m_pdf->zoom() / 1.25); });
    add("Fit PDF width", QKeySequence("Ctrl+0"), [this] { m_pdf->fitWidth(); });
    add("Present PDF", QKeySequence("Shift+F5"), [this] { present(); }); // PowerPoint's "from current slide"
    menu->addSeparator();
    add("Next tab", QKeySequence("Ctrl+PgDown"), [this] { m_tabs->setCurrentIndex((m_tabs->currentIndex() + 1) % m_tabs->count()); });
    add("Previous tab", QKeySequence("Ctrl+PgUp"), [this] { m_tabs->setCurrentIndex((m_tabs->currentIndex() + m_tabs->count() - 1) % m_tabs->count()); });
    bar->addWidget(menuBar);
    bar->addStretch();
    // Each control sits with what it acts on: Build with the editor tabs, zoom and pages with the PDF.
    // VS Code-style editor action: outlined run triangle, a stop square while building.
    m_runIcon = QIcon(svgPixmap(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.2" stroke-linejoin="round"><path d="M4.5 2.8v10.4L12.8 8z"/></svg>)", "#ADB7C4", 16));
    m_stopIcon = QIcon(svgPixmap(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><rect x="3.5" y="3.5" width="9" height="9" rx="1" fill="currentColor"/></svg>)", "#F48771", 16));
    m_build = new QPushButton;
    m_build->setObjectName("editorAction");
    m_build->setIcon(m_runIcon);
    m_build->setIconSize(QSize(16, 16));
    m_build->setToolTip("Build · F5");
    connect(m_build, &QPushButton::clicked, this, &Window::build);
    m_pdf = new PdfView;
    m_status = new QLabel("Ready");
    // VS Code's problem counters: click to open the Problems panel.
    auto counter = [this](const char *svg, const char *color, const QString &tip)
    {
        auto *button = new QToolButton;
        button->setObjectName("problemCount");
        button->setIcon(QIcon(svgPixmap(svg, color, 14)));
        button->setIconSize(QSize(14, 14));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setToolTip(tip);
        button->hide(); // until the first build
        connect(button, &QToolButton::clicked, this, [this]
        {
            m_panel->setCurrentWidget(m_problemsView);
            m_panel->show();
        });
        statusBar()->addWidget(button);
        return button;
    };
    m_errorCount = counter(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><circle cx="8" cy="8" r="6"/><path d="M5.8 5.8l4.4 4.4m0-4.4l-4.4 4.4"/></svg>)", "#F14C4C", "Errors · F8");
    m_warningCount = counter(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linejoin="round"><path d="M8 2.2l6.2 11H1.8z"/><path d="M8 6.5v3.2m0 1.6v.1"/></svg>)", "#CCA700", "Warnings");
    statusBar()->addWidget(m_status, 1);
    m_engine = new QComboBox;
    m_engine->addItems({"pdfLaTeX", "LuaLaTeX", "XeLaTeX"});
    m_engine->setToolTip("TeX engine");
    m_page = new QSpinBox;
    m_page->setRange(1, 1);
    m_page->setFixedWidth(40);
    m_page->setAlignment(Qt::AlignRight);
    m_page->setToolTip("PDF page");
    connect(m_page, qOverload<int>(&QSpinBox::valueChanged), this, [this](int page) { m_pdf->goToPage(page); });
    m_pageCount = new QLabel("/ 0");
    auto *pdfBar = new QWidget;
    pdfBar->setObjectName("pdfBar");
    pdfBar->setFixedHeight(35);
    auto *pdfBarLayout = new QHBoxLayout(pdfBar);
    pdfBarLayout->setContentsMargins(12, 0, 8, 0);
    pdfBarLayout->setSpacing(2);
    pdfBarLayout->addWidget(m_page);
    pdfBarLayout->addWidget(m_pageCount);
    pdfBarLayout->addStretch();
    auto pdfButton = [&](const QString &text, const QString &tip, auto handler)
    {
        auto *button = new QToolButton;
        button->setText(text);
        button->setToolTip(tip);
        connect(button, &QToolButton::clicked, this, handler);
        pdfBarLayout->addWidget(button);
    };
    pdfButton("−", "Zoom out · Ctrl+-", [this] { m_pdf->setZoom(m_pdf->zoom() / 1.25); });
    pdfButton("+", "Zoom in · Ctrl+=", [this] { m_pdf->setZoom(m_pdf->zoom() * 1.25); });
    pdfButton("Fit width", "Fit PDF width · Ctrl+0", [this] { m_pdf->fitWidth(); });
    m_pdfFindBar = new QWidget;
    m_pdfFindBar->setObjectName("pdfFindBar");
    auto *pdfFindLayout = new QHBoxLayout(m_pdfFindBar);
    pdfFindLayout->setContentsMargins(8, 4, 8, 4);
    pdfFindLayout->setSpacing(4);
    auto *pdfFindClose = new QToolButton;
    pdfFindClose->setText("×");
    pdfFindClose->setToolTip("Close · Escape");
    m_pdfFind = new QLineEdit;
    m_pdfFind->setPlaceholderText("Find in PDF");
    m_pdfFind->setToolTip("Enter: next match · Shift+Enter: previous");
    auto *pdfPrevious = new QToolButton;
    pdfPrevious->setText("↑");
    pdfPrevious->setToolTip("Previous match · Shift+Enter");
    auto *pdfNext = new QToolButton;
    pdfNext->setText("↓");
    pdfNext->setToolTip("Next match · Enter");
    m_pdfCase = new QCheckBox("Case sensitive");
    m_pdfWord = new QCheckBox("Whole words");
    m_pdfMatches = new QLabel;
    auto pdfFind = [this](bool backwards)
    {
        Poppler::Page::SearchFlags flags;
        if (!m_pdfCase->isChecked()) flags |= Poppler::Page::IgnoreCase;
        if (m_pdfWord->isChecked()) flags |= Poppler::Page::WholeWords;
        m_pdf->find(m_pdfFind->text(), backwards, flags);
    };
    connect(m_pdfFind, &QLineEdit::returnPressed, this, [pdfFind] { pdfFind(QGuiApplication::keyboardModifiers() & Qt::ShiftModifier); });
    connect(pdfPrevious, &QToolButton::clicked, this, [pdfFind] { pdfFind(true); });
    connect(pdfNext, &QToolButton::clicked, this, [pdfFind] { pdfFind(false); });
    connect(m_pdfCase, &QCheckBox::toggled, this, [pdfFind] { pdfFind(false); });
    connect(m_pdfWord, &QCheckBox::toggled, this, [pdfFind] { pdfFind(false); });
    connect(pdfFindClose, &QToolButton::clicked, this, [this]
    {
        m_pdfFindBar->hide();
        m_pdf->clearSearch();
        m_pdf->setFocus();
    });
    connect(m_pdf, &PdfView::matchChanged, this, [this](int index, int count)
    {
        m_pdfMatches->setText(count < 0 ? QString() : count == 0 ? "No results" : QString("%1 of %2").arg(index + 1).arg(count));
    });
    auto *pdfEscape = new QShortcut(QKeySequence(Qt::Key_Escape), m_pdfFindBar);
    pdfEscape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(pdfEscape, &QShortcut::activated, pdfFindClose, &QToolButton::click);
    pdfFindLayout->addWidget(pdfFindClose);
    pdfFindLayout->addWidget(m_pdfFind, 1);
    pdfFindLayout->addWidget(pdfPrevious);
    pdfFindLayout->addWidget(pdfNext);
    pdfFindLayout->addWidget(m_pdfCase);
    pdfFindLayout->addWidget(m_pdfWord);
    pdfFindLayout->addWidget(m_pdfMatches);
    m_pdfFindBar->hide();
    m_pdfPane = new QWidget;
    auto *pdfLayout = new QVBoxLayout(m_pdfPane);
    pdfLayout->setContentsMargins(0, 0, 0, 0);
    pdfLayout->setSpacing(0);
    pdfLayout->addWidget(pdfBar);
    pdfLayout->addWidget(m_pdfFindBar);
    pdfLayout->addWidget(m_pdf);
    statusBar()->setSizeGripEnabled(false);
    layout->addWidget(m_topBar);
    m_tabs = new QTabWidget;
    m_tabs->setObjectName("editorTabs");
    m_tabs->setAttribute(Qt::WA_StyledBackground); // otherwise the corner beside the tabs shows the window gap colour
    m_tabs->setDocumentMode(true);
    m_tabs->tabBar()->setDrawBase(false);
    m_tabs->setIconSize(QSize(16, 16));
    // Crowded tabs keep their full names and scroll sideways, like VS Code. Qt's scroll arrows
    // get zero width in the style sheet; the wheel drives them instead (see eventFilter).
    m_tabs->setUsesScrollButtons(true);
    m_tabs->setElideMode(Qt::ElideNone);
    m_tabs->tabBar()->setMouseTracking(true);
    m_tabs->tabBar()->installEventFilter(this);
    m_tabs->setMovable(true);
    // VS Code-style find widget floating over the top-right corner of the editor.
    m_findBar = new QWidget(m_tabs);
    m_findBar->setObjectName("findWidget");
    m_findBar->setAttribute(Qt::WA_StyledBackground);
    auto *findLayout = new QGridLayout(m_findBar);
    findLayout->setContentsMargins(4, 4, 6, 4);
    findLayout->setHorizontalSpacing(4);
    findLayout->setVerticalSpacing(4);
    auto toolButton = [](const QString &text, const QString &tip, bool checkable = false)
    {
        auto *button = new QToolButton;
        button->setText(text);
        button->setToolTip(tip);
        button->setCheckable(checkable);
        return button;
    };
    m_replaceToggle = toolButton("›", "Toggle Replace · Ctrl+H", true);
    m_replaceToggle->setObjectName("replaceToggle");
    m_find = new QLineEdit;
    m_find->setPlaceholderText("Find");
    m_find->setMinimumWidth(120);
    m_findCase = toolButton("Aa", "Match Case", true);
    m_findWord = toolButton("ab", "Match Whole Word", true);
    m_findCount = new QLabel;
    m_findCount->setMinimumWidth(70);
    auto *previous = toolButton("↑", "Previous Match · Shift+F3");
    auto *next = toolButton("↓", "Next Match · F3 or Enter");
    auto *close = toolButton("×", "Close · Escape");
    m_replace = new QLineEdit;
    m_replace->setPlaceholderText("Replace");
    auto *replaceOne = toolButton("Replace", "Replace · Enter");
    auto *replaceAll = toolButton("All", "Replace All");
    auto *findRow = new QHBoxLayout;
    findRow->setSpacing(2);
    for (QWidget *widget : std::initializer_list<QWidget *>{m_findCase, m_findWord, m_findCount, previous, next, close}) findRow->addWidget(widget);
    auto *replaceRow = new QHBoxLayout;
    replaceRow->setSpacing(2);
    replaceRow->addWidget(replaceOne);
    replaceRow->addWidget(replaceAll);
    replaceRow->addStretch();
    m_replaceRow = new QWidget;
    m_replaceRow->setLayout(replaceRow);
    replaceRow->setContentsMargins(0, 0, 0, 0);
    findLayout->addWidget(m_replaceToggle, 0, 0, Qt::AlignTop);
    findLayout->addWidget(m_find, 0, 1);
    findLayout->addLayout(findRow, 0, 2);
    findLayout->addWidget(m_replace, 1, 1);
    findLayout->addWidget(m_replaceRow, 1, 2);
    auto incremental = [this]
    {
        // As in VS Code, typing searches again from the start of the current match.
        auto *editor = currentEditor();
        if (!editor) return;
        auto cursor = editor->textCursor();
        cursor.setPosition(cursor.selectionStart());
        editor->setTextCursor(cursor);
        editor->findText(m_find->text(), findFlags());
        updateFindCount();
    };
    connect(m_find, &QLineEdit::textEdited, this, incremental);
    connect(m_findCase, &QToolButton::toggled, this, incremental);
    connect(m_findWord, &QToolButton::toggled, this, incremental);
    connect(m_find, &QLineEdit::returnPressed, this, [this] { findInEditor(QGuiApplication::keyboardModifiers() & Qt::ShiftModifier); });
    connect(previous, &QToolButton::clicked, this, [this] { findInEditor(true); });
    connect(next, &QToolButton::clicked, this, [this] { findInEditor(false); });
    connect(m_replaceToggle, &QToolButton::toggled, this, [this](bool on)
    {
        m_replaceToggle->setText(on ? "⌄" : "›");
        m_replace->setVisible(on);
        m_replaceRow->setVisible(on);
        placeFind();
    });
    connect(replaceOne, &QToolButton::clicked, this, [this]
    {
        if (auto *editor = currentEditor()) editor->replaceNext(m_find->text(), m_replace->text(), findFlags());
        updateFindCount();
    });
    connect(m_replace, &QLineEdit::returnPressed, replaceOne, &QToolButton::click);
    connect(replaceAll, &QToolButton::clicked, this, [this]
    {
        auto *editor = currentEditor();
        if (!editor) return;
        m_status->setText(QString("Replaced %1 occurrences").arg(editor->replaceAll(m_find->text(), m_replace->text(), findFlags())));
        updateFindCount();
    });
    connect(close, &QToolButton::clicked, this, [this] { m_findBar->hide(); if (auto *editor = currentEditor()) editor->setFocus(); });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), m_findBar);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, close, &QToolButton::click);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] { if (m_findBar->isVisible()) updateFindCount(); });
    m_replaceToggle->setChecked(true);
    m_replaceToggle->setChecked(false);
    m_findBar->hide();
    m_tabs->installEventFilter(this);
    auto *buildCorner = new QWidget;
    buildCorner->setObjectName("buildCorner");
    auto *cornerLayout = new QHBoxLayout(buildCorner);
    // QTabWidget bottom-aligns corner widgets; the bottom margin centers the 24 px controls on the 35 px tabs.
    cornerLayout->setContentsMargins(0, 0, 8, 6);
    cornerLayout->setSpacing(6);
    cornerLayout->addWidget(m_engine);
    cornerLayout->addWidget(m_build);
    m_tabs->setCornerWidget(buildCorner, Qt::TopRightCorner);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] { if (currentEditor()) setTabTitle(currentEditor()); refreshOutline(); updateTabButtons(); });
    m_outline = new QTreeWidget;
    m_outline->setHeaderHidden(true);
    m_outline->setIndentation(16);
    m_outline->setFrameShape(QFrame::NoFrame);
    m_fileModel = new QFileSystemModel(this);
    static FileIconProvider iconProvider;
    m_fileModel->setIconProvider(&iconProvider);
    m_fileModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot); // no QDir::Hidden: hides .folitex-build
    m_fileModel->setNameFilters({"*.tex", "*.bib", "*.sty", "*.cls", "*.pdf"});
    m_fileModel->setNameFilterDisables(false);
    m_files = new QTreeView;
    m_files->setModel(m_fileModel);
    m_files->setHeaderHidden(true);
    for (int column = 1; column < m_fileModel->columnCount(); ++column) m_files->hideColumn(column);
    m_files->setIndentation(16);
    m_files->setFrameShape(QFrame::NoFrame);
    connect(m_files, &QTreeView::clicked, this, [this](const QModelIndex &index)
    {
        if (m_fileModel->isDir(index)) { m_files->setExpanded(index, !m_files->isExpanded(index)); return; }
        QString path = m_fileModel->filePath(index);
        if (path.endsWith(".pdf", Qt::CaseInsensitive)) openPdf(path); else openSource(path);
    });
    // Side bar: VS Code proportions (48 px activity bar, 35 px uppercase title, 22 px rows).
    m_noFolder = new QWidget;
    auto *noFolderLayout = new QVBoxLayout(m_noFolder);
    noFolderLayout->setContentsMargins(20, 12, 20, 12);
    auto *noFolderText = new QLabel("No folder opened.");
    noFolderText->setWordWrap(true);
    auto *openFolderButton = new QPushButton("Open Folder");
    openFolderButton->setObjectName("primary");
    connect(openFolderButton, &QPushButton::clicked, openFolder, &QAction::trigger);
    noFolderLayout->addWidget(noFolderText);
    noFolderLayout->addWidget(openFolderButton);
    noFolderLayout->addStretch();
    m_sideStack = new QStackedWidget;
    m_sideStack->addWidget(m_noFolder);
    // VS Code's root folder row: bold name with a chevron that collapses the tree.
    auto *filesPage = new QWidget;
    auto *filesLayout = new QVBoxLayout(filesPage);
    filesLayout->setContentsMargins(0, 0, 0, 0);
    filesLayout->setSpacing(0);
    m_rootButton = new QToolButton;
    m_rootButton->setObjectName("rootButton");
    m_rootButton->setCheckable(true);
    m_rootButton->setChecked(true);
    m_rootButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_rootButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QIcon chevrons(":/chevron-right.svg");
    chevrons.addFile(":/chevron-down.svg", {}, QIcon::Normal, QIcon::On);
    m_rootButton->setIcon(chevrons);
    m_rootButton->setIconSize(QSize(16, 16));
    connect(m_rootButton, &QToolButton::toggled, m_files, &QWidget::setVisible);
    filesLayout->addWidget(m_rootButton);
    filesLayout->addWidget(m_files, 1);
    filesLayout->addStretch(); // keeps the root row at the top while the tree is collapsed
    m_sideStack->addWidget(filesPage);
    m_sideStack->addWidget(m_outline);
    m_sideTitle = new QLabel;
    m_sideTitle->setObjectName("sideTitle");
    m_side = new QWidget;
    m_side->setObjectName("side");
    auto *sideLayout = new QVBoxLayout(m_side);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);
    sideLayout->addWidget(m_sideTitle);
    sideLayout->addWidget(m_sideStack);
    m_side->hide();
    auto icon = [](const char *svg)
    {
        QIcon result;
        for (auto [color, state] : {std::pair{"#68727F", QIcon::Off}, std::pair{"#D2D7DE", QIcon::On}})
            result.addPixmap(svgPixmap(svg, color, 24), QIcon::Normal, state);
        return result;
    };
    auto *activity = new QWidget;
    activity->setObjectName("activity");
    activity->setFixedWidth(48);
    auto *activityLayout = new QVBoxLayout(activity);
    activityLayout->setContentsMargins(0, 0, 0, 0);
    activityLayout->setSpacing(0);
    auto activityButton = [&](const char *svg, const QString &tip, QWidget *page)
    {
        auto *button = new QToolButton;
        button->setObjectName("activityButton");
        button->setIcon(icon(svg));
        button->setIconSize(QSize(24, 24));
        button->setFixedSize(48, 48);
        button->setCheckable(true);
        button->setToolTip(tip);
        connect(button, &QToolButton::clicked, this, [this, page] { showSide(page); });
        activityLayout->addWidget(button);
        return button;
    };
    m_filesButton = activityButton(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linejoin="round"><path d="M13.5 3H6.5a1.5 1.5 0 0 0-1.5 1.5v15A1.5 1.5 0 0 0 6.5 21h11a1.5 1.5 0 0 0 1.5-1.5V8.5z"/><path d="M13.5 3v5.5H19"/></svg>)",
        "Files · Ctrl+Shift+E", m_files);
    m_outlineButton = activityButton(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"><path d="M4 5.5h16M8 12h12M8 18.5h12M4.5 12h.01M4.5 18.5h.01"/></svg>)",
        "Outline", m_outline);
    activityLayout->addStretch();
    connect(m_outline, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item)
    {
        if (auto *editor = currentEditor()) editor->goToLine(item->data(0, Qt::UserRole).toInt());
    });
    m_outlineTimer.setSingleShot(true);
    m_outlineTimer.setInterval(300);
    connect(&m_outlineTimer, &QTimer::timeout, this, &Window::refreshOutline);
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1500);
    m_log->setFont(QFont("monospace", 10));
    m_problemsView = new QTreeWidget;
    m_problemsView->setObjectName("problems");
    m_problemsView->setHeaderHidden(true);
    m_problemsView->setIndentation(16);
    m_problemsView->setFrameShape(QFrame::NoFrame);
    connect(m_problemsView, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item)
    {
        if (item->data(0, Qt::UserRole).isValid()) goToProblem(item->data(0, Qt::UserRole).toInt());
        else item->setExpanded(!item->isExpanded());
    });
    m_panel = new QTabWidget;
    m_panel->setObjectName("bottomPanel");
    m_panel->setAttribute(Qt::WA_StyledBackground); // its corner (filter, buttons) would show the window gap colour
    m_panel->setDocumentMode(true);
    m_panel->tabBar()->setDrawBase(false);
    m_panel->addTab(m_problemsView, "Problems");
    m_panel->addTab(m_log, "Output");
    m_panel->hide();
    // VS Code's panel header: filter, collapse all, maximize and close on the right of the tabs.
    auto *panelActions = new QWidget;
    auto *panelLayout = new QHBoxLayout(panelActions);
    panelLayout->setContentsMargins(0, 0, 6, 0);
    panelLayout->setSpacing(2);
    auto *filter = new QLineEdit;
    filter->setObjectName("problemFilter");
    filter->setPlaceholderText("Filter (e.g. text, main.tex)");
    filter->setFixedWidth(220);
    filter->setClearButtonEnabled(true);
    connect(filter, &QLineEdit::textChanged, this, [this](const QString &text)
    {
        for (int i = 0; i < m_problemsView->topLevelItemCount(); ++i)
        {
            auto *group = m_problemsView->topLevelItem(i);
            bool fileMatches = group->text(0).contains(text, Qt::CaseInsensitive);
            int shown = 0;
            for (int j = 0; j < group->childCount(); ++j)
            {
                bool match = fileMatches || group->child(j)->text(0).contains(text, Qt::CaseInsensitive);
                group->child(j)->setHidden(!match);
                shown += match;
            }
            group->setHidden(group->childCount() && !shown);
        }
    });
    auto panelButton = [&](const char *svg, const QString &tip)
    {
        auto *button = new QToolButton;
        button->setObjectName("panelAction");
        button->setIcon(QIcon(svgPixmap(svg, "#ADB7C4", 16)));
        button->setToolTip(tip);
        panelLayout->addWidget(button);
        return button;
    };
    panelLayout->addWidget(filter);
    auto *collapse = panelButton(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.2"><rect x="2.5" y="2.5" width="11" height="11" rx="1.5"/><path d="M5.5 8h5"/></svg>)", "Collapse All");
    connect(collapse, &QToolButton::clicked, m_problemsView, &QTreeWidget::collapseAll);
    auto *maximize = panelButton(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.2" stroke-linecap="round"><path d="M4 10l4-4 4 4"/></svg>)", "Maximize Panel Size");
    connect(maximize, &QToolButton::clicked, this, [this, maximize]
    {
        m_tabs->setVisible(!m_tabs->isVisible());
        maximize->setToolTip(m_tabs->isVisible() ? "Maximize Panel Size" : "Restore Panel Size");
    });
    auto *closePanel = panelButton(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M4.5 4.5l7 7m0-7l-7 7" stroke="currentColor" stroke-width="1.2" stroke-linecap="round" fill="none"/></svg>)", "Close Panel · Ctrl+J");
    connect(closePanel, &QToolButton::clicked, this, [this] { m_panel->hide(); m_tabs->show(); });
    m_panel->setCornerWidget(panelActions, Qt::TopRightCorner);
    setProblems({}, {}); // shows the empty-state line
    // The build log opens under the editor only, like VS Code's panel inside the editor column.
    m_vertical = new QSplitter(Qt::Vertical);
    m_vertical->addWidget(m_tabs);
    m_vertical->addWidget(m_panel);
    m_vertical->setStretchFactor(0, 1);
    m_vertical->setStretchFactor(1, 0);
    m_vertical->setChildrenCollapsible(false);
    m_horizontal = new QSplitter;
    m_horizontal->addWidget(m_side);
    m_horizontal->addWidget(m_vertical);
    m_horizontal->addWidget(m_pdfPane);
    m_horizontal->setSizes({200, 540, 540});
    m_horizontal->setStretchFactor(0, 0);
    m_horizontal->setStretchFactor(1, 1);
    m_horizontal->setStretchFactor(2, 1);
    m_horizontal->setChildrenCollapsible(false);
    for (auto *splitter : {m_horizontal, m_vertical}) splitter->setHandleWidth(6);
    for (QWidget *panel : std::initializer_list<QWidget *>{m_side, m_tabs, m_panel, m_pdfPane}) new RoundedCorners(panel);
    auto *row = new QHBoxLayout;
    row->setSpacing(0);
    row->addWidget(activity);
    row->setContentsMargins(0, 6, 6, 6);
    row->addWidget(m_horizontal);
    layout->addLayout(row);
    setCentralWidget(central);
    qApp->installEventFilter(this); // last: eventFilter uses the widgets built above
    connect(&m_runner, &BuildRunner::output, this, [this](const QString &text)
    {
        auto cursor = m_log->textCursor(); cursor.movePosition(QTextCursor::End); cursor.insertText(text);
        m_log->setTextCursor(cursor);
    });
    connect(&m_runner, &BuildRunner::finished, this, [this](bool success, const QString &pdf)
    {
        m_build->setIcon(m_runIcon);
        m_build->setToolTip("Build · F5");
        if (m_runner.cancelled()) { m_status->setText("Build stopped"); return; }
        // Always the latest log: when nothing changed, latexmk does not rerun and keeps the old
        // one, which still describes the document.
        QList<Problem> problems;
        QString directory = QFileInfo(QFileInfo(pdf).absolutePath()).absolutePath();
        QFile log(QFileInfo(pdf).absolutePath() + '/' + QFileInfo(pdf).completeBaseName() + ".log");
        if (!pdf.isEmpty() && log.open(QIODevice::ReadOnly))
            problems = BuildRunner::parseLog(QString::fromUtf8(log.readAll()), directory);
        setProblems(problems, directory);
        m_errorCount->show();
        m_warningCount->show();
        int errors = int(std::count_if(problems.cbegin(), problems.cend(), [](const Problem &p) { return p.error; }));
        int warnings = int(problems.size()) - errors;
        if (success)
            m_status->setText(warnings ? QString("Build succeeded · %1 warning%2").arg(warnings).arg(warnings == 1 ? "" : "s") : "Build succeeded");
        else
            m_status->setText(errors ? QString("Build failed: %1 error%2 · F8 to go to the first").arg(errors).arg(errors == 1 ? "" : "s")
                                     : "Build failed — see Output");
        // nonstopmode still writes a PDF after most errors; show it so the edits are visible.
        if (success || (QFileInfo::exists(pdf) && QFileInfo(pdf).lastModified() >= m_buildStarted.addSecs(-1))) openPdf(pdf);
        if (!success)
        {
            m_panel->setCurrentWidget(errors ? static_cast<QWidget *>(m_problemsView) : m_log);
            m_panel->show();
            m_vertical->setSizes({580, 170});
        }
    });
    connect(m_pdf, &PdfView::documentReady, this, [this](int pages)
    {
        m_page->setMaximum(std::max(1, pages));
        m_pageCount->setText("/ " + QString::number(pages));
        if (m_pendingPage) { m_pdf->goToPage(m_pendingPage, m_pendingY); m_pendingPage = 0; }
    });
    connect(m_pdf, &PdfView::loadFailed, this, [this](const QString &error) { m_status->setText(error); });
    connect(m_pdf, &PdfView::pageChanged, this, [this](int page)
    {
        QSignalBlocker blocker(m_page); m_page->setValue(page);
    });
    connect(m_pdf, &PdfView::sourceRequested, this, &Window::inverseSync);
    // External edits (e.g. an AI agent) reload silently; tools that write in bursts are coalesced.
    m_sourceReload.setSingleShot(true); m_sourceReload.setInterval(200);
    connect(&m_sourceWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path)
    {
        m_changedSources.insert(path);
        m_sourceReload.start();
    });
    connect(&m_sourceReload, &QTimer::timeout, this, [this]
    {
        const auto paths = std::exchange(m_changedSources, {});
        for (int i = 0; i < m_tabs->count(); ++i)
            if (auto *editor = qobject_cast<Editor *>(m_tabs->widget(i)); editor && paths.contains(editor->property("path").toString()))
                reloadSource(editor);
    });
    m_reload.setSingleShot(true); m_reload.setInterval(250);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_reload, qOverload<>(&QTimer::start));
    connect(&m_reload, &QTimer::timeout, this, [this]
    {
        if (!QFileInfo::exists(m_pdf->path()) || m_runner.running()) return;
        if (!m_watcher.files().contains(m_pdf->path())) m_watcher.addPath(m_pdf->path());
        m_pdf->open(m_pdf->path());
    });
    newDocument();
}

Editor *Window::currentEditor() const { return qobject_cast<Editor *>(m_tabs->currentWidget()); }

// Decodes a source file; empty error means success. Shared by open and external-change reload.
static QString decodeSource(const QByteArray &bytes, QString &text)
{
    QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    text = decoder(bytes);
    if (decoder.hasError()) return "Only UTF-8 sources are supported; convert the encoding first.";
    if (text.contains(QChar::LineSeparator) || text.contains(QChar::ParagraphSeparator))
        return "Source contains Unicode line separators U+2028/U+2029; convert them to LF or CRLF first.";
    return {};
}

static QString lineEndingOf(const QByteArray &bytes)
{
    return bytes.contains("\r\n") ? "\r\n" : !bytes.contains("\n") && bytes.contains("\r") ? "\r" : "\n";
}

void Window::setWordWrap(bool on)
{
    m_wordWrap = on;
    m_wrapAction->setChecked(on);
    for (int i = 0; i < m_tabs->count(); ++i)
        if (auto *editor = qobject_cast<Editor *>(m_tabs->widget(i))) editor->setWordWrap(on);
}

void Window::watchSource(const QString &path)
{
    // Atomic writers replace the inode, which drops the watch, so it is re-added after every change.
    if (QFileInfo::exists(path) && !m_sourceWatcher.files().contains(path)) m_sourceWatcher.addPath(path);
}

void Window::reloadSource(Editor *editor)
{
    QString path = editor->property("path").toString(), name = QFileInfo(path).fileName();
    watchSource(path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { m_status->setText(name + " was removed or renamed on disk."); return; }
    QByteArray bytes = file.readAll();
    QString text, error = decodeSource(bytes, text);
    if (!error.isEmpty()) { m_status->setText(name + " changed on disk: " + error); return; }
    text.replace("\r\n", "\n").replace('\r', '\n');
    QString current = editor->document()->toRawText();
    current.replace(QChar::ParagraphSeparator, QChar::LineFeed);
    if (text == current)
    {
        editor->setProperty("mtime", QFileInfo(path).lastModified());
        return;
    }
    if (editor->document()->isModified())
    {
        // Never discard unsaved edits; saving still asks before overwriting the newer file.
        m_status->setText(name + " changed on disk, but has unsaved edits here; not reloaded.");
        return;
    }
    // Replace only the changed middle so the cursor, scroll position and undo history stay useful.
    qsizetype prefix = 0, suffix = 0;
    qsizetype limit = std::min(text.size(), current.size());
    while (prefix < limit && text[prefix] == current[prefix]) ++prefix;
    while (suffix < limit - prefix && text[text.size() - 1 - suffix] == current[current.size() - 1 - suffix]) ++suffix;
    QTextCursor cursor(editor->document());
    cursor.setPosition(prefix);
    cursor.setPosition(current.size() - suffix, QTextCursor::KeepAnchor);
    cursor.insertText(text.mid(prefix, text.size() - prefix - suffix));
    editor->setProperty("bom", bytes.startsWith("\xef\xbb\xbf"));
    editor->setProperty("lineEnding", lineEndingOf(bytes));
    editor->setProperty("mtime", QFileInfo(path).lastModified());
    editor->document()->setModified(false);
    setTabTitle(editor);
    m_status->setText("Reloaded " + name + " (changed on disk) · Ctrl+Z to undo");
}

// shortcut: the outline covers the current tab only, upgrade by following \input/\include from the root file.
void Window::refreshOutline()
{
    m_outline->clear();
    auto *editor = currentEditor();
    if (!editor) return;
    QList<QPair<int, QTreeWidgetItem *>> parents;
    for (const auto &entry : Editor::outline(editor->toPlainText()))
    {
        if (entry.kind == OutlineEntry::Section)
            while (!parents.isEmpty() && parents.last().first >= entry.level) parents.removeLast();
        auto *item = parents.isEmpty() ? new QTreeWidgetItem(m_outline) : new QTreeWidgetItem(parents.last().second);
        static const QStringList prefixes = {"", "label  ", "file  ", "TODO  "};
        static const QStringList colors = {"#ADB7C4", "#4FC1FF", "#C586C0", "#CCA700"};
        item->setText(0, prefixes[entry.kind] + (entry.title.isEmpty() ? "…" : entry.title));
        item->setForeground(0, themeColor(colors[entry.kind]));
        item->setData(0, Qt::UserRole, entry.line);
        item->setData(0, Qt::UserRole + 1, entry.kind);
        item->setToolTip(0, QString("Line %1").arg(entry.line));
        if (entry.kind == OutlineEntry::Section) parents.append({entry.level, item});
    }
    m_outline->expandAll();
    syncOutline();
}

void Window::openWorkspace(const QString &directory)
{
    QFileInfo info(directory);
    if (!info.isDir()) return;
    m_workspace = QDir::cleanPath(info.absoluteFilePath()); // no trailing slash, so the folder has a name
    m_files->setRootIndex(m_fileModel->setRootPath(m_workspace));
    m_rootButton->setText(QFileInfo(m_workspace).fileName());
    m_rootButton->setToolTip(m_workspace);
    showSide(m_files, false);
    if (auto *editor = currentEditor()) setTabTitle(editor);
}

// Like VS Code's activity bar: clicking the active view's button closes the side bar.
void Window::showSide(QWidget *page, bool toggle)
{
    QWidget *shown = page != m_files ? page : m_workspace.isEmpty() ? m_noFolder : m_files->parentWidget();
    bool close = toggle && m_side->isVisible() && m_sideStack->currentWidget() == shown;
    m_side->setVisible(!close);
    m_sideStack->setCurrentWidget(shown);
    m_sideTitle->setText(page == m_outline ? "Outline" : "Explorer");
    m_filesButton->setChecked(!close && page == m_files);
    m_outlineButton->setChecked(!close && page == m_outline);
}

void Window::syncOutline()
{
    auto *editor = currentEditor();
    if (!editor) return;
    int line = editor->lineNumber();
    QTreeWidgetItem *best = nullptr;
    // The last section starting at or above the cursor line is where the cursor is, as in TeXstudio's structure view.
    for (QTreeWidgetItemIterator it(m_outline); *it; ++it)
        if ((*it)->data(0, Qt::UserRole + 1) == OutlineEntry::Section && (*it)->data(0, Qt::UserRole).toInt() <= line) best = *it;
    if (best && best != m_outline->currentItem()) m_outline->setCurrentItem(best);
}

Editor *Window::newDocument(const QString &text)
{
    auto *editor = new Editor;
    editor->setPlainText(text);
    editor->document()->setModified(false);
    int index = m_tabs->addTab(editor, "Untitled");
    // × and the file icon share one left widget, so their spacing is ours, not QTabBar's.
    auto *lead = new QWidget;
    auto *leadLayout = new QHBoxLayout(lead);
    leadLayout->setContentsMargins(6, 0, 0, 0);
    leadLayout->setSpacing(4);
    auto *close = new QToolButton;
    close->setObjectName("tabClose");
    close->setIconSize(QSize(12, 12));
    close->setAutoRaise(true);
    close->setToolTip("Close · Ctrl+W");
    connect(close, &QToolButton::clicked, this, [this, editor] { closeTab(m_tabs->indexOf(editor)); });
    auto *icon = new QLabel;
    icon->setAttribute(Qt::WA_TransparentForMouseEvents); // clicks select the tab
    leadLayout->addWidget(close);
    leadLayout->addWidget(icon);
    icon->setFixedSize(16, 16);
    lead->setFixedSize(lead->sizeHint()); // QTabBar would squeeze it otherwise
    m_tabs->tabBar()->setTabButton(index, QTabBar::LeftSide, lead); // left, as tabActionLocation: left
    m_tabs->setCurrentWidget(editor);
    setTabTitle(editor); // icon and × now that the left widget exists
    connect(editor->document(), &QTextDocument::modificationChanged, this, [this, editor] { setTabTitle(editor); });
    editor->setWordWrap(m_wordWrap);
    if (m_editorFont != QFont()) editor->setEditorFont(m_editorFont);
    connect(editor, &Editor::wordWrapToggled, this, &Window::setWordWrap);
    connect(editor, &Editor::fontZoomRequested, this, [this, editor](int steps)
    {
        QFont font = editor->font();
        font.setPointSizeF(std::clamp(font.pointSizeF() + steps, 6.0, 40.0));
        setEditorFont(font);
    });
    connect(editor, &QTextEdit::textChanged, this, [this, editor] { if (editor == currentEditor()) m_outlineTimer.start(); });
    connect(editor, &QTextEdit::cursorPositionChanged, this, [this, editor]
    {
        if (editor == currentEditor()) syncOutline();
        if (editor == currentEditor() && !m_runner.running())
            m_status->setText(QString("Ln %1, Col %2").arg(editor->lineNumber()).arg(editor->textCursor().positionInBlock() + 1));
    });
    editor->setFocus();
    return editor;
}

void Window::setTabTitle(Editor *editor)
{
    QString path = editor->property("path").toString();
    QString name = path.isEmpty() ? "Untitled" : QFileInfo(path).fileName();
    int index = m_tabs->indexOf(editor);
    if (index >= 0)
    {
        m_tabs->setTabText(index, name);
        // addTab() reports the new tab as current before its left widget exists.
        if (QWidget *lead = m_tabs->tabBar()->tabButton(index, QTabBar::LeftSide))
            lead->findChild<QLabel *>()->setPixmap(fileIcon(path.isEmpty() ? "Untitled.tex" : name).pixmap(16, 16));
        m_tabs->setTabToolTip(index, path);
    }
    updateTabButtons();
    updateTabHints();
    if (editor != currentEditor()) return;
    setWindowTitle((editor->document()->isModified() ? "● " : "") + name + (m_workspace.isEmpty() ? "" : " — " + QFileInfo(m_workspace).fileName()) + " — FoliTeX");
    if (!m_workspace.isEmpty() && !path.isEmpty()) m_files->setCurrentIndex(m_fileModel->index(path));
}

bool Window::openSource(const QString &path)
{
    QFileInfo info(path);
    QString absolute = info.absoluteFilePath();
    for (int i = 0; i < m_tabs->count(); ++i)
        if (m_tabs->widget(i)->property("path").toString() == absolute)
        { m_tabs->setCurrentIndex(i); addRecent(absolute); return true; }
    QFile file(absolute);
    if (!file.open(QIODevice::ReadOnly)) { m_status->setText("Cannot open file: " + file.errorString()); return false; }
    QByteArray bytes = file.readAll();
    QString text, error = decodeSource(bytes, text);
    if (!error.isEmpty()) { m_status->setText(error); return false; }
    auto *editor = currentEditor();
    if (!editor || !editor->toPlainText().isEmpty() || !editor->property("path").toString().isEmpty() || editor->document()->isModified())
        editor = newDocument();
    editor->setPlainText(text);
    editor->setProperty("bom", bytes.startsWith("\xef\xbb\xbf"));
    editor->setProperty("lineEnding", lineEndingOf(bytes));
    editor->setProperty("path", absolute);
    editor->setProperty("mtime", info.lastModified());
    editor->document()->setModified(false);
    setTabTitle(editor);
    applyProblems(editor);
    watchSource(absolute);
    addRecent(absolute);
    return true;
}

void Window::setEditorFont(const QFont &font)
{
    m_editorFont = font;
    for (int i = 0; i < m_tabs->count(); ++i)
        if (auto *editor = qobject_cast<Editor *>(m_tabs->widget(i))) editor->setEditorFont(font);
}

QString Window::templatesFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/templates";
}

QList<QPair<QString, QString>> Window::builtinTemplates()
{
    return {
        {"Article", "\\documentclass[11pt,a4paper]{article}\n\\usepackage[T1]{fontenc}\n\\usepackage{amsmath,amssymb}\n"
            "\\usepackage{graphicx}\n\\usepackage{hyperref}\n\n\\title{\u2022}\n\\author{\u2022}\n\\date{\\today}\n\n"
            "\\begin{document}\n\\maketitle\n\n\\section{\u2022}\n\u2022\n\n\\end{document}\n"},
        {"Article (Vietnamese)", "\\documentclass[12pt,a4paper]{article}\n\\usepackage[T5]{fontenc}\n\\usepackage[vietnamese]{babel}\n"
            "\\usepackage{amsmath,amssymb}\n\\usepackage{graphicx}\n\\usepackage{hyperref}\n\n\\title{\u2022}\n\\author{\u2022}\n"
            "\\date{\\today}\n\n\\begin{document}\n\\maketitle\n\n\\section{\u2022}\n\u2022\n\n\\end{document}\n"},
        {"Report", "\\documentclass[12pt,a4paper]{report}\n\\usepackage[T1]{fontenc}\n\\usepackage{amsmath,amssymb}\n"
            "\\usepackage{graphicx}\n\\usepackage{hyperref}\n\n\\title{\u2022}\n\\author{\u2022}\n\\date{\\today}\n\n"
            "\\begin{document}\n\\maketitle\n\\tableofcontents\n\n\\chapter{\u2022}\n\u2022\n\n\\end{document}\n"},
        {"Beamer presentation", "\\documentclass{beamer}\n\\usetheme{Madrid}\n\\usepackage{amsmath,amssymb}\n\\usepackage{graphicx}\n\n"
            "\\title{\u2022}\n\\author{\u2022}\n\\date{\\today}\n\n\\begin{document}\n\n\\begin{frame}\n    \\titlepage\n\\end{frame}\n\n"
            "\\begin{frame}{\u2022}\n    \\begin{itemize}\n        \\item \u2022\n    \\end{itemize}\n\\end{frame}\n\n\\end{document}\n"},
        {"Letter", "\\documentclass[11pt,a4paper]{letter}\n\\usepackage[T1]{fontenc}\n\n\\signature{\u2022}\n\\address{\u2022}\n\n"
            "\\begin{document}\n\\begin{letter}{\u2022}\n\\opening{Dear \u2022,}\n\n\u2022\n\n\\closing{Sincerely,}\n\\end{letter}\n\\end{document}\n"},
    };
}

Editor *Window::newFromTemplate(const QString &text)
{
    auto *editor = newDocument();
    editor->insertTemplate(text); // the first placeholder is selected, Tab goes to the next
    editor->setFocus();
    return editor;
}

void Window::showSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Settings");
    auto *form = new QFormLayout(&dialog);
    auto *family = new QFontComboBox;
    family->setFontFilters(QFontComboBox::MonospacedFonts);
    family->setCurrentFont(currentEditor() ? currentEditor()->font() : font());
    auto *size = new QSpinBox;
    size->setRange(6, 40);
    size->setSuffix(" pt");
    size->setValue(qRound(currentEditor() ? currentEditor()->font().pointSizeF() : 11));
    auto *fontRow = new QHBoxLayout;
    fontRow->addWidget(family, 1);
    fontRow->addWidget(size);
    form->addRow("Editor font", fontRow);
    auto *theme = new QComboBox;
    theme->addItems({"Dark", "Light"});
    theme->setCurrentIndex(lightTheme() ? 1 : 0);
    auto *restart = new QLabel("Restart FoliTeX to apply the theme.");
    restart->setVisible(false);
    connect(theme, &QComboBox::currentIndexChanged, restart, [restart, theme] { restart->setVisible((theme->currentIndex() == 1) != lightTheme()); });
    form->addRow("Theme", theme);
    form->addRow("", restart);
    auto *engine = new QComboBox;
    for (int i = 0; i < m_engine->count(); ++i) engine->addItem(m_engine->itemText(i));
    engine->setCurrentText(m_engine->currentText());
    form->addRow("Engine", engine);
    auto *buildOnSave = new QCheckBox("Build after each Ctrl+S");
    buildOnSave->setChecked(m_buildOnSaveAction->isChecked());
    form->addRow("Build", buildOnSave);
    auto *wrap = new QCheckBox("Wrap long lines (Alt+Z)");
    wrap->setChecked(m_wordWrap);
    form->addRow("Word wrap", wrap);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted) return;
    QFont chosen = family->currentFont();
    chosen.setStyleHint(QFont::Monospace);
    chosen.setPointSizeF(size->value());
    setEditorFont(chosen);
    m_engine->setCurrentText(engine->currentText());
    m_buildOnSaveAction->setChecked(buildOnSave->isChecked());
    setWordWrap(wrap->isChecked());
    // Read at start-up, before any widget exists; the other settings are saved on quit (main.cpp).
    QSettings().setValue("theme", theme->currentIndex() == 1 ? "light" : "dark");
}

void Window::addRecent(const QString &path)
{
    m_recent.removeAll(path);
    m_recent.prepend(path);
    while (m_recent.size() > 12) m_recent.removeLast();
}

QVariantMap Window::session() const
{
    QStringList files;
    QVariantList positions;
    int current = -1;
    for (int i = 0; i < m_tabs->count(); ++i)
    {
        auto *editor = qobject_cast<Editor *>(m_tabs->widget(i));
        if (!editor || editor->property("path").toString().isEmpty()) continue;
        if (i == m_tabs->currentIndex()) current = files.size();
        files << editor->property("path").toString();
        positions << editor->textCursor().position();
    }
    return {{"files", files}, {"positions", positions}, {"current", current}, {"pdf", m_pdf->path()}};
}

void Window::restoreSession(const QVariantMap &session)
{
    QStringList files = session.value("files").toStringList();
    QVariantList positions = session.value("positions").toList();
    QList<int> positionsByTab;
    Editor *current = nullptr;
    for (int i = 0; i < files.size(); ++i)
    {
        if (!QFileInfo(files[i]).isFile() || !openSource(files[i])) continue;
        auto *editor = currentEditor();
        QTextCursor cursor(editor->document());
        cursor.setPosition(std::clamp(positions.value(i).toInt(), 0, editor->document()->characterCount() - 1));
        editor->setTextCursor(cursor);
        if (i == session.value("current").toInt()) current = editor;
    }
    if (current) m_tabs->setCurrentWidget(current);
    if (auto *editor = currentEditor()) editor->ensureCursorVisible();
    QString pdf = session.value("pdf").toString();
    if (QFileInfo(pdf).isFile()) openPdf(pdf);
}

PdfView *Window::present()
{
    if (m_pdf->path().isEmpty() || !m_pdf->pageCount()) { m_status->setText("Open or build a PDF to present it."); return nullptr; }
    // A second view with its own renderer, so the editor's PDF keeps its position and cache.
    // A child window, so it closes with the main window instead of keeping the app running.
    auto *show = new PdfView(this);
    show->setWindowFlag(Qt::Window);
    show->setAttribute(Qt::WA_DeleteOnClose);
    show->setWindowTitle(QFileInfo(m_pdf->path()).fileName());
    show->setPresentation(true);
    show->goToPage(m_pdf->currentPage());
    show->open(m_pdf->path());
    show->showFullScreen();
    return show;
}

void Window::countWords()
{
    auto *editor = currentEditor();
    QString path = editor ? editor->property("path").toString() : QString();
    if (path.isEmpty()) { m_status->setText("Save the document to count its words."); return; }
    QString root = BuildRunner::rootFile(path);
    auto *process = new QProcess(this);
    process->setWorkingDirectory(QFileInfo(root).absolutePath());
    connect(process, &QProcess::finished, this, [this, process, root](int code)
    {
        QString report = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        process->deleteLater();
        if (code != 0 || report.isEmpty()) { m_status->setText("texcount failed; is it installed?"); return; }
        QMessageBox::information(this, "Word count — " + QFileInfo(root).fileName(), report);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error)
    {
        if (error == QProcess::FailedToStart) { m_status->setText("Cannot start texcount; install it with TeX Live."); process->deleteLater(); }
    });
    // -inc follows \input/\include from the root, like the build.
    process->start("texcount", {"-inc", "-utf8", "-total", QFileInfo(root).fileName()});
}

bool Window::save(Editor *editor, bool saveAs)
{
    if (!editor) return false;
    QString current = editor->property("path").toString();
    QString path = current;
    if (saveAs || path.isEmpty())
        path = QFileDialog::getSaveFileName(this, "Save TeX", current.isEmpty() ? m_workspace : current, "TeX (*.tex);;All files (*)");
    if (path.isEmpty()) return false;
    if (path == current && QFileInfo::exists(path) && QFileInfo(path).lastModified() != editor->property("mtime").toDateTime())
        if (QMessageBox::question(this, "File changed", "The file was modified on disk. Overwrite it with the open contents?") != QMessageBox::Yes) return false;
    QString text = editor->document()->toRawText();
    text.replace(QChar::ParagraphSeparator, QChar::LineFeed);
    text.replace(QChar::LineSeparator, QChar::LineFeed);
    QString ending = editor->property("lineEnding").toString();
    if (!ending.isEmpty() && ending != "\n") text.replace("\n", ending);
    QByteArray bytes = text.toUtf8();
    if (editor->property("bom").toBool()) bytes.prepend("\xef\xbb\xbf");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    { QMessageBox::warning(this, "Cannot save", file.errorString()); return false; }
    editor->setProperty("path", QFileInfo(path).absoluteFilePath());
    editor->setProperty("mtime", QFileInfo(path).lastModified());
    editor->document()->setModified(false);
    setTabTitle(editor);
    if (!current.isEmpty() && current != editor->property("path").toString()) m_sourceWatcher.removePath(current);
    watchSource(editor->property("path").toString());
    return true;
}

bool Window::mayClose(Editor *editor)
{
    if (!editor || !editor->document()->isModified()) return true;
    auto answer = QMessageBox::question(this, "Unsaved changes", "Save changes before closing?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel) return false;
    return answer == QMessageBox::Discard || save(editor);
}

void Window::closeTab(int index)
{
    auto *editor = qobject_cast<Editor *>(m_tabs->widget(index));
    if (!editor || !mayClose(editor)) return;
    m_tabs->removeTab(index); editor->deleteLater();
    QString path = editor->property("path").toString();
    if (!path.isEmpty()) m_sourceWatcher.removePath(path);
    if (!m_tabs->count()) newDocument();
    updateTabHints();
}

void Window::closeEvent(QCloseEvent *event)
{
    if (m_runner.running())
    {
        m_status->setText("Stop the build before closing.");
        event->ignore();
        return;
    }
    for (int i = 0; i < m_tabs->count(); ++i)
        if (!mayClose(qobject_cast<Editor *>(m_tabs->widget(i)))) { event->ignore(); return; }
    event->accept();
}

void Window::build()
{
    if (m_runner.running()) { m_runner.stop(); return; }
    auto *editor = currentEditor();
    if (!editor) return;
    if ((editor->property("path").toString().isEmpty() || editor->document()->isModified()) && !save(editor)) return;
    QString source = editor->property("path").toString();
    QString root = BuildRunner::rootFile(source);
    // A .bib, .sty or other non-TeX file builds the document built last, never itself.
    if (!root.isEmpty() && !QStringList{"tex", "ltx"}.contains(QFileInfo(root).suffix().toLower()))
    {
        if (m_lastRoot.isEmpty()) { m_status->setText("Open the main .tex file and press F5 to build."); return; }
        source = root = m_lastRoot;
    }
    // Only save other documents explicitly associated with the same TeX root.
    for (int i = 0; i < m_tabs->count(); ++i)
    {
        auto *other = qobject_cast<Editor *>(m_tabs->widget(i));
        if (!other || other == editor || !other->document()->isModified()) continue;
        QString path = other->property("path").toString();
        if (!path.isEmpty() && !root.isEmpty() && BuildRunner::rootFile(path) == root && !save(other)) return;
    }
    m_log->clear();
    m_buildStarted = QDateTime::currentDateTime();
    m_status->setText("Building…"); m_build->setIcon(m_stopIcon); m_build->setToolTip("Stop build · F5");
    m_lastRoot = root;
    m_runner.start(source, m_engine->currentText());
}

void Window::openPdf(const QString &path)
{
    if (!m_watcher.files().isEmpty()) m_watcher.removePaths(m_watcher.files());
    m_watcher.addPath(path);
    m_pdf->show(); m_pdf->open(path);
}

void Window::syncProcess(QStringList arguments, std::function<void(QString)> complete)
{
    auto *process = new QProcess(this);
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this, process, complete](int code, QProcess::ExitStatus status)
        {
            QString output = QString::fromUtf8(process->readAllStandardOutput());
            if (code == 0 && status == QProcess::NormalExit) complete(output);
            else m_status->setText("SyncTeX found no position; rebuild with synctex=1.");
            process->deleteLater();
        });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error)
    {
        if (error == QProcess::FailedToStart) { m_status->setText("Cannot start synctex."); process->deleteLater(); }
    });
    process->start("synctex", arguments);
}

void Window::forwardSync()
{
    auto *editor = currentEditor();
    if (!editor || editor->property("path").toString().isEmpty() || m_pdf->path().isEmpty()) return;
    if (editor->document()->isModified()) { m_status->setText("Build first to sync new edits with the PDF."); return; }
    QString input = QString::number(editor->lineNumber()) + ":0:" + editor->property("path").toString();
    syncProcess({"view", "-i", input, "-o", m_pdf->path()}, [this](const QString &text)
    {
        auto page = QRegularExpression(R"(Page:(\d+))").match(text);
        auto y = QRegularExpression(R"(y:([\d.eE+\-]+))").match(text);
        if (page.hasMatch()) m_pdf->goToPage(page.captured(1).toInt(), y.captured(1).toDouble());
    });
}

void Window::inverseSync(int page, double x, double y, const QStringList &words, int clicked)
{
    QString position = QString::number(page) + ':' + QString::number(x) + ':' + QString::number(y) + ':' + m_pdf->path();
    syncProcess({"edit", "-o", position}, [this, words, clicked](const QString &text)
    {
        auto input = QRegularExpression(R"(^Input:(.+)$)", QRegularExpression::MultilineOption).match(text);
        auto line = QRegularExpression(R"(^Line:(\d+))", QRegularExpression::MultilineOption).match(text);
        if (!input.hasMatch() || !line.hasMatch()) return;
        QString path = input.captured(1).trimmed();
        if (QFileInfo(path).isRelative()) path = QDir(QFileInfo(m_pdf->path()).absolutePath()).absoluteFilePath(path);
        if (openSource(QDir::cleanPath(path))) currentEditor()->goToWord(line.captured(1).toInt(), words, clicked);
    });
}

QTextDocument::FindFlags Window::findFlags() const
{
    QTextDocument::FindFlags flags;
    if (m_findCase->isChecked()) flags |= QTextDocument::FindCaseSensitively;
    if (m_findWord->isChecked()) flags |= QTextDocument::FindWholeWords;
    return flags;
}

void Window::findInEditor(bool backwards)
{
    auto *editor = currentEditor();
    if (!editor) return;
    editor->findText(m_find->text(), findFlags() | (backwards ? QTextDocument::FindBackward : QTextDocument::FindFlags{}));
    updateFindCount();
}

void Window::updateFindCount()
{
    auto *editor = currentEditor();
    int current = -1;
    int count = editor ? editor->countMatches(m_find->text(), findFlags(), &current) : 0;
    m_findCount->setText(m_find->text().isEmpty() ? QString() : count == 0 ? "No results"
        : QString("%1 of %2").arg(current < 0 ? QString("?") : QString::number(current + 1)).arg(count));
}

void Window::placeFind()
{
    if (!m_findBar->isVisible()) return;
    // About 480 px like VS Code, narrower when the editor is; the find field absorbs the difference.
    int width = std::min(std::max(m_findBar->sizeHint().width(), 480), m_tabs->width() - 22);
    m_findBar->resize(std::max(width, m_findBar->minimumSizeHint().width()), m_findBar->sizeHint().height());
    // Left of the editor's vertical scroll bar, just below the tabs.
    m_findBar->move(std::max(0, m_tabs->width() - m_findBar->width() - 18), m_tabs->tabBar()->height());
    m_findBar->raise();
}

// VS Code: × on the active or hovered tab, ● for unsaved changes (× when the dot is hovered),
// and an invisible button elsewhere so tab widths never jump.
void Window::updateTabButtons()
{
    static const char *closeSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M4.5 4.5l7 7m0-7l-7 7" stroke="currentColor" stroke-width="1.2" stroke-linecap="round" fill="none"/></svg>)";
    static const char *dotSvg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><circle cx="8" cy="8" r="3.5" fill="currentColor"/></svg>)";
    static const QPixmap closePixmap = svgPixmap(closeSvg, "#ADB7C4", 16), dotPixmap = svgPixmap(dotSvg, "#ADB7C4", 16);
    static const QPixmap blank = [] { QPixmap pixmap(48, 48); pixmap.fill(Qt::transparent); pixmap.setDevicePixelRatio(3); return pixmap; }();
    for (int i = 0; i < m_tabs->count(); ++i)
    {
        QWidget *lead = m_tabs->tabBar()->tabButton(i, QTabBar::LeftSide);
        auto *button = lead ? lead->findChild<QToolButton *>() : nullptr;
        auto *editor = qobject_cast<Editor *>(m_tabs->widget(i));
        if (!button || !editor) continue;
        bool modified = editor->document()->isModified();
        bool shown = i == m_tabs->currentIndex() || i == m_hoverTab;
        QIcon icon(modified ? dotPixmap : shown ? closePixmap : blank);
        icon.addPixmap(closePixmap, QIcon::Active);
        button->setIcon(icon);
    }
}

// Tabs whose file names collide get their nearest distinguishing folders, dimmed, after the
// name (VS Code). QTabBar draws one text colour, so the hint is a label in the tab's right slot.
void Window::updateTabHints()
{
    QHash<QString, QList<int>> byName;
    for (int i = 0; i < m_tabs->count(); ++i)
        if (QString path = m_tabs->widget(i)->property("path").toString(); !path.isEmpty())
            byName[QFileInfo(path).fileName()].append(i);
    for (int i = 0; i < m_tabs->count(); ++i)
    {
        QString path = m_tabs->widget(i)->property("path").toString();
        QString hint;
        const QList<int> same = path.isEmpty() ? QList<int>() : byName.value(QFileInfo(path).fileName());
        // Fewest trailing folders that no other tab with this name shares.
        QStringList folders = QFileInfo(path).absolutePath().split('/', Qt::SkipEmptyParts);
        for (int depth = 1; same.size() > 1 && depth <= folders.size() && hint.isEmpty(); ++depth)
        {
            QString candidate = folders.mid(folders.size() - depth).join('/');
            bool unique = std::none_of(same.cbegin(), same.cend(), [&](int other)
            {
                return other != i && QFileInfo(m_tabs->widget(other)->property("path").toString()).absolutePath().endsWith("/" + candidate);
            });
            if (unique || depth == folders.size()) hint = candidate;
        }
        auto *label = qobject_cast<QLabel *>(m_tabs->tabBar()->tabButton(i, QTabBar::RightSide));
        if (label && label->text() == hint) continue;
        if (label) { m_tabs->tabBar()->setTabButton(i, QTabBar::RightSide, nullptr); label->deleteLater(); }
        if (hint.isEmpty()) continue;
        label = new QLabel(hint);
        label->setObjectName("tabHint");
        label->setAttribute(Qt::WA_TransparentForMouseEvents); // clicks still select the tab
        m_tabs->tabBar()->setTabButton(i, QTabBar::RightSide, label);
    }
}

// The application-wide filter must go before child widgets are destroyed: their events would
// otherwise reach eventFilter, which uses those children.
Window::~Window() { qApp->removeEventFilter(this); }

// Problems panel, grouped by file like VS Code's; editors get squiggles for their file.
void Window::setProblems(const QList<Problem> &problems, const QString &directory)
{
    static const QIcon errorIcon(svgPixmap(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3"><circle cx="8" cy="8" r="6"/><path d="M5.8 5.8l4.4 4.4m0-4.4l-4.4 4.4"/></svg>)", "#F14C4C", 16));
    static const QIcon warningIcon(svgPixmap(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linejoin="round"><path d="M8 2.2l6.2 11H1.8z"/><path d="M8 6.5v3.2m0 1.6v.1"/></svg>)", "#CCA700", 16));
    m_problems = problems;
    m_problemsTime = QDateTime::currentDateTime();
    m_problemIndex = -1;
    m_problemsView->clear();
    QHash<QString, QTreeWidgetItem *> groups;
    for (int i = 0; i < problems.size(); ++i)
    {
        const Problem &problem = problems[i];
        auto *&group = groups[problem.file];
        if (!group)
        {
            group = new QTreeWidgetItem(m_problemsView);
            group->setIcon(0, fileIcon(problem.file));
            group->setToolTip(0, problem.file);
            group->setData(0, Qt::UserRole + 1, 0);
        }
        group->setData(0, Qt::UserRole + 1, group->data(0, Qt::UserRole + 1).toInt() + 1);
        QString folder = QDir(directory).relativeFilePath(QFileInfo(problem.file).absolutePath());
        group->setText(0, QFileInfo(problem.file).fileName() + (folder == "." ? "" : "  " + folder)
            + QString("  (%1)").arg(group->data(0, Qt::UserRole + 1).toInt()));
        auto *item = new QTreeWidgetItem(group);
        item->setIcon(0, problem.error ? errorIcon : warningIcon);
        item->setText(0, problem.message + (problem.line > 0 ? QString("  [Ln %1]").arg(problem.line) : QString()));
        item->setToolTip(0, problem.message);
        item->setData(0, Qt::UserRole, i);
    }
    if (problems.isEmpty())
        (new QTreeWidgetItem(m_problemsView, {"No problems have been detected in the workspace."}))->setFlags(Qt::NoItemFlags);
    m_problemsView->expandAll();
    int errors = int(std::count_if(problems.cbegin(), problems.cend(), [](const Problem &p) { return p.error; }));
    m_errorCount->setText(QString::number(errors));
    m_warningCount->setText(QString::number(problems.size() - errors));
    for (int i = 0; i < m_tabs->count(); ++i)
        if (auto *editor = qobject_cast<Editor *>(m_tabs->widget(i))) applyProblems(editor);
}

void Window::applyProblems(Editor *editor)
{
    QString path = editor->property("path").toString();
    QList<std::tuple<int, bool, QString>> marks;
    // A file changed since the build would get squiggles on the wrong lines.
    bool current = !path.isEmpty() && QFileInfo(path).lastModified() <= m_problemsTime;
    for (const auto &problem : m_problems)
        if (current && problem.line > 0 && QFileInfo(problem.file) == QFileInfo(path))
            marks.append({problem.line, problem.error, problem.message});
    editor->setProblems(marks);
}

void Window::goToProblem(int index)
{
    if (index < 0 || index >= m_problems.size()) return;
    m_problemIndex = index;
    const Problem &problem = m_problems[index];
    if (!openSource(problem.file)) return;
    auto *editor = currentEditor();
    if (problem.line > 0) editor->goToLine(problem.line);
    editor->setFocus();
    for (QTreeWidgetItemIterator it(m_problemsView); *it; ++it)
        if ((*it)->data(0, Qt::UserRole).isValid() && (*it)->data(0, Qt::UserRole).toInt() == index) m_problemsView->setCurrentItem(*it);
    m_status->setText(QString("%1 %2 of %3: %4").arg(problem.error ? "Error" : "Warning").arg(index + 1).arg(m_problems.size()).arg(problem.message));
}

void Window::setMenuBarShown(bool on)
{
    m_menuBarShown = on;
    m_topBar->setVisible(on);
    m_menuBarAction->setChecked(on);
}

bool Window::eventFilter(QObject *object, QEvent *event)
{
    // A lone Alt (pressed and released with no other key) shows or hides a hidden menu bar.
    // The application filter may see one key event per receiver, hence the flag reset.
    // Keys bound to shortcuts (Alt+Z) arrive only as ShortcutOverride, never as KeyPress.
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride)
        m_altAlone = static_cast<QKeyEvent *>(event)->key() == Qt::Key_Alt;
    else if (event->type() == QEvent::KeyRelease && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Alt && m_altAlone)
    {
        m_altAlone = false;
        if (!m_menuBarShown) m_topBar->setVisible(!m_topBar->isVisible());
    }
    if (event->type() == QEvent::Shortcut && qobject_cast<QAction *>(object))
    {
        m_viaShortcut = true; // the action triggers synchronously inside this event
        QTimer::singleShot(0, this, [this] { m_viaShortcut = false; });
    }
    // Any wheel or touchpad movement over the tabs scrolls them; Qt would switch tabs instead.
    if (object == m_tabs->tabBar() && event->type() == QEvent::Wheel)
    {
        auto *wheel = static_cast<QWheelEvent *>(event);
        QPoint pixels = wheel->pixelDelta(), angle = wheel->angleDelta();
        int delta = wheel->phase() == Qt::NoScrollPhase || pixels.isNull()
            ? (angle.x() ? angle.x() : angle.y()) * 40 / 120 : (pixels.x() ? pixels.x() : pixels.y());
        m_tabScroll += delta;
        // Each hidden-arrow click scrolls by one tab; about 40 px of travel per click.
        auto *bar = m_tabs->tabBar();
        for (; m_tabScroll <= -40; m_tabScroll += 40)
            if (auto *right = bar->findChild<QToolButton *>("ScrollRightButton"); right && right->isEnabled()) right->click();
        for (; m_tabScroll >= 40; m_tabScroll -= 40)
            if (auto *left = bar->findChild<QToolButton *>("ScrollLeftButton"); left && left->isEnabled()) left->click();
        return true;
    }
    if (object == m_tabs->tabBar() && (event->type() == QEvent::MouseMove || event->type() == QEvent::Leave))
    {
        int hover = event->type() == QEvent::Leave ? -1 : m_tabs->tabBar()->tabAt(static_cast<QMouseEvent *>(event)->position().toPoint());
        if (hover != m_hoverTab) { m_hoverTab = hover; updateTabButtons(); }
    }
    if (object == m_tabs && event->type() == QEvent::Resize) placeFind();
    return QMainWindow::eventFilter(object, event);
}

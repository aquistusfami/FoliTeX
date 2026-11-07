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
    QString flag = engine == "LuaLaTeX" ? "-lualatex" : engine == "XeLaTeX" ? "-xelatex" : "-pdf";
    // Arguments are passed directly, so spaces, Unicode and shell metacharacters stay filenames.
    m_process.start("latexmk", {flag, "-interaction=nonstopmode", "-file-line-error", "-synctex=1", "-outdir=.folitex-build", "./" + QFileInfo(root).fileName()});
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
    QByteArray data = QByteArray(svg).replace("currentColor", color);
    QBuffer buffer(&data);
    QImageReader reader(&buffer, "svg");
    reader.setScaledSize(QSize(size * 3, size * 3));
    QPixmap pixmap = QPixmap::fromImage(reader.read());
    pixmap.setDevicePixelRatio(3);
    return pixmap;
}

Window::Window() : m_runner(this)
{
    setWindowTitle("Folitex");
    resize(1280, 820);
    setMinimumSize(680, 420);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(8, 4, 8, 4);
    auto *menuBar = new QMenuBar;
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
    });
    menu->addSeparator();
    add("Save", QKeySequence::Save, [this] { save(currentEditor()); });
    add("Save as…", QKeySequence::SaveAs, [this] { save(currentEditor(), true); });
    add("Close file", QKeySequence::Close, [this] { closeTab(m_tabs->currentIndex()); });
    menu = menuBar->addMenu("&Edit");
    add("Undo", QKeySequence::Undo, [this] { if (auto *editor = currentEditor()) editor->undo(); });
    add("Redo", QKeySequence::Redo, [this] { if (auto *editor = currentEditor()) editor->redo(); });
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
    menu = menuBar->addMenu("&Insert");
    add("Bold", QKeySequence("Ctrl+B"), insert("\\textbf{\u2022}"));
    add("Italic", QKeySequence("Ctrl+I"), insert("\\textit{\u2022}"));
    add("Underline", QKeySequence("Ctrl+U"), insert("\\underline{\u2022}"));
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
    menu = menuBar->addMenu("&View");
    add("Toggle side bar", QKeySequence("Ctrl+Shift+O"), [this] { showSide(m_outlineButton->isChecked() ? m_outline : m_files); });
    add("Toggle PDF", QKeySequence("Ctrl+\\"), [this] { m_pdfPane->setVisible(!m_pdfPane->isVisible()); });
    add("Toggle log", QKeySequence("Ctrl+J"), [this] { m_log->setVisible(!m_log->isVisible()); });
    menu->addSeparator();
    m_wrapAction = add("Word wrap", QKeySequence("Alt+Z"), [this](bool on) { setWordWrap(on); });
    m_wrapAction->setCheckable(true);
    menu->addSeparator();
    add("Zoom in PDF", QKeySequence::ZoomIn, [this] { m_pdf->setZoom(m_pdf->zoom() * 1.25); })->setShortcuts({QKeySequence("Ctrl+="), QKeySequence::ZoomIn});
    add("Zoom out PDF", QKeySequence::ZoomOut, [this] { m_pdf->setZoom(m_pdf->zoom() / 1.25); });
    add("Fit PDF width", QKeySequence("Ctrl+0"), [this] { m_pdf->fitWidth(); });
    menu->addSeparator();
    add("Next tab", QKeySequence("Ctrl+PgDown"), [this] { m_tabs->setCurrentIndex((m_tabs->currentIndex() + 1) % m_tabs->count()); });
    add("Previous tab", QKeySequence("Ctrl+PgUp"), [this] { m_tabs->setCurrentIndex((m_tabs->currentIndex() + m_tabs->count() - 1) % m_tabs->count()); });
    bar->addWidget(menuBar);
    bar->addStretch();
    // Each control sits with what it acts on: Build with the editor tabs, zoom and pages with the PDF.
    // VS Code-style editor action: outlined run triangle, a stop square while building.
    m_runIcon = QIcon(svgPixmap(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.2" stroke-linejoin="round"><path d="M4.5 2.8v10.4L12.8 8z"/></svg>)", "#CCCCCC", 16));
    m_stopIcon = QIcon(svgPixmap(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><rect x="3.5" y="3.5" width="9" height="9" rx="1" fill="currentColor"/></svg>)", "#F48771", 16));
    m_build = new QPushButton;
    m_build->setObjectName("editorAction");
    m_build->setIcon(m_runIcon);
    m_build->setIconSize(QSize(16, 16));
    m_build->setToolTip("Build · F5");
    connect(m_build, &QPushButton::clicked, this, &Window::build);
    m_pdf = new PdfView;
    m_status = new QLabel("Ready");
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
    layout->addLayout(bar);
    m_tabs = new QTabWidget;
    m_tabs->setDocumentMode(true);
    m_tabs->tabBar()->setDrawBase(false);
    m_tabs->setTabsClosable(true);
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
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &Window::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] { if (currentEditor()) setTabTitle(currentEditor()); refreshOutline(); });
    m_outline = new QTreeWidget;
    m_outline->setHeaderHidden(true);
    m_outline->setIndentation(12);
    m_outline->setFrameShape(QFrame::NoFrame);
    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot); // no QDir::Hidden: hides .folitex-build
    m_fileModel->setNameFilters({"*.tex", "*.bib", "*.sty", "*.cls", "*.pdf"});
    m_fileModel->setNameFilterDisables(false);
    m_files = new QTreeView;
    m_files->setModel(m_fileModel);
    m_files->setHeaderHidden(true);
    for (int column = 1; column < m_fileModel->columnCount(); ++column) m_files->hideColumn(column);
    m_files->setIndentation(12);
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
    m_sideStack->addWidget(m_files);
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
        for (auto [color, state] : {std::pair{"#868686", QIcon::Off}, std::pair{"#D7D7D7", QIcon::On}})
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
    m_horizontal = new QSplitter;
    m_horizontal->addWidget(m_side);
    m_horizontal->addWidget(m_tabs);
    m_horizontal->addWidget(m_pdfPane);
    m_horizontal->setSizes({200, 540, 540});
    m_horizontal->setStretchFactor(0, 0);
    m_horizontal->setStretchFactor(1, 1);
    m_horizontal->setStretchFactor(2, 1);
    m_horizontal->setChildrenCollapsible(false);
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1500);
    m_log->setFont(QFont("monospace", 10));
    m_log->hide();
    m_vertical = new QSplitter(Qt::Vertical);
    m_vertical->addWidget(m_horizontal);
    m_vertical->addWidget(m_log);
    m_vertical->setStretchFactor(0, 1);
    m_vertical->setStretchFactor(1, 0);
    auto *row = new QHBoxLayout;
    row->setSpacing(0);
    row->addWidget(activity);
    row->addWidget(m_vertical);
    layout->addLayout(row);
    setCentralWidget(central);
    connect(&m_runner, &BuildRunner::output, this, [this](const QString &text)
    {
        auto cursor = m_log->textCursor(); cursor.movePosition(QTextCursor::End); cursor.insertText(text);
        m_log->setTextCursor(cursor);
    });
    connect(&m_runner, &BuildRunner::finished, this, [this](bool success, const QString &pdf)
    {
        m_build->setIcon(m_runIcon);
        m_build->setToolTip("Build · F5");
        m_status->setText(success ? "Build succeeded" : "Build failed — see log");
        if (success) openPdf(pdf);
        else { m_log->show(); m_vertical->setSizes({580, 170}); }
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
        static const QStringList colors = {"#CCCCCC", "#4FC1FF", "#C586C0", "#CCA700"};
        item->setText(0, prefixes[entry.kind] + (entry.title.isEmpty() ? "…" : entry.title));
        item->setForeground(0, QColor(colors[entry.kind]));
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
    m_workspace = info.absoluteFilePath();
    m_files->setRootIndex(m_fileModel->setRootPath(m_workspace));
    showSide(m_files, false);
    if (auto *editor = currentEditor()) setTabTitle(editor);
}

// Like VS Code's activity bar: clicking the active view's button closes the side bar.
void Window::showSide(QWidget *page, bool toggle)
{
    QWidget *shown = page == m_files && m_workspace.isEmpty() ? m_noFolder : page;
    bool close = toggle && m_side->isVisible() && m_sideStack->currentWidget() == shown;
    m_side->setVisible(!close);
    m_sideStack->setCurrentWidget(shown);
    m_sideTitle->setText(page == m_outline ? "OUTLINE" : m_workspace.isEmpty() ? "FILES" : QFileInfo(m_workspace).fileName().toUpper());
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
    m_tabs->addTab(editor, "Untitled");
    m_tabs->setCurrentWidget(editor);
    connect(editor->document(), &QTextDocument::modificationChanged, this, [this, editor] { setTabTitle(editor); });
    editor->setWordWrap(m_wordWrap);
    connect(editor, &Editor::wordWrapToggled, this, &Window::setWordWrap);
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
    if (editor->document()->isModified()) name += " •";
    int index = m_tabs->indexOf(editor);
    if (index >= 0) m_tabs->setTabText(index, name);
    if (editor != currentEditor()) return;
    setWindowTitle(name + (m_workspace.isEmpty() ? "" : " — " + QFileInfo(m_workspace).fileName()) + " — Folitex");
    if (!m_workspace.isEmpty() && !path.isEmpty()) m_files->setCurrentIndex(m_fileModel->index(path));
}

bool Window::openSource(const QString &path)
{
    QFileInfo info(path);
    QString absolute = info.absoluteFilePath();
    for (int i = 0; i < m_tabs->count(); ++i)
        if (m_tabs->widget(i)->property("path").toString() == absolute)
        { m_tabs->setCurrentIndex(i); return true; }
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
    watchSource(absolute);
    return true;
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
    QString root = BuildRunner::rootFile(editor->property("path").toString());
    // Only save other documents explicitly associated with the same TeX root.
    for (int i = 0; i < m_tabs->count(); ++i)
    {
        auto *other = qobject_cast<Editor *>(m_tabs->widget(i));
        if (!other || other == editor || !other->document()->isModified()) continue;
        QString path = other->property("path").toString();
        if (!path.isEmpty() && !root.isEmpty() && BuildRunner::rootFile(path) == root && !save(other)) return;
    }
    m_log->clear();
    m_status->setText("Building…"); m_build->setIcon(m_stopIcon); m_build->setToolTip("Stop build · F5");
    m_runner.start(editor->property("path").toString(), m_engine->currentText());
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

void Window::inverseSync(int page, double x, double y)
{
    QString position = QString::number(page) + ':' + QString::number(x) + ':' + QString::number(y) + ':' + m_pdf->path();
    syncProcess({"edit", "-o", position}, [this](const QString &text)
    {
        auto input = QRegularExpression(R"(^Input:(.+)$)", QRegularExpression::MultilineOption).match(text);
        auto line = QRegularExpression(R"(^Line:(\d+))", QRegularExpression::MultilineOption).match(text);
        if (!input.hasMatch() || !line.hasMatch()) return;
        QString path = input.captured(1).trimmed();
        if (QFileInfo(path).isRelative()) path = QDir(QFileInfo(m_pdf->path()).absolutePath()).absoluteFilePath(path);
        if (openSource(QDir::cleanPath(path))) currentEditor()->goToLine(line.captured(1).toInt());
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

bool Window::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_tabs && event->type() == QEvent::Resize) placeFind();
    return QMainWindow::eventFilter(object, event);
}

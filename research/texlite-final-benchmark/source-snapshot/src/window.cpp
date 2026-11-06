#include "window.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
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
        emit output("Không chạy được latexmk: " + m_process.errorString());
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
    return info.absolutePath() + "/.texlite-build/" + info.completeBaseName() + ".pdf";
}

bool BuildRunner::start(const QString &source, const QString &engine)
{
    if (running()) return false;
    m_cancelRequested = false;
    QString root = rootFile(source);
    if (root.isEmpty())
    {
        emit output("Không tìm được file gốc, hoặc chỉ dẫn TeX root tạo thành vòng lặp.");
        emit finished(false, {});
        return false;
    }
    m_pdf = pdfPath(root);
    m_process.setWorkingDirectory(QFileInfo(root).absolutePath());
    QString flag = engine == "LuaLaTeX" ? "-lualatex" : engine == "XeLaTeX" ? "-xelatex" : "-pdf";
    // Arguments are passed directly, so spaces, Unicode and shell metacharacters stay filenames.
    m_process.start("latexmk", {flag, "-interaction=nonstopmode", "-file-line-error", "-synctex=1", "-outdir=.texlite-build", "./" + QFileInfo(root).fileName()});
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

Window::Window() : m_runner(this)
{
    setWindowTitle("TeXLite");
    resize(1280, 820);
    setMinimumSize(680, 420);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(12, 7, 12, 7);
    auto *menuButton = new QToolButton;
    menuButton->setText("≡");
    menuButton->setToolTip("Lệnh và phím tắt");
    menuButton->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(menuButton);
    menuButton->setMenu(menu);
    auto add = [this, menu](const QString &title, const QKeySequence &keys, auto handler)
    {
        auto *action = menu->addAction(title);
        action->setShortcut(keys);
        addAction(action);
        connect(action, &QAction::triggered, this, handler);
    };
    add("Tài liệu mới", QKeySequence::New, [this] { newDocument(); });
    add("Mở TeX hoặc PDF…", QKeySequence::Open, [this]
    {
        QString path = QFileDialog::getOpenFileName(this, "Mở tài liệu", {}, "TeX / PDF (*.tex *.bib *.sty *.cls *.pdf);;Tất cả (*)");
        if (path.isEmpty()) return;
        if (path.endsWith(".pdf", Qt::CaseInsensitive)) openPdf(path); else openSource(path);
    });
    add("Lưu", QKeySequence::Save, [this] { save(currentEditor()); });
    add("Lưu thành…", QKeySequence::SaveAs, [this] { save(currentEditor(), true); });
    add("Đóng file", QKeySequence::Close, [this] { closeTab(m_tabs->currentIndex()); });
    menu->addSeparator();
    add("Biên dịch / Dừng", QKeySequence("Ctrl+B"), [this] { build(); });
    add("Mã nguồn → PDF", QKeySequence("Ctrl+Return"), [this] { forwardSync(); });
    add("Vừa chiều rộng PDF", QKeySequence("Ctrl+0"), [this] { m_pdf->fitWidth(); });
    add("Ẩn / hiện PDF", QKeySequence("Ctrl+\\"), [this] { m_pdf->setVisible(!m_pdf->isVisible()); });
    add("Ẩn / hiện log", QKeySequence("Ctrl+J"), [this] { m_log->setVisible(!m_log->isVisible()); });
    add("Tìm trong file", QKeySequence::Find, [this] { m_findBar->show(); m_find->setFocus(); m_find->selectAll(); });
    add("Đi đến dòng…", QKeySequence("Ctrl+G"), [this]
    {
        auto *editor = currentEditor(); if (!editor) return;
        bool ok = false;
        int line = QInputDialog::getInt(this, "Đi đến dòng", "Dòng:", editor->lineNumber(), 1,
            editor->document()->blockCount(), 1, &ok);
        if (ok) editor->goToLine(line);
    });
    add("Bật / tắt comment", QKeySequence("Ctrl+/"), [this] { if (currentEditor()) currentEditor()->toggleComment(); });
    bar->addWidget(menuButton);
    auto *brand = new QLabel("TeXLite");
    brand->setStyleSheet("font-weight:600; color:#C9D3DF; padding:0 10px;");
    bar->addWidget(brand);
    bar->addStretch();
    m_status = new QLabel("Sẵn sàng");
    m_status->setMinimumWidth(70);
    bar->addWidget(m_status);
    m_engine = new QComboBox;
    m_engine->addItems({"pdfLaTeX", "LuaLaTeX", "XeLaTeX"});
    bar->addWidget(m_engine);
    m_build = new QPushButton("Build  Ctrl+B");
    connect(m_build, &QPushButton::clicked, this, &Window::build);
    bar->addWidget(m_build);
    auto *fit = new QPushButton("Fit");
    fit->setToolTip("Vừa chiều rộng PDF · Ctrl+0");
    connect(fit, &QPushButton::clicked, m_pdf = new PdfView, &PdfView::fitWidth);
    bar->addWidget(fit);
    m_page = new QSpinBox;
    m_page->setRange(1, 1);
    m_page->setFixedWidth(68);
    connect(m_page, qOverload<int>(&QSpinBox::valueChanged), this, [this](int page) { m_pdf->goToPage(page); });
    bar->addWidget(m_page);
    m_pageCount = new QLabel("/ 0");
    bar->addWidget(m_pageCount);
    layout->addLayout(bar);
    m_findBar = new QWidget;
    auto *findLayout = new QHBoxLayout(m_findBar);
    findLayout->setContentsMargins(12, 2, 12, 7);
    m_find = new QLineEdit;
    m_find->setPlaceholderText("Tìm trong file hiện tại…");
    findLayout->addWidget(m_find);
    auto find = [this](bool backwards)
    {
        auto *editor = currentEditor();
        if (!editor || m_find->text().isEmpty()) return;
        auto flags = backwards ? QTextDocument::FindBackward : QTextDocument::FindFlags{};
        if (editor->find(m_find->text(), flags)) return;
        auto cursor = editor->textCursor();
        cursor.movePosition(backwards ? QTextCursor::End : QTextCursor::Start);
        editor->setTextCursor(cursor);
        editor->find(m_find->text(), flags);
    };
    auto *previous = new QPushButton("↑");
    auto *next = new QPushButton("↓");
    auto *close = new QPushButton("×");
    connect(previous, &QPushButton::clicked, this, [find] { find(true); });
    connect(next, &QPushButton::clicked, this, [find] { find(false); });
    connect(m_find, &QLineEdit::returnPressed, this, [find] { find(false); });
    connect(close, &QPushButton::clicked, m_findBar, &QWidget::hide);
    findLayout->addWidget(previous); findLayout->addWidget(next); findLayout->addWidget(close);
    layout->addWidget(m_findBar);
    m_findBar->hide();
    m_tabs = new QTabWidget;
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &Window::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] { if (currentEditor()) setTabTitle(currentEditor()); });
    m_horizontal = new QSplitter;
    m_horizontal->addWidget(m_tabs);
    m_horizontal->addWidget(m_pdf);
    m_horizontal->setSizes({640, 640});
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
    layout->addWidget(m_vertical);
    setCentralWidget(central);
    connect(&m_runner, &BuildRunner::output, this, [this](const QString &text)
    {
        auto cursor = m_log->textCursor(); cursor.movePosition(QTextCursor::End); cursor.insertText(text);
        m_log->setTextCursor(cursor);
    });
    connect(&m_runner, &BuildRunner::finished, this, [this](bool success, const QString &pdf)
    {
        m_build->setText("Build  Ctrl+B");
        m_status->setText(success ? "Build xong" : "Build lỗi — xem log");
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

Editor *Window::newDocument(const QString &text)
{
    auto *editor = new Editor;
    editor->setPlainText(text);
    editor->document()->setModified(false);
    m_tabs->addTab(editor, "Chưa đặt tên");
    m_tabs->setCurrentWidget(editor);
    connect(editor->document(), &QTextDocument::modificationChanged, this, [this, editor] { setTabTitle(editor); });
    connect(editor, &QTextEdit::cursorPositionChanged, this, [this, editor]
    {
        if (editor == currentEditor() && !m_runner.running())
            m_status->setText(QString("Ln %1, Col %2").arg(editor->lineNumber()).arg(editor->textCursor().positionInBlock() + 1));
    });
    editor->setFocus();
    return editor;
}

void Window::setTabTitle(Editor *editor)
{
    QString path = editor->property("path").toString();
    QString name = path.isEmpty() ? "Chưa đặt tên" : QFileInfo(path).fileName();
    if (editor->document()->isModified()) name += " •";
    int index = m_tabs->indexOf(editor);
    if (index >= 0) m_tabs->setTabText(index, name);
    if (editor == currentEditor()) setWindowTitle(name + " — TeXLite");
}

bool Window::openSource(const QString &path)
{
    QFileInfo info(path);
    QString absolute = info.absoluteFilePath();
    for (int i = 0; i < m_tabs->count(); ++i)
        if (m_tabs->widget(i)->property("path").toString() == absolute)
        { m_tabs->setCurrentIndex(i); return true; }
    QFile file(absolute);
    if (!file.open(QIODevice::ReadOnly)) { m_status->setText("Không mở được file: " + file.errorString()); return false; }
    QByteArray bytes = file.readAll();
    QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    QString text = decoder(bytes);
    if (decoder.hasError())
    {
        m_status->setText("Bản thử chỉ mở mã nguồn UTF-8; hãy chuyển encoding trước khi mở.");
        return false;
    }
    if (text.contains(QChar::LineSeparator) || text.contains(QChar::ParagraphSeparator))
    {
        m_status->setText("Mã nguồn có ký tự ngắt dòng Unicode U+2028/U+2029; hãy chuyển sang LF hoặc CRLF trước.");
        return false;
    }
    auto *editor = currentEditor();
    if (!editor || !editor->toPlainText().isEmpty() || !editor->property("path").toString().isEmpty() || editor->document()->isModified())
        editor = newDocument();
    editor->setPlainText(text);
    editor->setProperty("bom", bytes.startsWith("\xef\xbb\xbf"));
    editor->setProperty("lineEnding", bytes.contains("\r\n") ? "\r\n" : !bytes.contains("\n") && bytes.contains("\r") ? "\r" : "\n");
    editor->setProperty("path", absolute);
    editor->setProperty("mtime", info.lastModified());
    editor->document()->setModified(false);
    setTabTitle(editor);
    return true;
}

bool Window::save(Editor *editor, bool saveAs)
{
    if (!editor) return false;
    QString current = editor->property("path").toString();
    QString path = current;
    if (saveAs || path.isEmpty())
        path = QFileDialog::getSaveFileName(this, "Lưu TeX", current, "TeX (*.tex);;Tất cả (*)");
    if (path.isEmpty()) return false;
    if (path == current && QFileInfo::exists(path) && QFileInfo(path).lastModified() != editor->property("mtime").toDateTime())
        if (QMessageBox::question(this, "File đã thay đổi", "File trên đĩa đã được sửa bên ngoài. Ghi đè bằng nội dung đang mở?") != QMessageBox::Yes) return false;
    QString text = editor->document()->toRawText();
    text.replace(QChar::ParagraphSeparator, QChar::LineFeed);
    text.replace(QChar::LineSeparator, QChar::LineFeed);
    QString ending = editor->property("lineEnding").toString();
    if (!ending.isEmpty() && ending != "\n") text.replace("\n", ending);
    QByteArray bytes = text.toUtf8();
    if (editor->property("bom").toBool()) bytes.prepend("\xef\xbb\xbf");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    { QMessageBox::warning(this, "Không lưu được", file.errorString()); return false; }
    editor->setProperty("path", QFileInfo(path).absoluteFilePath());
    editor->setProperty("mtime", QFileInfo(path).lastModified());
    editor->document()->setModified(false);
    setTabTitle(editor);
    return true;
}

bool Window::mayClose(Editor *editor)
{
    if (!editor || !editor->document()->isModified()) return true;
    auto answer = QMessageBox::question(this, "Thay đổi chưa lưu", "Lưu thay đổi trước khi đóng?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel) return false;
    return answer == QMessageBox::Discard || save(editor);
}

void Window::closeTab(int index)
{
    auto *editor = qobject_cast<Editor *>(m_tabs->widget(index));
    if (!editor || !mayClose(editor)) return;
    m_tabs->removeTab(index); editor->deleteLater();
    if (!m_tabs->count()) newDocument();
}

void Window::closeEvent(QCloseEvent *event)
{
    if (m_runner.running())
    {
        m_status->setText("Dừng biên dịch trước khi đóng.");
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
    m_status->setText("Đang biên dịch…"); m_build->setText("Dừng");
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
            else m_status->setText("SyncTeX chưa tìm được vị trí; build lại với synctex=1.");
            process->deleteLater();
        });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error)
    {
        if (error == QProcess::FailedToStart) { m_status->setText("Không chạy được synctex."); process->deleteLater(); }
    });
    process->start("synctex", arguments);
}

void Window::forwardSync()
{
    auto *editor = currentEditor();
    if (!editor || editor->property("path").toString().isEmpty() || m_pdf->path().isEmpty()) return;
    if (editor->document()->isModified()) { m_status->setText("Build trước để đồng bộ nội dung mới với PDF."); return; }
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

#include "window.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QPdfWriter>
#include <QPainter>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QAction>
#include <QCompleter>
#include <QTreeWidget>
#include <QFileSystemModel>
#include <QClipboard>
#include <QFontDatabase>
#include <QAbstractTextDocumentLayout>
#include <QAbstractItemView>
#ifdef Q_OS_UNIX
#include <signal.h>
#endif

class Tests : public QObject
{
    Q_OBJECT
private slots:
    void rootFiles();
    void editorCommentUndo();
    void completionAndSnippets();
    void editorAidsAndOutline();
    void workspaceTree();
    void externalEditsReload();
    void pdfInteraction();
    void findReplace();
    void wordWrap();
    void kineticScroll();
    void touchpadPixels();
    void wheelAnimation();
    void tiledPdf();
    void compilation();
    void utf8AndInvalidInput();
    void pdfErrors();
    void sourceRoundtrip();
    void immediateCancellation();
    void largePdfViewport();
    void failedTileDoesNotSpin();
    void cancellationCleansOrphanCompiler();
};

static void write(const QString &path, const QByteArray &text)
{
    QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(text), text.size());
}

void Tests::rootFiles()
{
    QTemporaryDir dir;
    write(dir.filePath("root.tex"), "\\documentclass{article}");
    write(dir.filePath("chapter.tex"), "% !TeX root = root.tex\nchapter");
    QCOMPARE(BuildRunner::rootFile(dir.filePath("chapter.tex")), dir.filePath("root.tex"));
    write(dir.filePath("root.tex"), "% !TEX root = chapter.tex\n");
    QVERIFY(BuildRunner::rootFile(dir.filePath("chapter.tex")).isEmpty());
}

void Tests::editorCommentUndo()
{
    Editor editor;
    editor.setPlainText("alpha\nbeta\ngamma");
    editor.selectAll();
    editor.toggleComment();
    QCOMPARE(editor.toPlainText(), "% alpha\n% beta\n% gamma");
    editor.undo();
    QCOMPARE(editor.toPlainText(), "alpha\nbeta\ngamma");
    editor.goToLine(2, 2);
    QCOMPARE(editor.lineNumber(), 2);
    QCOMPARE(editor.textCursor().positionInBlock(), 2);
}

void Tests::completionAndSnippets()
{
    QCOMPARE(Editor::closingFor("\\begin{itemize}\n\\item {a"), "}");
    QCOMPARE(Editor::closingFor("\\begin{itemize}\n\\item a % {"), "\\end{itemize}");
    QCOMPARE(Editor::closingFor("\\begin{a}\\begin{b}\\end{b} \\{"), "\\end{a}");
    QVERIFY(Editor::closingFor("\\begin{a}x\\end{a}").isEmpty());

    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.insertTemplate(QString("\\frac{%1}{%1}").arg(QChar(0x2022)));
    QCOMPARE(editor.toPlainText(), "\\frac{}{}");
    QCOMPARE(editor.textCursor().position(), 6);
    QTest::keyClicks(&editor, "a");
    QTest::keyClick(&editor, Qt::Key_Tab);
    QCOMPARE(editor.textCursor().position(), 9);
    QTest::keyClick(&editor, Qt::Key_Left, Qt::ControlModifier);
    QCOMPARE(editor.textCursor().position(), 7);
    QTest::keyClick(&editor, Qt::Key_Tab);
    QTest::keyClick(&editor, Qt::Key_Tab);
    QCOMPARE(editor.textCursor().position(), editor.toPlainText().size());
    QTest::keyClick(&editor, Qt::Key_Tab);
    QCOMPARE(editor.toPlainText(), "\\frac{a}{}    ");

    auto *completer = editor.findChild<QCompleter *>();
    QVERIFY(completer);
    auto accept = [&] { QTest::keyClick(completer->popup(), Qt::Key_Return); };
    editor.setPlainText("  \\begin{itemi");
    editor.moveCursor(QTextCursor::End);
    QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);
    QVERIFY(completer->popup()->isVisible());
    accept();
    QCOMPARE(editor.toPlainText(), "  \\begin{itemize}\n      \n  \\end{itemize}");
    QCOMPARE(editor.textCursor().blockNumber(), 1);

    editor.setPlainText("\\newcommand{\\vect}[1]{#1}\n\\label{fig:one}\n\\ref{fi");
    editor.moveCursor(QTextCursor::End);
    QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);
    QCOMPARE(completer->currentCompletion(), "fig:one");
    accept();
    QVERIFY(editor.toPlainText().endsWith("\\ref{fig:one"));
    QTest::keyClicks(&editor, "} \\vec");
    QCOMPARE(completer->completionCount(), 2);
    QTest::keyClicks(&editor, "t");
    QCOMPARE(completer->currentCompletion(), "\\vect{\u2022}");
    accept();
    QVERIFY(editor.toPlainText().endsWith("\\vect{}"));

    QTemporaryDir dir;
    write(dir.filePath("refs.bib"), "@article{knuth84,\n title={TeX}}\n@string{x = \"y\"}\n");
    editor.setProperty("path", dir.filePath("main.tex"));
    editor.setPlainText("\\cite{a, kn");
    editor.moveCursor(QTextCursor::End);
    QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);
    QCOMPARE(completer->completionCount(), 1);
    QCOMPARE(completer->currentCompletion(), "knuth84");

    editor.setPlainText("\\documentclass{artic");
    editor.moveCursor(QTextCursor::End);
    QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);
    QCOMPARE(completer->currentCompletion(), "article");
    editor.setPlainText("\\usepackage[utf8]{inputenc, amsmat");
    editor.moveCursor(QTextCursor::End);
    QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);
    QCOMPARE(completer->currentCompletion(), "amsmath");
    accept();
    QCOMPARE(editor.toPlainText(), "\\usepackage[utf8]{inputenc, amsmath");

    Window window;
    window.show();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    auto *source = window.currentEditor();
    source->setFocus();
    QTest::keyClick(source, Qt::Key_B, Qt::ControlModifier);
    QTest::keyClicks(source, "x");
    QTest::keyClick(source, Qt::Key_Tab);
    QTest::keyClick(source, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
    QCOMPARE(source->toPlainText(), "\\textbf{x}$$");
    source->setPlainText("\\begin{center}{");
    source->moveCursor(QTextCursor::End);
    QTest::keyClick(source, Qt::Key_Return, Qt::AltModifier);
    QTest::keyClick(source, Qt::Key_Return, Qt::AltModifier);
    QCOMPARE(source->toPlainText(), "\\begin{center}{}\\end{center}");
    // TeXstudio defaults, plus Ctrl+U for \underline; a selection is wrapped.
    source->setPlainText("word");
    source->selectAll();
    QTest::keyClick(source, Qt::Key_U, Qt::ControlModifier);
    QCOMPARE(source->toPlainText(), "\\underline{word}");
    source->clear();
    QTest::keyClick(source, Qt::Key_E, Qt::ControlModifier | Qt::ShiftModifier);
    QTest::keyClicks(source, "a");
    QTest::keyClick(source, Qt::Key_Tab);
    QTest::keyClick(source, Qt::Key_F, Qt::AltModifier | Qt::ShiftModifier);
    QTest::keyClicks(source, "1");
    QTest::keyClick(source, Qt::Key_Tab);
    QTest::keyClicks(source, "2");
    QTest::keyClick(source, Qt::Key_Tab);
    QTest::keyClick(source, Qt::Key_Return, Qt::ControlModifier);
    QCOMPARE(source->toPlainText(), "\\emph{a}\\frac{1}{2}\\\\\n");
}

void Tests::editorAidsAndOutline()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    QTest::keyClicks(&editor, "  \\textbf{x");
    QCOMPARE(editor.toPlainText(), "  \\textbf{x}");
    QCOMPARE(editor.extraSelections().size(), 3);
    QTest::keyClicks(&editor, "}");
    QCOMPARE(editor.toPlainText(), "  \\textbf{x}");
    QCOMPARE(editor.extraSelections().size(), 3);
    QTest::keyClick(&editor, Qt::Key_Return);
    QCOMPARE(editor.toPlainText(), "  \\textbf{x}\n  ");
    QCOMPARE(editor.extraSelections().size(), 1);
    QTest::keyClicks(&editor, "\\{(");
    QCOMPARE(editor.toPlainText(), "  \\textbf{x}\n  \\{()");
    QTest::keyClick(&editor, Qt::Key_Backspace);
    QCOMPARE(editor.toPlainText(), "  \\textbf{x}\n  \\{");

    const auto entries = Editor::outline(
        "\\section{Intro}\n\\label{sec:intro}\n% \\section{Hidden}\n% TODO: fix\n"
        "\\subsection*{Deep \\emph{x}}\n\\input{chap}\n\\chapter{Next}");
    QCOMPARE(entries.size(), 6);
    QCOMPARE(entries[0].title, "Intro");
    QCOMPARE(entries[0].level, 2);
    QCOMPARE(entries[1].kind, OutlineEntry::Label);
    QCOMPARE(entries[2].kind, OutlineEntry::Todo);
    QCOMPARE(entries[2].title, "fix");
    QCOMPARE(entries[3].title, "Deep \\emph{x}");
    QCOMPARE(entries[3].line, 5);
    QCOMPARE(entries[4].kind, OutlineEntry::Include);
    QCOMPARE(entries[5].level, 1);

    Window window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.currentEditor()->setPlainText("\\section{A}\n\\subsection{B}\n\\label{b}\n\\section{C}");
    QTRY_COMPARE(window.outlineView()->topLevelItemCount(), 2);
    auto *b = window.outlineView()->topLevelItem(0)->child(0);
    QCOMPARE(b->text(0), "B");
    QCOMPARE(b->childCount(), 1);
    window.outlineView()->itemClicked(b, 0);
    QCOMPARE(window.currentEditor()->lineNumber(), 2);
    QCOMPARE(window.outlineView()->currentItem(), b);
}

void Tests::workspaceTree()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkpath("chapters"));
    QVERIFY(QDir(dir.path()).mkpath(".folitex-build"));
    write(dir.filePath("main.tex"), "\\section{Main}\n");
    write(dir.filePath("chapters/one.tex"), "one");
    write(dir.filePath("notes.txt"), "ignored");
    write(dir.filePath(".folitex-build/main.pdf"), "ignored");

    Window window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(!window.filesView()->isVisible());
    window.openWorkspace(dir.path());
    QVERIFY(window.filesView()->isVisible());
    QVERIFY(window.windowTitle().contains("Folitex"));
    auto *model = qobject_cast<QFileSystemModel *>(window.filesView()->model());
    auto root = window.filesView()->rootIndex();
    QTRY_COMPARE(model->rowCount(root), 2);
    QStringList names;
    for (int row = 0; row < model->rowCount(root); ++row) names << model->fileName(model->index(row, 0, root));
    names.sort();
    QCOMPARE(names, QStringList({"chapters", "main.tex"}));
    auto main = model->index(dir.filePath("main.tex"));
    window.filesView()->clicked(main);
    QCOMPARE(window.currentEditor()->property("path").toString(), QFileInfo(dir.filePath("main.tex")).absoluteFilePath());
    QCOMPARE(window.filesView()->currentIndex(), main);
    QVERIFY(window.windowTitle().contains(QFileInfo(dir.path()).fileName()));
}

void Tests::externalEditsReload()
{
    QTemporaryDir dir;
    QString path = dir.filePath("main.tex");
    write(path, "\\section{A}\r\nold line\r\nkeep\u00a0me\r\n");
    // Like editors and AI tools that save atomically: write a temporary file, then rename it over.
    auto replace = [&](const QByteArray &bytes)
    {
        write(dir.filePath("tmp"), bytes);
        QFile::remove(path);
        QVERIFY(QFile::rename(dir.filePath("tmp"), path));
    };
    Window window;
    QVERIFY(window.openSource(path));
    auto *editor = window.currentEditor();
    editor->goToLine(3, 2);
    replace("\\section{A}\r\nnew line from AI\r\nkeep\u00a0me\r\n");
    QTRY_VERIFY_WITH_TIMEOUT(editor->toPlainText().contains("new line from AI"), 3000);
    QVERIFY(!editor->document()->isModified());
    QCOMPARE(editor->textCursor().blockNumber(), 2);
    QCOMPARE(editor->textCursor().positionInBlock(), 2);
    QVERIFY(editor->document()->toRawText().contains(QChar(0x00a0)));

    replace("\\section{A}\r\nsecond change\r\nkeep\u00a0me\r\n");
    QTRY_VERIFY_WITH_TIMEOUT(editor->toPlainText().contains("second change"), 3000);
    editor->undo();
    QVERIFY(editor->toPlainText().contains("new line from AI"));

    editor->textCursor().insertText("mine ");
    replace("\\section{B}\r\n");
    QTest::qWait(600);
    QVERIFY(editor->toPlainText().contains("mine "));
    QVERIFY(!editor->toPlainText().contains("section{B}"));
    QVERIFY(editor->document()->isModified());
}

void Tests::pdfInteraction()
{
    // Without any installed font QPdfWriter produces no extractable text (minimal containers).
    if (QFontDatabase::families().isEmpty()) QSKIP("No fonts installed");
    QTemporaryDir dir;
    QString path = dir.filePath("text.pdf");
    {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setResolution(72);
        QPainter painter(&writer);
        painter.setFont(QFont("DejaVu Sans", 40));
        painter.drawText(QPointF(60, 420), "Hello wide world");
    }
    PdfView viewer;
    viewer.resize(700, 900);
    viewer.show();
    QVERIFY(QTest::qWaitForWindowExposed(&viewer));
    QSignalSpy loaded(&viewer, &PdfView::documentReady);
    viewer.open(path);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(viewer.cachedTiles() > 0 && viewer.renderingIdle(), 5000); // scroll bars settled
    auto *area = viewer.viewport();
    double scale = (area->width() - 48) / QPageSize(QPageSize::A4).size(QPageSize::Point).width();
    int y = qRound(24 + 405 * scale - viewer.verticalScrollBar()->value());
    QTest::mousePress(area, Qt::LeftButton, {}, QPoint(30, y));
    QTest::qWait(300); // text extraction runs on the renderer thread
    QTest::mouseMove(area, QPoint(area->width() - 30, y));
    QTest::mouseRelease(area, Qt::LeftButton, {}, QPoint(area->width() - 30, y));
    QCOMPARE(viewer.selectedText(), "Hello wide world");
    QTest::keyClick(&viewer, Qt::Key_C, Qt::ControlModifier);
    QCOMPARE(QGuiApplication::clipboard()->text(), "Hello wide world");
    QPointF word(24 + 80 * scale, y);
    QMouseEvent doubleClick(QEvent::MouseButtonDblClick, word, area->mapToGlobal(word), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(area, &doubleClick);
    QCOMPARE(viewer.selectedText(), "Hello");
    QSignalSpy source(&viewer, &PdfView::sourceRequested);
    QTest::mouseClick(area, Qt::LeftButton, Qt::ControlModifier, QPoint(area->width() / 2, y));
    QCOMPARE(source.size(), 1);
    QCOMPARE(source.first().at(0).toInt(), 1);
    QVERIFY(qAbs(source.first().at(2).toDouble() - 405) < 2);
    QSignalSpy matches(&viewer, &PdfView::matchChanged);
    viewer.find("WIDE", false, Poppler::Page::IgnoreCase);
    QTRY_COMPARE_WITH_TIMEOUT(matches.size(), 1, 5000);
    QCOMPARE(matches.first().at(1).toInt(), 1);
    QCOMPARE(viewer.currentMatch(), 0);
    viewer.find("WIDE", false, Poppler::Page::IgnoreCase); // wraps to the same single match
    QCOMPARE(viewer.currentMatch(), 0);
    viewer.find("WIDE"); // case-sensitive: a new search
    QTRY_COMPARE_WITH_TIMEOUT(matches.size(), 3, 5000);
    QCOMPARE(matches.last().at(1).toInt(), 0);
}

void Tests::findReplace()
{
    Editor editor;
    editor.setPlainText("foo bar Foo baz foo");
    QVERIFY(editor.findText("foo"));
    QCOMPARE(editor.textCursor().selectionStart(), 0);
    QVERIFY(editor.replaceNext("foo", "x"));
    QCOMPARE(editor.toPlainText(), "x bar Foo baz foo");
    QCOMPARE(editor.textCursor().selectionStart(), 6); // next match selected, case-insensitive
    QVERIFY(editor.findText("foo", QTextDocument::FindBackward)); // wraps backwards to the last match
    QCOMPARE(editor.textCursor().selectionStart(), 14);
    QCOMPARE(editor.replaceAll("foo", "foofoo"), 2);
    QCOMPARE(editor.toPlainText(), "x bar foofoo baz foofoo");
    editor.undo();
    QCOMPARE(editor.toPlainText(), "x bar Foo baz foo");
    QVERIFY(!editor.findText("absent"));
    editor.setPlainText("Foo foo food");
    int current = 0;
    QCOMPARE(editor.countMatches("foo", {}, &current), 3);
    QCOMPARE(current, -1);
    QCOMPARE(editor.countMatches("foo", QTextDocument::FindCaseSensitively | QTextDocument::FindWholeWords), 1);
    QVERIFY(editor.findText("foo", QTextDocument::FindCaseSensitively));
    QCOMPARE(editor.countMatches("foo", QTextDocument::FindCaseSensitively, &current), 2);
    QCOMPARE(current, 0);
    // A case-sensitive replace leaves a selected "Foo" alone.
    auto cursor = editor.textCursor();
    cursor.setPosition(0);
    cursor.setPosition(3, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);
    editor.replaceNext("foo", "x", QTextDocument::FindCaseSensitively);
    QCOMPARE(editor.toPlainText(), "Foo foo food");

    Window window;
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.currentEditor()->setFocus();
    QTest::keyClick(window.currentEditor(), Qt::Key_H, Qt::ControlModifier);
    auto *widget = window.findChild<QWidget *>("findWidget");
    QVERIFY(widget && widget->isVisible());
    // The widget floats inside the editor area, at its top-right corner.
    QRect editorArea(window.currentEditor()->mapTo(&window, QPoint()), window.currentEditor()->size());
    QRect area(widget->mapTo(&window, QPoint()), widget->size());
    QVERIFY(editorArea.contains(area));
    QVERIFY(editorArea.right() - area.right() < 40);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
    QVERIFY(!widget->isVisible());
    window.pdfView()->setFocus();
    QTest::keyClick(window.pdfView(), Qt::Key_F, Qt::ControlModifier);
    auto *pdfBar = window.findChild<QWidget *>("pdfFindBar");
    QVERIFY(pdfBar && pdfBar->isVisible() && !widget->isVisible());
}

void Tests::wordWrap()
{
    Window window;
    window.resize(900, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *first = window.currentEditor();
    first->setPlainText(QString("word ").repeated(400));
    QTRY_VERIFY(first->horizontalScrollBar()->maximum() > 0);
    QAction *wrap = nullptr;
    for (auto *action : window.actions()) if (action->shortcut() == QKeySequence("Alt+Z")) wrap = action;
    QVERIFY(wrap && wrap->isCheckable());
    wrap->trigger();
    QVERIFY(window.wordWrap() && first->wordWrap());
    QTRY_COMPARE(first->horizontalScrollBar()->maximum(), 0);
    QTRY_VERIFY(first->document()->documentLayout()->documentSize().height() > 200);
    QTest::keyClick(first, Qt::Key_N, Qt::ControlModifier);
    QVERIFY(window.currentEditor() != first && window.currentEditor()->wordWrap());
    wrap->trigger();
    QVERIFY(!first->wordWrap() && !window.currentEditor()->wordWrap());
}

void Tests::kineticScroll()
{
    Editor editor;
    editor.resize(600, 300);
    editor.setPlainText(QString("test line\n").repeated(2000));
    editor.show();
    QTest::qWait(30);
    auto *bar = editor.verticalScrollBar();
    auto send = [&](QPoint pixels, Qt::ScrollPhase phase, ulong time)
    {
        QWheelEvent event(QPointF(30, 30), QPointF(30, 30), pixels, QPoint(), Qt::NoButton, Qt::NoModifier, phase, false);
        event.setTimestamp(time);
        QApplication::sendEvent(editor.viewport(), &event);
    };
    // A quick flick: five 20 px steps 10 ms apart, fingers lifted right after.
    send({}, Qt::ScrollBegin, 1000);
    for (int i = 0; i < 5; ++i) send(QPoint(0, -20), Qt::ScrollUpdate, 1010 + 10 * i);
    QCOMPARE(bar->value(), 100);
    send({}, Qt::ScrollEnd, 1058);
    QTRY_VERIFY_WITH_TIMEOUT(bar->value() > 300, 1000);
    // Fingers rested before lifting: no inertia.
    send({}, Qt::ScrollBegin, 5000);
    int start = bar->value();
    send(QPoint(0, -20), Qt::ScrollUpdate, 5010);
    send({}, Qt::ScrollEnd, 5300);
    QTest::qWait(150);
    QCOMPARE(bar->value(), start + 20);
}

void Tests::touchpadPixels()
{
    Editor editor;
    editor.resize(600, 300);
    editor.setPlainText(QString("test line\n").repeated(500));
    editor.show();
    QTest::qWait(30);
    auto *bar = editor.verticalScrollBar();
    QVERIFY(bar->maximum() > 300);
    QWheelEvent event(QPointF(30, 30), QPointF(30, 30), QPoint(0, -17), QPoint(), Qt::NoButton,
        Qt::NoModifier, Qt::ScrollUpdate, false);
    QApplication::sendEvent(editor.viewport(), &event);
    QCOMPARE(bar->value(), 17);
}

void Tests::wheelAnimation()
{
    Editor editor;
    editor.resize(600, 300);
    editor.setPlainText(QString("test line\n").repeated(500));
    editor.show(); QTest::qWait(30);
    auto *bar = editor.verticalScrollBar();
    QWheelEvent event(QPointF(30, 30), QPointF(30, 30), QPoint(), QPoint(0, -120), Qt::NoButton,
        Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(editor.viewport(), &event);
    QCOMPARE(bar->value(), 0);
    QTest::qWait(55);
    QVERIFY(bar->value() > 0 && bar->value() < 72);
    QTRY_COMPARE_WITH_TIMEOUT(bar->value(), 72, 600);
    // Qt on Wayland sends both deltas for a wheel notch; the notch must use the wheel step, not 15 px.
    QWheelEvent wayland(QPointF(30, 30), QPointF(30, 30), QPoint(0, -15), QPoint(0, -120), Qt::NoButton,
        Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(editor.viewport(), &wayland);
    QTRY_COMPARE_WITH_TIMEOUT(bar->value(), 144, 600);
}

void Tests::tiledPdf()
{
    QTemporaryDir dir;
    QString path = dir.filePath("sample.pdf");
    {
        QPdfWriter writer(path);
        writer.setResolution(72);
        QPainter painter(&writer);
        for (int i = 0; i < 8; ++i)
        {
            painter.drawText(QPointF(60, 80), QString("Page %1: Unicode and vector text.").arg(i + 1));
            if (i < 7) writer.newPage();
        }
    }
    PdfView viewer;
    viewer.resize(700, 650);
    viewer.show();
    QSignalSpy loaded(&viewer, &PdfView::documentReady);
    viewer.open(path);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 1, 5000);
    QCOMPARE(viewer.pageCount(), 8);
    QTRY_VERIFY_WITH_TIMEOUT(viewer.cachedTiles() > 0, 5000);
    QVERIFY(viewer.cacheKiB() <= 32 * 1024);
    viewer.goToPage(7);
    QCOMPARE(viewer.currentPage(), 7);
    for (double zoom : {0.5, 1.0, 2.0, 4.0})
    {
        viewer.setZoom(zoom);
        QTest::qWait(80);
        QVERIFY(viewer.cacheKiB() <= 32 * 1024);
    }
    viewer.open(path);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 2, 5000);
    QCOMPARE(viewer.pageCount(), 8);
}

void Tests::compilation()
{
    if (QStandardPaths::findExecutable("latexmk").isEmpty()) QSKIP("latexmk unavailable");
    QTemporaryDir dir(QDir::tempPath() + "/folitex file spaces-XXXXXX");
    QVERIFY(dir.isValid());
    QString path = dir.filePath("main file.tex");
    write(path, "\\documentclass{article}\n\\begin{document}\nHello $x^2+1$.\n\\end{document}\n");
    BuildRunner runner;
    QSignalSpy done(&runner, &BuildRunner::finished);
    QVERIFY(runner.start(path, "pdfLaTeX"));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 30000);
    QVERIFY(done.at(0).at(0).toBool());
    QVERIFY(QFileInfo::exists(BuildRunner::pdfPath(path)));
    QVERIFY(QFileInfo::exists(QFileInfo(path).absolutePath() + "/.folitex-build/main file.synctex.gz"));
    QProcess synctex;
    synctex.start("synctex", {"view", "-i", "3:0:" + path, "-o", BuildRunner::pdfPath(path)});
    QVERIFY(synctex.waitForFinished(5000));
    QString output = QString::fromUtf8(synctex.readAllStandardOutput());
    QVERIFY(output.contains("Page:1"));
    auto x = QRegularExpression(R"(^x:([\d.eE+\-]+))", QRegularExpression::MultilineOption).match(output);
    auto y = QRegularExpression(R"(^y:([\d.eE+\-]+))", QRegularExpression::MultilineOption).match(output);
    QVERIFY(x.hasMatch() && y.hasMatch());
    QString point = "1:" + x.captured(1) + ':' + y.captured(1) + ':' + BuildRunner::pdfPath(path);
    synctex.start("synctex", {"edit", "-o", point});
    QVERIFY(synctex.waitForFinished(5000));
    QString back = QString::fromUtf8(synctex.readAllStandardOutput());
    QVERIFY(back.contains("main file.tex") && back.contains("Line:3"));
}

void Tests::utf8AndInvalidInput()
{
    QTemporaryDir dir;
    auto path = dir.filePath("unicode.tex");
    write(path, QString("\\section{Đạo hàm và tích phân}\n").toUtf8());
    Window window;
    QVERIFY(window.openSource(path));
    QVERIFY(window.currentEditor()->toPlainText().contains("Đạo hàm"));
    write(dir.filePath("invalid.tex"), QByteArray("\xff\xfe\x80", 3));
    QVERIFY(!window.openSource(dir.filePath("invalid.tex")));
    for (const QByteArray &tail : {QByteArray::fromHex("41e282"), QByteArray::fromHex("41c3"), QByteArray::fromHex("41f09f92")})
    {
        write(dir.filePath("truncated.tex"), tail);
        QVERIFY(!window.openSource(dir.filePath("truncated.tex")));
    }
    write(dir.filePath("unicode-separator.tex"), QString("A\u2028B").toUtf8());
    QVERIFY(!window.openSource(dir.filePath("unicode-separator.tex")));
    QVERIFY(window.currentEditor()->toPlainText().contains("Đạo hàm"));
}

void Tests::pdfErrors()
{
    QTemporaryDir dir;
    QString path = dir.filePath("invalid.pdf");
    write(path, "not a PDF");
    PdfView viewer;
    QSignalSpy failed(&viewer, &PdfView::loadFailed);
    viewer.open(path);
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 5000);
    QCOMPARE(viewer.pageCount(), 0);
}

void Tests::sourceRoundtrip()
{
    QTemporaryDir dir;
    QString path = dir.filePath("unicode-bom.tex");
    QByteArray original = QByteArray::fromHex("efbbbf") + QString("a\u00a0b\r\n").toUtf8();
    write(path, original);
    Window window;
    QVERIFY(window.openSource(path));
    auto *editor = window.currentEditor();
    QVERIFY(editor->document()->toRawText().contains(QChar(0x00a0)));
    auto cursor = editor->textCursor(); cursor.movePosition(QTextCursor::End); cursor.insertText("% saved");
    bool saved = false;
    for (auto *action : window.actions())
        if (action->shortcut() == QKeySequence::Save) { action->trigger(); saved = true; break; }
    QVERIFY(saved);
    QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), original + "% saved");
}

void Tests::immediateCancellation()
{
#ifdef Q_OS_UNIX
    QTemporaryDir dir;
    QString command = dir.filePath("latexmk");
    write(command, "#!/bin/sh\nsleep 20\n");
    QVERIFY(QFile::setPermissions(command, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    QString source = dir.filePath("cancel.tex"); write(source, "cancel test");
    QByteArray oldPath = qgetenv("PATH");
    auto restore = qScopeGuard([&] { qputenv("PATH", oldPath); });
    qputenv("PATH", dir.path().toUtf8() + ':' + oldPath);
    BuildRunner runner;
    QSignalSpy finished(&runner, &BuildRunner::finished);
    QVERIFY(runner.start(source, "pdfLaTeX"));
    runner.stop();
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
    QVERIFY(!runner.running());
    QVERIFY(runner.start(source, "pdfLaTeX"));
    QTest::qWait(50);
    runner.stop();
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 5000);
    QVERIFY(!runner.running());
#endif
}

void Tests::largePdfViewport()
{
    QTemporaryDir dir;
    QString path = dir.filePath("large-viewport.pdf");
    {
        QPdfWriter writer(path); writer.setResolution(72);
        QPainter painter(&writer); painter.drawText(QPointF(60, 80), "Large viewport, vector text.");
    }
    QCOMPARE(PdfView::renderDensity({1280, 800}, 1.25), 1.25);
    QVERIFY(PdfView::renderDensity({9000, 6000}, 2) < 2);
    QVERIFY(PdfView::renderDensity({9000, 6000}, 2) > 0);
    PdfView viewer;
    viewer.resize(3840, 2160); viewer.show(); viewer.open(path);
    QTRY_VERIFY_WITH_TIMEOUT(viewer.cachedTiles() > 0 && viewer.renderingIdle(), 12000);
    QVERIFY(viewer.cacheLimitKiB() <= 128 * 1024);
    QVERIFY(viewer.cacheKiB() <= viewer.cacheLimitKiB());
    quint64 requests = viewer.renderRequests();
    QTest::qWait(200);
    QCOMPARE(viewer.renderRequests(), requests);
}

void Tests::failedTileDoesNotSpin()
{
    QTemporaryDir dir;
    QString path = dir.filePath("render-failure.pdf");
    { QPdfWriter writer(path); QPainter painter(&writer); painter.drawText(100, 100, "Test"); }
    PdfView viewer;
    viewer.resize(500, 400);
    QSignalSpy requested(&viewer, &PdfView::tileRequested);
    QSignalSpy failed(&viewer, &PdfView::loadFailed);
    viewer.show(); viewer.open(path);
    QTRY_VERIFY_WITH_TIMEOUT(!requested.isEmpty(), 5000);
    Tile tile = qvariant_cast<Tile>(requested.first().at(0));
    QVERIFY(QMetaObject::invokeMethod(&viewer, "tileFinished", Qt::DirectConnection, Q_ARG(Tile, tile), Q_ARG(QImage, QImage())));
    QTRY_VERIFY_WITH_TIMEOUT(!failed.isEmpty() && viewer.renderingIdle(), 5000);
    quint64 attempts = viewer.renderRequests();
    QTest::qWait(200);
    QCOMPARE(viewer.renderRequests(), attempts);
}

void Tests::cancellationCleansOrphanCompiler()
{
#ifdef Q_OS_UNIX
    QTemporaryDir dir;
    QString command = dir.filePath("latexmk");
    QString childFile = dir.filePath("child.pid");
    QByteArray script = "#!/bin/sh\ntrap 'exit 0' TERM\nsh -c 'trap \"\" TERM; exec sleep 20' &\necho $! > \"" + childFile.toUtf8() + "\"\nwait\n";
    write(command, script);
    QVERIFY(QFile::setPermissions(command, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    QString source = dir.filePath("orphan.tex"); write(source, "cancellation test");
    QByteArray oldPath = qgetenv("PATH");
    auto restore = qScopeGuard([&] { qputenv("PATH", oldPath); });
    qputenv("PATH", dir.path().toUtf8() + ':' + oldPath);
    BuildRunner runner;
    QSignalSpy finished(&runner, &BuildRunner::finished);
    QVERIFY(runner.start(source, "pdfLaTeX"));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(childFile), 3000);
    QFile pidFile(childFile); QVERIFY(pidFile.open(QIODevice::ReadOnly));
    int pid = pidFile.readAll().trimmed().toInt(); QVERIFY(pid > 0);
    auto cleanup = qScopeGuard([&] { if (pid > 0) ::kill(pid, SIGKILL); });
    QTest::qWait(50);
    runner.stop();
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 3000);
    auto alive = [pid]
    {
        QFile status(QString("/proc/%1/status").arg(pid));
        if (!status.open(QIODevice::ReadOnly)) return false;
        return !status.readAll().contains("State:\tZ");
    };
    QTRY_VERIFY_WITH_TIMEOUT(!alive(), 4000);
#endif
}

QTEST_MAIN(Tests)
#include "tests.moc"

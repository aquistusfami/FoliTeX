#include "window.h"
#include "palette.h"
#include "agent.h"
#ifdef FOLITEX_TERMINAL
#include <qtermwidget.h>
#endif
#include <QComboBox>
#include <QListWidget>
#include <QToolButton>
#include <QStackedWidget>
#include <QTabBar>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolTip>
#include <QHelpEvent>
#include <QPushButton>
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
#include <QStringListModel>
#include <QTreeWidget>
#include <QLineEdit>
#include <QFileSystemModel>
#include <QClipboard>
#include <QFontDatabase>
#include <QAbstractTextDocumentLayout>
#include <QAbstractItemView>
#include <QFileDialog>
#include <QMessageBox>
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
    void escapedBrackets();
    void completionIgnoresComments();
    void bibliographyRefresh();
    void workspaceTree();
    void externalEditsReload();
    void recreatedFileReloads();
    void externalFormatReload();
    void saveAsOpenTab();
    void pdfFindNavigation();
    void pdfInteraction();
    void pdfBlankSelection();
    void pdfZeroPages();
    void pdfStaleTiles();
    void findReplace();
    void tabIndent();
    void continueList();
    void inverseSearchWord();
    void sessionRestore();
    void projectReferences();
    void mathAndImagePreview();
    void presentation();
    void folding();
    void syntaxColors();
    void buildOnSave();
    void buildSavesAuxiliaryTabs();
    void explorerOperations();
    void fontThemeAndTemplates();
    void undoWithWrapIndent();
    void tabScrolling();
    void packageCompletion();
    void menuBarToggle();
    void buildProblems();
    void logPathsWithParentheses();
    void packageErrorProblems();
    void rebuildKeepsWarnings();
    void wordWrap();
    void kineticScroll();
    void touchpadPixels();
    void wheelAnimation();
    void tiledPdf();
    void navigationHistory();
    void quickOpenAndCommands();
    void selectionBarAndFocusMode();
    void honestEmptyStates();
    void buildFailureEmptyProblemsState();
    void panelMaximizeAndRestore();
    void agentPanel();
    void agentTerminal();
    void darkPdfPages();
    void compilation();
    void buildBoundaryCases_data();
    void buildBoundaryCases();
    void cancellationKeepsUnrelatedProcess();
    void utf8AndInvalidInput();
    void pdfErrors();
    void sourceRoundtrip();
    void mixedNewlinesRejected();
    void immediateCancellation();
    void largePdfViewport();
    void failedTileDoesNotSpin();
    void cancellationCleansOrphanCompiler();
    void selfSaveNoFalseConflict();
    void shortcutGuardsUnfocusedEditor();
    void buildPreparesReferencedSubdirs();
    void inverseSyncRelativePath();
    void presentationPrefetchRespectsCacheBudget();
    void pdfReloadPreservesViewAndSwitchResets();
    void programmaticScrollStopsSmoothScroll();
    void gutterGeometryTracksFontAndLines();
    void fontZoomDoesNotWipeRedo();
    void closingBracketSkipWhenBalanced();
    void insertTemplateLiteralBulletAndMultiline();
    void beamerEnvironmentCompletion();
    void enterAutoIndentCapAndFoldedHeader();
    // --- Task A: editor jumps, selection bar labels, Go to definition ---
    void problemJumpFollowsEdits();
    void goToLineCentresOffscreen();
    void selectionBarLabels();
    void goToDefinition();
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

    // Without a magic comment, a non-document file resolves through whoever \input's it.
    QTemporaryDir project;
    QVERIFY(QDir(project.path()).mkdir("chapters"));
    write(project.filePath("main.tex"), "\\documentclass{article}\n% \\input{chapters/old}\n\\begin{document}\n\\include{chapters/intro}\n\\end{document}\n");
    write(project.filePath("chapters/intro.tex"), "Intro \\input{chapters/part.tex}\n");
    write(project.filePath("chapters/part.tex"), "Part\n");
    write(project.filePath("chapters/old.tex"), "Old\n");
    QCOMPARE(BuildRunner::rootFile(project.filePath("chapters/intro.tex")), project.filePath("main.tex"));
    QCOMPARE(BuildRunner::rootFile(project.filePath("chapters/part.tex")), project.filePath("main.tex"));
    QCOMPARE(BuildRunner::rootFile(project.filePath("chapters/old.tex")), project.filePath("chapters/old.tex"));
    QCOMPARE(BuildRunner::rootFile(project.filePath("main.tex")), project.filePath("main.tex"));
    QVERIFY(QDir(project.path()).mkpath("text/cau"));
    write(project.filePath("text/cau/week.tex"), "Week\n");
    write(project.filePath("main.tex"), "\\documentclass{article}\n\\begin{document}\n\\include{text/cau/week}\n\\end{document}\n");
    QCOMPARE(BuildRunner::rootFile(project.filePath("text/cau/week.tex")), project.filePath("main.tex"));
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
    QCOMPARE(editor.toPlainText(), "\\frac{a}{}  "); // past the last placeholder Tab pads to column 12

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
    QAction *quotes = nullptr;
    for (auto *action : window.actions()) if (action->shortcut() == QKeySequence("Ctrl+'")) quotes = action;
    QVERIFY(quotes);
    source->setPlainText("word");
    source->selectAll();
    quotes->trigger();
    QCOMPARE(source->toPlainText(), "`word'");
    quotes->trigger(); // twice: double quotes
    QCOMPARE(source->toPlainText(), "``word''");
    source->clear();
    quotes->trigger();
    quotes->trigger();
    QTest::keyClicks(source, "x");
    QCOMPARE(source->toPlainText(), "``x''");
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

void Tests::completionIgnoresComments()
{
    Editor editor;
    const QString source = "% \\label{hidden}\n\\label{visible}\n\\% \\label{escapedPercent}\n"
                           "\\\\% \\label{afterLineBreakComment}\n% \\newcommand{\\hiddenmacro}{}\n"
                           "% \\newenvironment{hiddenenvironment}{}{}\n% \\usepackage{physics}\n";
    auto offered = [&](const QString &suffix)
    {
        editor.setPlainText(source + suffix);
        editor.moveCursor(QTextCursor::End);
        QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);
        return static_cast<QStringListModel *>(editor.findChild<QCompleter *>()->model())->stringList();
    };
    const QStringList labels = offered("\\ref{");
    QVERIFY(labels.contains("visible") && labels.contains("escapedPercent"));
    QVERIFY(!labels.contains("hidden") && !labels.contains("afterLineBreakComment"));
    const QStringList commands = offered("\\");
    QVERIFY(!commands.contains("\\hiddenmacro"));
    QVERIFY(!commands.contains("\\dv{\u2022}{\u2022}"));
    QVERIFY(!offered("\\begin{").contains("hiddenenvironment"));
}

void Tests::bibliographyRefresh()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("refs.bib");
    write(path, "@article{oldKey, title={Old}}\n");
    Editor editor;
    editor.setProperty("path", dir.filePath("main.tex"));
    auto offered = [&]
    {
        editor.setPlainText("\\cite{");
        editor.moveCursor(QTextCursor::End);
        QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);
        return static_cast<QStringListModel *>(editor.findChild<QCompleter *>()->model())->stringList();
    };
    QVERIFY(offered().contains("oldKey"));
    const QDateTime changed = QFileInfo(path).lastModified().addSecs(2);
    write(path, "@article{newKey, title={New}}\n");
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(changed, QFileDevice::FileModificationTime));
    file.close();
    const QStringList keys = offered();
    QVERIFY(keys.contains("newKey") && !keys.contains("oldKey"));
    QVERIFY(QFile::remove(path));
    QVERIFY(offered().isEmpty());
}

void Tests::escapedBrackets()
{
    Editor editor;
    editor.setPlainText("{\\}");
    auto cursor = editor.textCursor();
    cursor.setPosition(2);
    editor.setTextCursor(cursor);
    QTest::keyClicks(&editor, "}");
    QCOMPARE(editor.toPlainText(), "{\\}}");
    for (int count = 0; count <= 4; ++count)
    {
        const QString prefix(count, '\\');
        editor.setPlainText(prefix);
        editor.moveCursor(QTextCursor::End);
        QTest::keyClicks(&editor, "{");
        const bool isEscaped = count % 2;
        QCOMPARE(editor.toPlainText(), prefix + (isEscaped ? "{" : "{}"));
        QCOMPARE(editor.extraSelections().size(), isEscaped ? 1 : 3);
        QTest::keyClick(&editor, Qt::Key_Backspace);
        QCOMPARE(editor.toPlainText(), prefix);
        editor.undoText();
        QCOMPARE(editor.toPlainText(), prefix + (isEscaped ? "{" : "{}"));
        editor.redoText();
        QCOMPARE(editor.toPlainText(), prefix);
    }
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
    write(dir.filePath("main.log"), "ignored"); // build clutter stays hidden
    write(dir.filePath(".folitex-build/main.pdf"), "ignored");

    Window window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(!window.filesView()->isVisible());
    window.openWorkspace(dir.path() + "/"); // a trailing slash still names the root folder row
    QVERIFY(window.filesView()->isVisible());
    auto *rootRow = window.findChild<QToolButton *>("rootButton");
    QVERIFY(rootRow);
    QCOMPARE(rootRow->text(), QFileInfo(dir.path()).fileName());
    int rootTop = rootRow->mapTo(&window, QPoint()).y();
    rootRow->click(); // collapses the tree, like VS Code's root folder row
    QVERIFY(!window.filesView()->isVisible());
    QTest::qWait(50); // let the layout settle
    QCOMPARE(rootRow->mapTo(&window, QPoint()).y(), rootTop); // the row stays where it was
    rootRow->click();
    QVERIFY(window.filesView()->isVisible());
    QVERIFY(window.windowTitle().contains("FoliTeX"));
    auto *model = qobject_cast<QFileSystemModel *>(window.filesView()->model());
    auto root = window.filesView()->rootIndex();
    QTRY_COMPARE(model->rowCount(root), 2);
    QStringList names;
    for (int row = 0; row < model->rowCount(root); ++row) names << model->fileName(model->index(row, 0, root));
    names.sort();
    QCOMPARE(names, QStringList({"chapters", "main.tex"}));
    // Collapse Folders is enabled only while a folder is open, and closes them all.
    QToolButton *collapse = nullptr;
    for (auto *button : window.findChildren<QToolButton *>("explorerAction")) if (button->toolTip() == "Collapse Folders") collapse = button;
    QVERIFY(collapse && !collapse->isEnabled());
    QModelIndex chapters = model->index(dir.filePath("chapters"));
    window.filesView()->expand(chapters);
    QVERIFY(collapse->isEnabled());
    QTest::mouseClick(collapse, Qt::LeftButton);
    QVERIFY(!window.filesView()->isExpanded(chapters) && !collapse->isEnabled());
    auto main = model->index(dir.filePath("main.tex"));
    window.filesView()->clicked(main);
    QCOMPARE(window.currentEditor()->property("path").toString(), QFileInfo(dir.filePath("main.tex")).absoluteFilePath());
    QCOMPARE(window.filesView()->currentIndex(), main);
    QVERIFY(window.windowTitle().contains(QFileInfo(dir.path()).fileName()));

    // Two tabs named main.tex show their distinguishing folder, dimmed, after the name.
    write(dir.filePath("chapters/main.tex"), "two");
    QVERIFY(window.openSource(dir.filePath("chapters/main.tex")));
    auto *tabBar = window.findChild<QTabWidget *>("editorTabs")->tabBar();
    auto hint = [&](int index) { auto *label = qobject_cast<QLabel *>(tabBar->tabButton(index, QTabBar::RightSide)); return label ? label->text() : QString(); };
    QCOMPARE(tabBar->count(), 2);
    QCOMPARE(hint(0), QFileInfo(dir.path()).fileName());
    QCOMPARE(hint(1), "chapters");
    tabBar->tabButton(1, QTabBar::LeftSide)->findChild<QToolButton *>()->click();
    QCOMPARE(tabBar->count(), 1);
    QCOMPARE(hint(0), QString()); // no longer ambiguous
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

void Tests::recreatedFileReloads()
{
    QTemporaryDir dir;
    QString path = dir.filePath("main.tex");
    write(path, "\\section{Original}\n");
    Window window;
    QVERIFY(window.openSource(path));
    auto *editor = window.currentEditor();
    QCOMPARE(editor->toPlainText(), QString("\\section{Original}\n"));

    QVERIFY(QFile::remove(path));
    QTest::qWait(300);

    write(path, "\\section{Recreated}\n");
    QTRY_VERIFY_WITH_TIMEOUT(editor->toPlainText().contains("Recreated"), 3000);
}

void Tests::externalFormatReload()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("main.tex");
    write(path, "same\ntext\n");
    Window window;
    QVERIFY(window.openSource(path));
    auto *editor = window.currentEditor();
    const QByteArray changed = QByteArray::fromHex("efbbbf") + "same\r\ntext\r\n";
    write(path, changed);
    QTest::qWait(600);
    QVERIFY(editor->property("bom").toBool());
    QCOMPARE(editor->property("lineEnding").toString(), "\r\n");
    QVERIFY(!editor->document()->isModified());
    editor->moveCursor(QTextCursor::End);
    editor->insertPlainText("mine");
    for (auto *action : window.actions()) if (action->text() == "Save") action->trigger();
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), changed + "mine");
    file.close();
    QCOMPARE(window.recentFiles().first(), path);

    // A detected conflict must still ask even when a writer preserves the timestamp.
    editor->insertPlainText(" local");
    const QDateTime savedTime = editor->property("mtime").toDateTime();
    write(path, "external\n");
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(savedTime, QFileDevice::FileModificationTime));
    file.close();
    QTRY_VERIFY(editor->property("conflict").toBool());
    bool asked = false;
    QTimer timer;
    connect(&timer, &QTimer::timeout, &window, [&]
    {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
        { asked = true; box->done(QMessageBox::No); }
    });
    timer.start(10);
    for (auto *action : window.actions()) if (action->text() == "Save") action->trigger();
    QVERIFY(asked);
    QVERIFY(editor->document()->isModified());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("external\n"));
}

void Tests::saveAsOpenTab()
{
    QTemporaryDir dir;
    const QString first = dir.filePath("first.tex"), second = dir.filePath("second.tex");
    write(first, "first\n");
    write(second, "second\n");
    Window window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.openSource(second));
    auto *other = window.currentEditor();
    other->insertPlainText("unsaved ");
    const QString alias = dir.filePath("alias.tex");
    QVERIFY(QFile::link(second, alias));
    QVERIFY(window.openSource(alias));
    QCOMPARE(window.currentEditor(), other);
    QVERIFY(window.openSource(first));
    auto *editor = window.currentEditor();
    bool blocked = false;
    QTimer timer;
    connect(&timer, &QTimer::timeout, &window, [&]
    {
        auto *modal = QApplication::activeModalWidget();
        if (auto *dialog = qobject_cast<QFileDialog *>(modal))
        {
            dialog->setOption(QFileDialog::DontConfirmOverwrite);
            if (auto *name = dialog->findChild<QLineEdit *>("fileNameEdit")) name->setText(alias);
            QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
        }
        else if (auto *box = qobject_cast<QMessageBox *>(modal))
        { blocked = box->text().contains("already open"); box->done(QMessageBox::Ok); }
    });
    timer.start(10);
    for (auto *action : window.actions()) if (action->text() == "Save as…") action->trigger();
    QVERIFY(blocked);
    QCOMPARE(editor->property("path").toString(), first);
    QCOMPARE(other->toPlainText(), "unsaved second\n");
    QFile file(second);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("second\n"));
}

void Tests::pdfFindNavigation()
{
    QTemporaryDir dir;
    QString path = dir.filePath("matches.pdf");
    {
        QPdfWriter writer(path);
        writer.setResolution(72);
        QPainter painter(&writer);
        for (int page = 0; page < 3; ++page)
        {
            if (page) writer.newPage();
            painter.drawText(QPointF(60, 100), "needle needle");
        }
    }
    Window window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *viewer = window.pdfView();
    QAction *toggle = nullptr, *previous = nullptr, *next = nullptr;
    for (auto *action : window.actions())
    {
        if (action->text() == "Toggle PDF") toggle = action;
        if (action->text() == "Find previous") previous = action;
        if (action->text() == "Find next") next = action;
    }
    QVERIFY(toggle && previous && next);
    toggle->trigger();
    QVERIFY(!viewer->isVisible());
    window.openPdf(path);
    QVERIFY(viewer->isVisible());
    QTRY_COMPARE(viewer->pageCount(), 3);
    viewer->goToPage(2);
    QSignalSpy matches(viewer, &PdfView::matchChanged);
    viewer->find("needle", true, Poppler::Page::IgnoreCase);
    QTRY_COMPARE(matches.size(), 1);
    QCOMPARE(matches.last().at(1).toInt(), 6);
    QCOMPARE(viewer->currentMatch(), 3); // last match on the current page
    viewer->setFocus();
    QTest::keyClick(viewer, Qt::Key_F, Qt::ControlModifier);
    auto *input = window.findChild<QWidget *>("pdfFindBar")->findChild<QLineEdit *>();
    QVERIFY(input);
    input->setText("needle");
    previous->trigger();
    QCOMPARE(viewer->currentMatch(), 2);
    next->trigger();
    QCOMPARE(viewer->currentMatch(), 3);
    viewer->find("");
    QCOMPARE(viewer->currentMatch(), -1);
    QCOMPARE(matches.last().at(1).toInt(), -1);
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
    QStringList words = source.first().at(3).toStringList();
    int clicked = source.first().at(4).toInt();
    QVERIFY(clicked >= 0 && clicked < words.size());
    QCOMPARE(words.join(' '), "Hello wide world");
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

void Tests::pdfBlankSelection()
{
    QTemporaryDir dir;
    QString path = dir.filePath("blank-selection.pdf");
    {
        QPdfWriter writer(path);
        writer.setResolution(72);
        QPainter painter(&writer);
        painter.drawRect(QRectF(60, 60, 100, 100)); // an image-only page has no selectable words
    }
    PdfView viewer;
    viewer.resize(700, 900);
    viewer.show();
    viewer.open(path);
    QTRY_VERIFY_WITH_TIMEOUT(viewer.cachedTiles() > 0 && viewer.renderingIdle(), 5000);
    auto *area = viewer.viewport();
    QSignalSpy requested(&viewer, &PdfView::textRequested);
    QTest::mouseMove(area, QPoint(100, 100));
    QTRY_COMPARE(requested.size(), 1);
    QTest::qWait(300); // extraction is queued on the renderer thread
    QTest::mouseDClick(area, Qt::LeftButton, {}, QPoint(100, 100));
    QVERIFY(viewer.selectedText().isEmpty());
    QVERIFY(!area->grab().isNull()); // painting an empty selection must not index word -1
    QTest::mousePress(area, Qt::LeftButton, {}, QPoint(100, 100));
    QTest::mouseMove(area, QPoint(200, 100));
    QTest::mouseRelease(area, Qt::LeftButton, {}, QPoint(200, 100));
    QVERIFY(viewer.selectedText().isEmpty());
    QVERIFY(!area->grab().isNull());
}

void Tests::pdfZeroPages()
{
    QTemporaryDir dir;
    QString path = dir.filePath("zero-pages.pdf");
    QByteArray data = "%PDF-1.4\n";
    int catalog = data.size();
    data += "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";
    int pages = data.size();
    data += "2 0 obj\n<< /Type /Pages /Kids [] /Count 0 >>\nendobj\n";
    int xref = data.size();
    data += "xref\n0 3\n0000000000 65535 f \n";
    data += QByteArray::number(catalog).rightJustified(10, '0') + " 00000 n \n";
    data += QByteArray::number(pages).rightJustified(10, '0') + " 00000 n \n";
    data += "trailer\n<< /Size 3 /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
    write(path, data);
    PdfView viewer;
    QSignalSpy failed(&viewer, &PdfView::loadFailed);
    QSignalSpy loaded(&viewer, &PdfView::documentReady);
    viewer.open(path);
    QTRY_VERIFY_WITH_TIMEOUT(!failed.isEmpty() || !loaded.isEmpty(), 5000);
    QCOMPARE(failed.size(), 1);
    QVERIFY(loaded.isEmpty());
    QCOMPARE(viewer.pageCount(), 0);
}

void Tests::pdfStaleTiles()
{
    QTemporaryDir dir;
    QString path = dir.filePath("many-pages.pdf");
    {
        QPdfWriter writer(path);
        writer.setResolution(72);
        QPainter painter(&writer);
        for (int page = 0; page < 240; ++page)
        {
            if (page) QVERIFY(writer.newPage());
            painter.drawRect(QRectF(60, 60, 100, 100));
        }
    }
    PdfView viewer;
    viewer.resize(700, 650);
    QSignalSpy loaded(&viewer, &PdfView::documentReady);
    QSignalSpy requested(&viewer, &PdfView::tileRequested);
    viewer.show();
    viewer.open(path);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 1, 5000);
    QCOMPARE(viewer.pageCount(), 240);
    QTRY_VERIFY_WITH_TIMEOUT(viewer.cachedTiles() > 0 && viewer.renderingIdle(), 5000);
    QVERIFY(!requested.isEmpty());
    Tile obsolete = qvariant_cast<Tile>(requested.last().first());
    viewer.goToPage(240);
    QCOMPARE(viewer.currentPage(), 240);
    for (double zoom : {0.5, 4.0, 2.0, 1.0}) viewer.setZoom(zoom);
    viewer.open(path);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 2, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(viewer.cachedTiles() > 0 && viewer.renderingIdle(), 5000);
    QVERIFY(viewer.cacheLimitKiB() <= 128 * 1024);
    QVERIFY(viewer.cacheKiB() <= viewer.cacheLimitKiB());
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(Qt::red);
    const int count = viewer.cachedTiles(), cost = viewer.cacheKiB();
    QVERIFY(QMetaObject::invokeMethod(&viewer, "tileFinished", Qt::DirectConnection, Q_ARG(Tile, obsolete), Q_ARG(QImage, image)));
    QCOMPARE(viewer.cachedTiles(), count);
    QCOMPARE(viewer.cacheKiB(), cost);
    Tile wrongDpi = qvariant_cast<Tile>(requested.last().first());
    ++wrongDpi.dpi1000;
    QVERIFY(QMetaObject::invokeMethod(&viewer, "tileFinished", Qt::DirectConnection, Q_ARG(Tile, wrongDpi), Q_ARG(QImage, image)));
    QCOMPARE(viewer.cachedTiles(), count);
    QCOMPARE(viewer.cacheKiB(), cost);
}

void Tests::rebuildKeepsWarnings()
{
    // A successful build with warnings, then F5 again without changes: latexmk does nothing and
    // keeps the old log, which must still be read.
    if (QStandardPaths::findExecutable("latexmk").isEmpty()) QSKIP("latexmk unavailable");
    QTemporaryDir dir;
    QString main = dir.filePath("main.tex");
    write(main, "\\documentclass{article}\n\\begin{document}\nSee \\ref{missing}.\n\\end{document}\n");
    Window window;
    window.show();
    QVERIFY(window.openSource(main));
    QAction *build = nullptr;
    for (auto *action : window.actions()) if (action->shortcut() == QKeySequence("F5")) build = action;
    auto *button = window.findChild<QPushButton *>("editorAction");
    auto *view = window.findChild<QTreeWidget *>("problems");
    for (int run = 0; run < 2; ++run)
    {
        if (run) QTest::qWait(1500); // a later F5, as people press it
        build->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(!button->toolTip().startsWith("Stop"), 60000);
        QCOMPARE(view->topLevelItemCount(), 1); // main.tex with its warnings
        QVERIFY(view->topLevelItem(0)->childCount() > 0);
    }
}

void Tests::packageErrorProblems()
{
    // An error raised while a package loads is reported in the system .sty; it must land on
    // the project file that loaded the package, not open the system file.
    QTemporaryDir project, system;
    write(project.filePath("main.tex"), "x");
    write(system.filePath("pkg.sty"), "x");
    QString log = QString("(%1\n(%2\n%2:1005: Package keyval Error: bogus undefined.\n)\n)\n")
        .arg(project.filePath("main.tex"), system.filePath("pkg.sty"));
    auto problems = BuildRunner::parseLog(log, project.path());
    QCOMPARE(problems.size(), 1);
    QCOMPARE(QFileInfo(problems[0].file), QFileInfo(project.filePath("main.tex")));
    QCOMPARE(problems[0].line, 0);
    QVERIFY(problems[0].error && problems[0].message.startsWith("pkg.sty: Package keyval Error"));
}

void Tests::logPathsWithParentheses()
{
    QTemporaryDir project;
    QString main = project.filePath("main (draft).tex");
    QString chapter = project.filePath("chapter (draft).tex");
    write(main, "\\documentclass{article}\n");
    write(chapter, "See \\ref{missing}.\n");
    const QString warning = "LaTeX Warning: Reference `missing' on page 1 undefined on input line 1.\n";
    QString log = "(./main (draft).tex\n(./chapter (draft).tex\n" + warning + warning
        + ")\nLaTeX Warning: There were undefined references.\n)\n";
    const auto problems = BuildRunner::parseLog(log, project.path());
    QCOMPARE(problems.size(), 2);
    QCOMPARE(problems[0].file, chapter);
    QCOMPARE(problems[0].line, 1);
    QCOMPARE(problems[1].file, main);
    QCOMPARE(problems[1].line, 0);
}

void Tests::buildProblems()
{
    if (QStandardPaths::findExecutable("latexmk").isEmpty()) QSKIP("latexmk unavailable");
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkpath("sections"));
    QString main = dir.filePath("main.tex"), results = dir.filePath("sections/results.tex");
    write(main, "\\documentclass{article}\n\\usepackage{graphicx}\n\\begin{document}\nSee Table~\\ref{tab:missing}.\n"
                "\\input{sections/results}\n\\noindent\\hbox to 2\\textwidth{Too wide}\n\\end{document}\n");
    write(results, "Results here.\n\\tabel{oops}\nMore text.\n\\begin{itemize}\n\\end{enumerate}\n");
    BuildRunner runner;
    QSignalSpy done(&runner, &BuildRunner::finished);
    QVERIFY(runner.start(main, "pdfLaTeX"));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 60000);
    QVERIFY(!done.at(0).at(0).toBool());
    QFile logFile(dir.filePath(".folitex-build/main.log"));
    QVERIFY(logFile.open(QIODevice::ReadOnly));
    auto problems = BuildRunner::parseLog(QString::fromUtf8(logFile.readAll()), dir.path());
    auto find = [&](const QString &file, int line, bool error, const QString &text)
    {
        return std::any_of(problems.cbegin(), problems.cend(), [&](const Problem &p)
        {
            return QFileInfo(p.file) == QFileInfo(file) && p.line == line && p.error == error && p.message.contains(text);
        });
    };
    QString all;
    for (const auto &p : problems) all += QString("%1:%2 %3 %4\n").arg(p.file).arg(p.line).arg(p.error ? "E" : "W", p.message);
    QVERIFY2(find(results, 2, true, "Undefined control sequence.  \\tabel"), qPrintable(all));
    QVERIFY2(find(results, 5, true, "ended by \\end{enumerate}"), qPrintable(all));
    // Warnings carry no file name; it comes from the file the log has open.
    QVERIFY2(find(main, 4, false, "Reference `tab:missing'"), qPrintable(all));
    QVERIFY2(find(main, 6, false, "Overfull \\hbox (345.0pt too wide)"), qPrintable(all));
    QVERIFY2(std::none_of(problems.cbegin(), problems.cend(), [](const Problem &p) { return !p.file.startsWith(QDir::tempPath()); }), qPrintable(all));

    // In the window: a failed build opens Problems, F8 walks them, editors show squiggles.
    Window window;
    window.resize(1100, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.openSource(main));
    QAction *build = nullptr, *next = nullptr;
    for (auto *action : window.actions())
    {
        if (action->shortcut() == QKeySequence("F5")) build = action;
        if (action->shortcut() == QKeySequence("F8")) next = action;
    }
    QVERIFY(build && next);
    build->trigger();
    auto *panel = window.findChild<QTabWidget *>("bottomPanel");
    auto *view = window.findChild<QTreeWidget *>("problems");
    QTRY_VERIFY_WITH_TIMEOUT(panel->isVisible() && view->topLevelItemCount() == 2, 60000);
    QCOMPARE(panel->currentWidget(), view);
    // The failed build still wrote a PDF; it is shown.
    QCOMPARE(QFileInfo(window.pdfView()->path()), QFileInfo(BuildRunner::pdfPath(main)));
    // The filter keeps matching problems (or every problem of a matching file).
    auto *filter = window.findChild<QLineEdit *>("problemFilter");
    filter->setText("tabel");
    int shown = 0;
    for (QTreeWidgetItemIterator it(view); *it; ++it) shown += (*it)->parent() && !(*it)->isHidden();
    QCOMPARE(shown, 1);
    filter->clear();
    next->trigger(); // the first problem: main.tex line 4 (warnings and errors keep log order)
    QCOMPARE(window.currentEditor()->property("path").toString(), QFileInfo(main).absoluteFilePath());
    QCOMPARE(window.currentEditor()->lineNumber(), 4);
    next->trigger();
    QCOMPARE(QFileInfo(window.currentEditor()->property("path").toString()), QFileInfo(results));
    QCOMPARE(window.currentEditor()->lineNumber(), 2);
    auto squiggles = window.currentEditor()->extraSelections();
    QVERIFY(std::any_of(squiggles.cbegin(), squiggles.cend(), [](const QTextEdit::ExtraSelection &s)
    {
        return s.format.underlineStyle() == QTextCharFormat::WaveUnderline && s.cursor.block().blockNumber() == 1;
    }));
}

void Tests::menuBarToggle()
{
    Window window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *menuBar = window.findChild<QMenuBar *>();
    QAction *toggle = nullptr;
    for (auto *action : window.actions()) if (action->text() == "Menu bar") toggle = action;
    QVERIFY(menuBar && toggle && toggle->isCheckable() && toggle->isChecked());
    QVERIFY(menuBar->isVisible());
    toggle->trigger();
    QVERIFY(!window.menuBarShown() && !menuBar->isVisible());
    auto *editor = window.currentEditor();
    editor->setFocus();
    // A lone Alt reveals the hidden menu bar, a second one hides it again.
    QTest::keyPress(editor, Qt::Key_Alt);
    QTest::keyRelease(editor, Qt::Key_Alt);
    QVERIFY(menuBar->isVisible());
    QTest::keyPress(editor, Qt::Key_Alt);
    QTest::keyRelease(editor, Qt::Key_Alt);
    QVERIFY(!menuBar->isVisible());
    // Alt as a modifier (Alt+Z) does not reveal it.
    QTest::keyPress(editor, Qt::Key_Alt);
    QTest::keyClick(editor, Qt::Key_Z, Qt::AltModifier);
    QTest::keyRelease(editor, Qt::Key_Alt);
    QVERIFY(!menuBar->isVisible());
    // Using an item of the revealed menu hides it again.
    QTest::keyPress(editor, Qt::Key_Alt);
    QTest::keyRelease(editor, Qt::Key_Alt);
    QVERIFY(menuBar->isVisible());
    // A shortcut of a menu item (here Alt+Z) is not a menu choice; the bar stays.
    bool wrap = window.wordWrap();
    QTest::keyClick(editor, Qt::Key_Z, Qt::AltModifier);
    QCOMPARE(window.wordWrap(), !wrap);
    QVERIFY(menuBar->isVisible());
    emit menuBar->triggered(toggle);
    QVERIFY(!menuBar->isVisible());
    window.setMenuBarShown(true);
    QVERIFY(menuBar->isVisible() && toggle->isChecked());
}

void Tests::packageCompletion()
{
    Editor editor;
    editor.show();
    auto *completer = editor.findChild<QCompleter *>();
    auto offered = [&](const QString &text)
    {
        editor.setPlainText(text);
        editor.moveCursor(QTextCursor::End);
        QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);
        return static_cast<QStringListModel *>(completer->model())->stringList();
    };
    // Curated words appear only once the package is loaded.
    QVERIFY(!offered("\\documentclass{article}\n\\q").contains("\\qty{•}{•}"));
    QVERIFY(offered("\\documentclass{article}\n\\usepackage{amsmath,siunitx}\n\\q").contains("\\qty{•}{•}"));
    QVERIFY(offered("\\documentclass{beamer}\n\\begin{").contains("alertblock{•}"));
    // Packages without curated data contribute their \newcommand-style definitions.
    if (QStandardPaths::findExecutable("kpsewhich").isEmpty()) QSKIP("TeX Live is not installed");
    QStringList cancel = offered("\\usepackage{cancel}\n\\canc");
    QVERIFY(std::any_of(cancel.cbegin(), cancel.cend(), [](const QString &word) { return word.startsWith("\\cancelto"); }));
}

void Tests::tabScrolling()
{
    QTemporaryDir dir;
    Window window;
    window.resize(900, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    for (int i = 0; i < 8; ++i)
    {
        QString path = dir.filePath(QString("chapter-with-a-long-name-%1.tex").arg(i));
        write(path, "x");
        QVERIFY(window.openSource(path));
    }
    auto *bar = window.findChild<QTabWidget *>("editorTabs")->tabBar();
    // Names stay whole; the bar is scrolled so the newest (current) tab is visible.
    QVERIFY(bar->findChild<QToolButton *>("ScrollRightButton"));
    QTRY_VERIFY(bar->tabRect(0).x() < 0);
    QVERIFY(bar->tabRect(bar->currentIndex()).right() <= bar->width());
    int before = bar->tabRect(0).x();
    QPointF inside(bar->width() / 2.0, bar->height() / 2.0);
    QWheelEvent up(inside, bar->mapToGlobal(inside), {}, QPoint(0, 360), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(bar, &up);
    QTRY_VERIFY(bar->tabRect(0).x() > before);
    QCOMPARE(bar->currentIndex(), 7); // the wheel scrolls, it does not switch tabs
}

void Tests::undoWithWrapIndent()
{
    // Wrap-indent format steps must not trap Ctrl+Z or eat the redo history.
    Editor editor;
    editor.resize(400, 300);
    editor.setWordWrap(true);
    editor.show();
    editor.setPlainText("first line\nsecond");
    QTest::qWait(20);
    editor.moveCursor(QTextCursor::Start);
    QStringList states{editor.toPlainText()};
    QTest::keyClick(&editor, Qt::Key_Tab);
    QTest::qWait(20);
    states << editor.toPlainText();
    QTest::keyClick(&editor, Qt::Key_Tab);
    QTest::qWait(20);
    states << editor.toPlainText();
    editor.moveCursor(QTextCursor::EndOfBlock);
    QTest::keyClick(&editor, Qt::Key_Return);
    QTest::qWait(20);
    states << editor.toPlainText();
    double space = QFontMetricsF(editor.font()).horizontalAdvance(' ');
    for (int i = states.size() - 2; i >= 0; --i)
    {
        QTest::keyClick(&editor, Qt::Key_Z, Qt::ControlModifier);
        QTest::qWait(20);
        QCOMPARE(editor.toPlainText(), states[i]);
    }
    QVERIFY(editor.document()->isRedoAvailable());
    for (int i = 1; i < states.size(); ++i)
    {
        QTest::keyClick(&editor, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
        QTest::qWait(20);
        QCOMPARE(editor.toPlainText(), states[i]);
    }
    // Margins match the text again after the round trip.
    QTRY_VERIFY(qAbs(editor.document()->firstBlock().blockFormat().leftMargin() - 8 * space) < 0.5);
}

void Tests::inverseSearchWord()
{
    // SyncTeX reports \end{frame} (line 7) for every word of a beamer frame.
    Editor editor;
    editor.setPlainText("\\begin{frame}{Intro}\n"
                        "    \\centering Placeholder for ``the part''.\n"
                        "    Second part here, with the part again.\n"
                        "\\end{frame}\n"
                        "\\begin{frame}\n"
                        "    the part\n"
                        "\\end{frame}\n");
    auto at = [&] { return std::pair(editor.textCursor().blockNumber() + 1, editor.textCursor().positionInBlock()); };
    editor.goToWord(4, {"Placeholder", "for", "\u201cthe", "part\u201d."}, 3);
    QCOMPARE(at(), std::pair(2, 37)); // "part" next to "Placeholder for the", not the later ones
    editor.goToWord(4, {"Second", "part", "here,"}, 1);
    QCOMPARE(at(), std::pair(3, 11));
    editor.goToWord(4, {"centering"}, 0); // command names are not text
    QCOMPARE(at(), std::pair(4, 0));
    editor.goToWord(7, {"the", "part"}, 1); // only inside the frame SyncTeX named
    QCOMPARE(at(), std::pair(6, 8));
    editor.goToWord(4, {}, -1);
    QCOMPARE(at(), std::pair(4, 0));
}

void Tests::sessionRestore()
{
    QTemporaryDir dir;
    write(dir.filePath("a.tex"), "first file\nsecond line\n");
    write(dir.filePath("b.tex"), "other");
    QVariantMap session;
    {
        Window window;
        QVERIFY(window.openSource(dir.filePath("a.tex")));
        auto cursor = window.currentEditor()->textCursor();
        cursor.setPosition(14);
        window.currentEditor()->setTextCursor(cursor);
        QVERIFY(window.openSource(dir.filePath("b.tex")));
        QVERIFY(window.openSource(dir.filePath("a.tex"))); // switches back to the open tab
        session = window.session();
        QCOMPARE(window.recentFiles().first(), QFileInfo(dir.filePath("a.tex")).absoluteFilePath());
        QCOMPARE(window.recentFiles().size(), 2);
    }
    QFile::remove(dir.filePath("b.tex")); // a file deleted since is skipped
    write(dir.filePath("c.tex"), "c");
    session["files"] = session["files"].toStringList() << dir.filePath("c.tex");
    Window window;
    window.restoreSession(session);
    auto *tabs = window.findChild<QTabWidget *>("editorTabs");
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(window.currentEditor()->property("path").toString(), QFileInfo(dir.filePath("a.tex")).absoluteFilePath());
    QCOMPARE(window.currentEditor()->textCursor().position(), 14);
}

void Tests::projectReferences()
{
    // Only the root is open; labels and keys come from the files it includes.
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkpath("sections") && QDir(dir.path()).mkpath("refs"));
    write(dir.filePath("main.tex"), "\\documentclass{article}\n\\addbibresource{refs/lib.bib}\n\\begin{document}\n"
                                    "\\input{sections/one}\n% \\input{ghost}\n\\end{document}\n");
    write(dir.filePath("sections/one.tex"), "\\section{One}\\label{sec:one}\n\\include{sections/two}\n");
    write(dir.filePath("sections/two.tex"), "\\label{sec:two}\n");
    write(dir.filePath("ghost.tex"), "\\label{sec:ghost}\n");
    write(dir.filePath("refs/lib.bib"), "@article{knuth84,\n  title={Literate programming}\n}\n");
    Window window;
    window.show();
    QVERIFY(window.openSource(dir.filePath("main.tex")));
    auto *editor = window.currentEditor();
    auto *completer = editor->findChild<QCompleter *>();
    auto offered = [&](const QString &text)
    {
        editor->moveCursor(QTextCursor::End);
        editor->insertPlainText(text);
        QTest::keyClick(editor, Qt::Key_Space, Qt::ControlModifier);
        return static_cast<QStringListModel *>(completer->model())->stringList();
    };
    QStringList labels = offered("\\ref{");
    QVERIFY(labels.contains("sec:one") && labels.contains("sec:two"));
    QVERIFY(!labels.contains("sec:ghost")); // commented out
    QVERIFY(offered("}\n\\cite{").contains("knuth84"));
}

void Tests::mathAndImagePreview()
{
    Editor editor;
    editor.setPlainText("Cost \\$5 and $x^2$ or \\(a+b\\) and $$c$$.\n"
                        "\\begin{align*}\n  a &= b \\\\[2pt]\n  c &= d\n\\end{align*}\n"
                        "Text \\[ \\int f \\] end % $not$\n");
    auto at = [&](int line, int column) { return editor.document()->findBlockByNumber(line - 1).position() + column; };
    QCOMPARE(editor.mathAt(at(1, 2)), QString());
    QCOMPARE(editor.mathAt(at(1, 15)), "$x^2$");
    QCOMPARE(editor.mathAt(at(1, 25)), "\\(a+b\\)");
    QCOMPARE(editor.mathAt(at(1, 37)), "\\[c\\]");
    QCOMPARE(editor.mathAt(at(4, 3)), "\\begin{align*}\n  a &= b \\\\[2pt]\n  c &= d\n\\end{align*}");
    QCOMPARE(editor.mathAt(at(6, 9)), "\\[ \\int f \\]");
    QCOMPARE(editor.mathAt(at(6, 20)), QString());
    QCOMPARE(editor.mathAt(at(6, 27)), QString()); // in a comment
    // Figures resolve through \graphicspath; bitmaps need no rendering.
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkpath("figs"));
    QImage(40, 20, QImage::Format_RGB32).save(dir.filePath("figs/plot.png"));
    write(dir.filePath("main.tex"), "\\graphicspath{{figs/}}\n\\includegraphics[width=3cm]{plot}\n");
    Window window;
    QVERIFY(window.openSource(dir.filePath("main.tex")));
    auto *source = window.currentEditor();
    QCOMPARE(source->imageAt(source->document()->findBlockByNumber(1).position() + 30), QDir::cleanPath(dir.filePath("figs/plot.png")));
    QPixmap figure = source->preview(source->document()->findBlockByNumber(1).position() + 30);
    QCOMPARE(figure.deviceIndependentSize(), QSizeF(40, 20));
    QCOMPARE(figure.devicePixelRatio(), source->devicePixelRatioF()); // drawn 1:1 on screen pixels, not blurred
    if (QStandardPaths::findExecutable("pdflatex").isEmpty() || QStandardPaths::findExecutable("pdftocairo").isEmpty())
        QSKIP("pdflatex or pdftocairo is not installed");
    QSignalSpy ready(&editor, &Editor::previewReady);
    QVERIFY(editor.preview(at(1, 15)).isNull()); // renders in the background
    QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 20000);
    QPixmap formula = editor.preview(at(1, 15));
    QVERIFY(!formula.isNull());
    QCOMPARE(formula.devicePixelRatio(), editor.devicePixelRatioF());
}

void Tests::presentation()
{
    QTemporaryDir dir;
    QString path = dir.filePath("slides.pdf");
    {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QSizeF(160, 90), QPageSize::Millimeter));
        QPainter painter(&writer);
        writer.newPage(); writer.newPage();
    }
    PdfView viewer;
    viewer.resize(800, 600);
    viewer.setPresentation(true);
    viewer.goToPage(2); // before loading, as Window::present does
    QSignalSpy loaded(&viewer, &PdfView::documentReady);
    viewer.show();
    QVERIFY(QTest::qWaitForWindowExposed(&viewer));
    viewer.open(path);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 1, 5000);
    QCOMPARE(viewer.pageCount(), 3);
    QCOMPARE(viewer.currentPage(), 2);
    QCOMPARE(viewer.verticalScrollBar()->value(), viewer.viewport()->height()); // one screen per page
    // The pages before and after are rendered ahead, so stepping never shows a blank page.
    QTRY_VERIFY_WITH_TIMEOUT(viewer.renderingIdle() && viewer.cachedTiles() == 3, 5000);
    QTest::keyClick(&viewer, Qt::Key_Right);
    QCOMPARE(viewer.currentPage(), 3);
    QTest::keyClick(&viewer, Qt::Key_Space);
    QCOMPARE(viewer.currentPage(), 3); // stays on the last page
    QTest::mouseClick(viewer.viewport(), Qt::LeftButton);
    QCOMPARE(viewer.currentPage(), 3);
    QTest::keyClick(&viewer, Qt::Key_Home);
    QCOMPARE(viewer.currentPage(), 1);
    QWheelEvent down(QPointF(100, 100), viewer.mapToGlobal(QPointF(100, 100)), {}, QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(viewer.viewport(), &down);
    QCOMPARE(viewer.currentPage(), 2);
    QTest::keyClick(&viewer, Qt::Key_Escape);
    QVERIFY(!viewer.isVisible());
}

void Tests::syntaxColors()
{
    Editor editor;
    editor.setPlainText("\\begin{align}\n"                      // 1
                        "  a &= \\text{if} b % note\n"            // 2
                        "\\end{align}\n"                          // 3
                        "\\section{Title}\\label{key} $x$ \\$\n"   // 4
                        "\\begin{verbatim}\n"                     // 5
                        "\\raw{ $ % \n"                           // 6
                        "\\end{verbatim} \\includegraphics[width=1cm]{fig}\n"); // 7
    auto color = [&](int line, int column)
    {
        auto block = editor.document()->findBlockByNumber(line - 1);
        for (const auto &range : block.layout()->formats())
            if (column >= range.start && column < range.start + range.length) return range.format.foreground().color().name().toUpper();
        return QString("plain");
    };
    auto bold = [&](int line, int column)
    {
        for (const auto &range : editor.document()->findBlockByNumber(line - 1).layout()->formats())
            if (column >= range.start && column < range.start + range.length) return range.format.fontWeight() == QFont::Bold;
        return false;
    };
    QCOMPARE(color(1, 1), "#B294BB");   // \begin
    QCOMPARE(color(1, 8), "#7DC4B4");   // align
    QCOMPARE(color(1, 6), "#C8B466");   // { at depth 0
    QCOMPARE(color(2, 2), "#A8C98A");   // math continues on the next line
    QCOMPARE(color(2, 4), "#CDB483");   // &
    QCOMPARE(color(2, 13), "plain");    // \text{if} is text inside math
    QCOMPARE(color(2, 19), "#6F9080");  // comment
    QCOMPARE(color(3, 6), "#7DC4B4");   // \end{align} ends math
    QCOMPARE(color(4, 2), "#DCDCAA");   // \section: bold, heading colour
    QVERIFY(bold(4, 2) && !bold(4, 10));
    QCOMPARE(color(4, 10), "plain");    // heading text stays plain
    QCOMPARE(color(4, 22), "plain");    // label key stays plain
    QCOMPARE(color(4, 28), "#A8C98A");  // $x$
    QCOMPARE(color(4, 31), "#7FA8C9");  // \$ is an escape, not math
    QCOMPARE(color(6, 1), "plain");     // verbatim stays raw: no command, math or comment
    QCOMPARE(color(6, 9), "plain");
    QCOMPARE(color(7, 6), "#7DC4B4");   // \end{verbatim} itself
    QCOMPARE(color(7, 43), "plain");    // the file after [width=1cm]
    editor.setPlainText("text \\"); // a backslash being typed at the end of a line once hung the scan
    QCOMPARE(color(1, 5), "plain");    // A stray { colours only its paragraph: the blank line resets the depth.
    editor.setPlainText("{ open\n\n{x}");
    QCOMPARE(color(3, 0), "#C8B466");
    QCOMPARE(editor.document()->findBlockByNumber(2).userState() & 0xFF, 0);
    // The hover popup is a window of its own; it hides with its tab.
    QStackedWidget tabs;
    auto *other = new Editor;
    auto *first = new Editor;
    tabs.addWidget(first);
    tabs.addWidget(other);
    tabs.show();
    QVERIFY(QTest::qWaitForWindowExposed(&tabs));
    auto *popup = first->findChild<QLabel *>("preview");
    popup->show();
    tabs.setCurrentWidget(other);
    QVERIFY(!popup->isVisible());
}

void Tests::explorerOperations()
{
    QTemporaryDir dir;
    // The trash goes to a folder of the test, never the user's.
    QVERIFY(QDir(dir.path()).mkpath("data"));
    QByteArray dataHome = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", dir.filePath("data").toUtf8());
    auto restore = qScopeGuard([dataHome] { if (dataHome.isEmpty()) qunsetenv("XDG_DATA_HOME"); else qputenv("XDG_DATA_HOME", dataHome); });
    QString root = QDir::cleanPath(dir.path());
    Window window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.openWorkspace(root);
    auto *model = static_cast<QFileSystemModel *>(window.filesView()->model());
    auto inlineEditor = [&] { return window.filesView()->findChild<QLineEdit *>(); };
    // New file: named inline with the extension left out of the selection, then opened.
    QString created = window.newEntry(root, false);
    QCOMPARE(created, root + "/untitled.tex");
    QTRY_VERIFY(inlineEditor() && inlineEditor()->isVisible());
    QTRY_COMPARE(inlineEditor()->selectedText(), "untitled");
    inlineEditor()->setText("chapter.tex");
    QTest::keyClick(inlineEditor(), Qt::Key_Return);
    QTRY_VERIFY(QFileInfo::exists(root + "/chapter.tex") && !QFileInfo::exists(created));
    QCOMPARE(window.currentEditor()->property("path").toString(), root + "/chapter.tex");
    // Renaming an open file moves its tab along.
    QVERIFY(model->setData(model->index(root + "/chapter.tex"), "intro.tex"));
    QCOMPARE(window.currentEditor()->property("path").toString(), root + "/intro.tex");
    // A new folder; renaming it moves the tabs of the files inside.
    QString folder = window.newEntry(root, true);
    QCOMPARE(folder, root + "/New Folder");
    QTRY_VERIFY(inlineEditor() && inlineEditor()->isVisible());
    QTest::keyClick(inlineEditor(), Qt::Key_Escape);
    write(folder + "/part.tex", "x");
    QVERIFY(window.openSource(folder + "/part.tex"));
    QVERIFY(model->setData(model->index(folder), "parts"));
    QCOMPARE(window.currentEditor()->property("path").toString(), root + "/parts/part.tex");
    // Delete moves to the trash; unmodified tabs close, a tab with unsaved edits stays.
    window.currentEditor()->insertPlainText("unsaved ");
    auto *tabs = window.findChild<QTabWidget *>("editorTabs");
    QCOMPARE(tabs->count(), 2);
    QVERIFY(window.deleteEntry(root + "/intro.tex", false));
    QCOMPARE(tabs->count(), 1);
    QVERIFY(window.deleteEntry(root + "/parts", false));
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(window.currentEditor()->property("path").toString(), root + "/parts/part.tex");
    QVERIFY(!QFileInfo::exists(root + "/intro.tex") && !QFileInfo::exists(root + "/parts"));
    QCOMPARE(QDir(dir.filePath("data/Trash/files")).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size(), 2);
    QVERIFY(!window.deleteEntry(root, false)); // never the workspace itself
}

void Tests::buildOnSave()
{
    if (QStandardPaths::findExecutable("latexmk").isEmpty()) QSKIP("latexmk is not installed");
    QTemporaryDir dir;
    write(dir.filePath("main.tex"), "\\documentclass{article}\n\\begin{document}\nHi\n\\end{document}\n");
    write(dir.filePath("refs.bib"), "@misc{a, title={A}}\n");
    Window window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.setBuildOnSave(true);
    QAction *save = nullptr;
    for (auto *action : window.actions()) if (action->text() == "Save") save = action;
    QVERIFY(save);
    auto status = [&] (const QString &text)
    {
        const auto labels = window.findChildren<QLabel *>();
        return std::any_of(labels.cbegin(), labels.cend(), [&](QLabel *label) { return label->text() == text; });
    };
    auto edit = [&](const QString &path)
    {
        QVERIFY(window.openSource(path));
        window.currentEditor()->moveCursor(QTextCursor::End);
        window.currentEditor()->insertPlainText("%\n");
        save->trigger();
    };
    // A .bib never builds itself; with nothing built yet there is nothing to rebuild.
    edit(dir.filePath("refs.bib"));
    QVERIFY(status("Open the main .tex file and press F5 to build."));
    edit(dir.filePath("main.tex"));
    QVERIFY(status("Building…"));
    QTRY_VERIFY_WITH_TIMEOUT(!status("Building…"), 60000);
    QVERIFY(QFileInfo::exists(dir.filePath(".folitex-build/main.pdf")));
    // Saving the .bib now rebuilds main.tex.
    edit(dir.filePath("refs.bib"));
    QVERIFY(status("Building…"));
    QTRY_VERIFY_WITH_TIMEOUT(!status("Building…"), 60000);
}

void Tests::buildSavesAuxiliaryTabs()
{
    QTemporaryDir dir;
    const QString mainPath = dir.filePath("main.tex");
    const QString styPath = dir.filePath("custom.sty");
    const QString bibPath = dir.filePath("refs.bib");

    write(mainPath, "\\documentclass{article}\n\\usepackage{custom}\n\\bibliography{refs}\n\\begin{document}\nHi\n\\end{document}\n");
    write(styPath, "% original sty\n");
    write(bibPath, "% original bib\n");

    QCOMPARE(BuildRunner::rootFile(styPath), mainPath);
    QCOMPARE(BuildRunner::rootFile(bibPath), mainPath);

    Window window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(window.openSource(mainPath));
    QVERIFY(window.openSource(styPath));
    auto *styEditor = window.currentEditor();
    styEditor->insertPlainText("% edit in sty\n");
    QVERIFY(styEditor->document()->isModified());

    QVERIFY(window.openSource(bibPath));
    auto *bibEditor = window.currentEditor();
    bibEditor->insertPlainText("% edit in bib\n");
    QVERIFY(bibEditor->document()->isModified());

    // Switch back to main.tex
    QVERIFY(window.openSource(mainPath));

    window.build();
    if (window.m_runner.running()) window.m_runner.stop();

    // Verify both auxiliary tabs were saved
    QVERIFY(!styEditor->document()->isModified());
    QVERIFY(!bibEditor->document()->isModified());

    QFile styFile(styPath);
    QVERIFY(styFile.open(QIODevice::ReadOnly));
    QVERIFY(QString::fromUtf8(styFile.readAll()).contains("edit in sty"));

    QFile bibFile(bibPath);
    QVERIFY(bibFile.open(QIODevice::ReadOnly));
    QVERIFY(QString::fromUtf8(bibFile.readAll()).contains("edit in bib"));
}

void Tests::folding()
{
    Editor editor;
    editor.resize(500, 400);
    editor.show();
    editor.setPlainText("\\section{One}\n"                 // 1
                        "\\begin{itemize} % \\end{itemize}\n" // 2: the comment does not close it
                        "  \\item a\n"                       // 3
                        "  \\begin{itemize}\\item x\\end{itemize}\n" // 4: closed on its line
                        "\\end{itemize}\n"                   // 5
                        "\n"                                 // 6
                        "\\subsection{Sub}\n"                // 7
                        "text\n"                             // 8
                        "\n"                                 // 9
                        "\\section{Two}\n"                   // 10
                        "end\n");                            // 11
    auto line = [&](int number) { return editor.document()->findBlockByNumber(number - 1); };
    // foldEnd() gives 0-based block numbers: line 4 is block 3.
    QCOMPARE(editor.foldEnd(line(2)), 3);  // through the line before \end, which stays visible
    QCOMPARE(editor.foldEnd(line(4)), -1);
    QCOMPARE(editor.foldEnd(line(1)), 7);  // the section ends before \section{Two}, blank lines stay
    QCOMPARE(editor.foldEnd(line(7)), 7);  // ...and \subsection{Sub} at its text
    QCOMPARE(editor.foldEnd(line(8)), -1);
    QVERIFY(editor.toggleFold(0)); // block numbers are 0-based: line 1
    QVERIFY(editor.isFolded(0));
    for (int number = 2; number <= 8; ++number) QVERIFY(!line(number).isVisible());
    QVERIFY(line(9).isVisible() && line(10).isVisible());
    QVERIFY(!editor.document()->isModified() && !editor.document()->isUndoAvailable()); // not an edit
    QCOMPARE(editor.toPlainText().count('\n'), 11); // the text is untouched
    // Moving the cursor into folded lines (find, go to line) unfolds them.
    editor.goToLine(3);
    QVERIFY(line(3).isVisible() && !editor.isFolded(0));
    // Fold at the cursor picks the innermost range; unfold all restores everything.
    editor.foldAtCursor();
    QVERIFY(editor.isFolded(1) && !line(3).isVisible() && line(5).isVisible());
    QCOMPARE(editor.textCursor().blockNumber(), 1); // the cursor leaves the hidden lines
    editor.toggleFold(0);
    editor.unfoldAll();
    for (int number = 1; number <= 11; ++number) QVERIFY(line(number).isVisible());
    // Hidden lines whose first line was edited away still unfold from the line above.
    editor.toggleFold(1);
    QTextCursor cursor(line(2));
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText("plain");
    QVERIFY(editor.isFolded(1));
    QVERIFY(editor.toggleFold(1) && line(3).isVisible());
}

void Tests::fontThemeAndTemplates()
{
    Window window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *first = window.currentEditor();
    QFont font("Monospace");
    font.setPointSizeF(15);
    window.setEditorFont(font);
    QCOMPARE(first->font().pointSizeF(), 15.0);
    QCOMPARE(first->tabStopDistance(), QFontMetricsF(first->font()).horizontalAdvance(' ') * 4);
    // Ctrl+wheel resizes the text in every editor, new ones included.
    QPointF middle(100, 100);
    QWheelEvent zoom(middle, first->viewport()->mapToGlobal(middle), {}, QPoint(0, 240), Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(first->viewport(), &zoom);
    QCOMPARE(window.editorFont().pointSizeF(), 17.0);
    // Templates open as a new modified document with the first placeholder selected.
    const auto templates = Window::builtinTemplates();
    auto beamer = std::find_if(templates.cbegin(), templates.cend(), [](const auto &entry) { return entry.first == "Beamer presentation"; });
    QVERIFY(beamer != templates.cend());
    auto *editor = window.newFromTemplate(beamer->second);
    QCOMPARE(editor->font().pointSizeF(), 17.0);
    QVERIFY(editor->toPlainText().startsWith("\\documentclass{beamer}"));
    QVERIFY(!editor->toPlainText().contains(QChar(0x2022)));
    QVERIFY(editor->document()->isModified());
    QVERIFY(editor->textCursor().block().text().startsWith("\\title{"));
    for (const auto &[name, text] : templates)
        QVERIFY2(text.contains("\\begin{document}") && text.endsWith("\\end{document}\n"), qPrintable(name));
    // The light theme maps the dark palette; the dark one leaves colours alone.
    QCOMPARE(themeColor("#16181B"), QColor("#16181B"));
    setLightTheme(true);
    auto reset = qScopeGuard([] { setLightTheme(false); });
    QCOMPARE(themeColor("#16181b"), QColor("#FFFFFF"));
    QCOMPARE(themed("a { background:#16181B; color:#d2d7de; }"), "a { background:#FFFFFF; color:#1F1F1F; }");
    // Every colour in the sources is a key (or a light value) of the light map: an unmapped
    // background stays dark under text that turned dark, as the command palette once did.
    QSet<QString> known;
    QFile map(SOURCE_DIR "/src/editor.cpp");
    QVERIFY(map.open(QIODevice::ReadOnly));
    auto pairs = QRegularExpression(R"rx(\{"(#[0-9A-F]{6})", "(#[0-9A-F]{6})"\})rx").globalMatch(QString::fromUtf8(map.readAll()));
    while (pairs.hasNext()) { auto pair = pairs.next(); known << pair.captured(1) << pair.captured(2); }
    QVERIFY(known.size() > 40);
    for (const QString &name : QDir(SOURCE_DIR "/src").entryList({"*.cpp"})) // SVG resources carry fixed colours and bypass the map
    {
        QFile source(QString(SOURCE_DIR "/src/") + name);
        QVERIFY(source.open(QIODevice::ReadOnly));
        auto colours = QRegularExpression("#[0-9A-Fa-f]{6}\\b").globalMatch(QString::fromUtf8(source.readAll()));
        while (colours.hasNext())
        {
            const QString colour = colours.next().captured().toUpper();
            QVERIFY2(known.contains(colour), qPrintable(name + " uses " + colour + ", which the light theme does not map"));
        }
    }
}

void Tests::continueList()
{
    Editor editor;
    auto shiftEnter = [&] { QTest::keyClick(&editor, Qt::Key_Return, Qt::ShiftModifier); };
    editor.setPlainText("\\begin{itemize}\n  \\item first");
    editor.moveCursor(QTextCursor::End);
    shiftEnter();
    QTest::keyClicks(&editor, "second");
    QCOMPARE(editor.toPlainText(), "\\begin{itemize}\n  \\item first\n  \\item second");
    // From the continuation line of a long item, and with \task (tasks package).
    editor.setPlainText("\\begin{tasks}\n\\task one\n    more of one");
    editor.moveCursor(QTextCursor::End);
    shiftEnter();
    QCOMPARE(editor.toPlainText(), "\\begin{tasks}\n\\task one\n    more of one\n\\task ");
    // Nested lists: after an inner list, the outer list continues.
    editor.setPlainText("\\begin{enumerate}\n  \\item outer\n    \\begin{itemize}\n      \\item inner\n    \\end{itemize}\n    rest of outer");
    editor.moveCursor(QTextCursor::End);
    shiftEnter();
    QVERIFY(editor.toPlainText().endsWith("rest of outer\n  \\item "));
    // Inside the inner list it continues the inner one.
    editor.setPlainText("\\begin{enumerate}\n  \\item outer\n    \\begin{itemize}\n      \\item inner");
    editor.moveCursor(QTextCursor::End);
    shiftEnter();
    QVERIFY(editor.toPlainText().endsWith("\\item inner\n      \\item "));
    // An environment opened and closed on the item's own line does not hide that item.
    editor.setPlainText("\\begin{itemize}\n  \\item see \\begin{math}x\\end{math}");
    editor.moveCursor(QTextCursor::End);
    shiftEnter();
    QVERIFY(editor.toPlainText().endsWith("\\end{math}\n  \\item "));
    // A commented-out \end does not close the list.
    editor.setPlainText("\\begin{itemize}\n  \\item one\n  % \\end{itemize}\n  more");
    editor.moveCursor(QTextCursor::End);
    shiftEnter();
    QVERIFY(editor.toPlainText().endsWith("more\n  \\item "));
    // Right after \begin{itemize}: its first item, one level deeper.
    editor.setPlainText("  \\begin{itemize}");
    editor.moveCursor(QTextCursor::End);
    shiftEnter();
    QCOMPARE(editor.toPlainText(), "  \\begin{itemize}\n    \\item ");
    // Outside a list it is a plain newline that keeps the indentation (no U+2028).
    editor.setPlainText("\\begin{center}\n  text");
    editor.moveCursor(QTextCursor::End);
    shiftEnter();
    QCOMPARE(editor.toPlainText(), "\\begin{center}\n  text\n  ");
}

void Tests::tabIndent()
{
    Editor editor;
    editor.setPlainText("a\n  b\n\nc");
    editor.moveCursor(QTextCursor::Start);
    editor.moveCursor(QTextCursor::End, QTextCursor::KeepAnchor);
    QTest::keyClick(&editor, Qt::Key_Tab);
    QCOMPARE(editor.toPlainText(), "    a\n      b\n\n    c"); // empty lines stay empty
    QVERIFY(editor.textCursor().hasSelection());
    QTest::keyClick(&editor, Qt::Key_Backtab, Qt::ShiftModifier);
    QCOMPARE(editor.toPlainText(), "a\n  b\n\nc");
    QTest::keyClick(&editor, Qt::Key_Backtab, Qt::ShiftModifier);
    QCOMPARE(editor.toPlainText(), "a\nb\n\nc");
    editor.undo();
    QCOMPARE(editor.toPlainText(), "a\n  b\n\nc"); // one undo step per Shift+Tab
    // Caret only: Tab pads to the next multiple of four, Shift+Tab outdents the line.
    editor.setPlainText("ab");
    editor.moveCursor(QTextCursor::End);
    QTest::keyClick(&editor, Qt::Key_Tab);
    QCOMPARE(editor.toPlainText(), "ab  ");
    editor.setPlainText("\t\tx");
    editor.moveCursor(QTextCursor::End);
    QTest::keyClick(&editor, Qt::Key_Backtab, Qt::ShiftModifier);
    QCOMPARE(editor.toPlainText(), "\tx");
    QCOMPARE(editor.textCursor().positionInBlock(), 2);
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
    editor.moveCursor(QTextCursor::End);
    const int position = editor.textCursor().position();
    QVERIFY(!editor.findText("absent"));
    QCOMPARE(editor.textCursor().position(), position);
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
    auto describe = [](QRect r) { return QString("(%1,%2 %3x%4)").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height()); };
    QVERIFY2(editorArea.contains(area), qPrintable("widget " + describe(area) + " outside editor " + describe(editorArea)));
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
    // Ctrl+J opens the build log under the editor only, not under the PDF.
    QAction *logAction = nullptr;
    for (auto *action : window.actions()) if (action->shortcut() == QKeySequence("Ctrl+J")) logAction = action;
    QVERIFY(logAction);
    auto *log = window.findChild<QTabWidget *>("bottomPanel");
    QVERIFY(log->isVisible()); // open from the start, showing the empty Problems list
    logAction->trigger();
    QVERIFY(!log->isVisible());
    logAction->trigger();
    QTRY_VERIFY(log && log->isVisible() && log->width() > 0);
    auto *editorTabs = window.findChild<QTabWidget *>("editorTabs");
    int logLeft = log->mapTo(&window, QPoint()).x(), tabsLeft = editorTabs->mapTo(&window, QPoint()).x();
    QCOMPARE(logLeft, tabsLeft);
    QCOMPARE(log->width(), editorTabs->width());
    QVERIFY(log->mapTo(&window, QPoint()).y() > editorTabs->mapTo(&window, QPoint()).y() + editorTabs->height() - 1);
    logAction->trigger();
    // Each tab has its own close button (VS Code style), which closes that tab.
    auto *tabs = window.findChild<QTabWidget *>("editorTabs");
    int count = tabs->count();
    auto *close = tabs->tabBar()->tabButton(count - 1, QTabBar::LeftSide)->findChild<QToolButton *>();
    QVERIFY(close);
    close->click();
    QCOMPARE(tabs->count(), count - 1);

    // Wrapped lines continue under the first non-blank character.
    Editor indented;
    indented.resize(320, 400);
    indented.setWordWrap(true);
    indented.show();
    QVERIFY(QTest::qWaitForWindowExposed(&indented));
    QString text = "    " + QString("word ").repeated(40);
    indented.setPlainText(text);
    double space = QFontMetricsF(indented.font()).horizontalAdvance(' ');
    auto margin = [&](int block) { return indented.document()->findBlockByNumber(block).blockFormat().leftMargin(); };
    QTRY_VERIFY(qAbs(margin(0) - 4 * space) < 0.5);
    QTRY_VERIFY(indented.document()->firstBlock().layout()->lineCount() > 1);
    auto *layout = indented.document()->firstBlock().layout();
    QVERIFY(qAbs(layout->lineAt(1).x() - layout->lineAt(0).x() - 4 * space) < 0.5);
    QCOMPARE(indented.toPlainText(), text);
    QVERIFY(!indented.document()->isModified());
    QVERIFY(!indented.document()->isUndoAvailable());
    indented.moveCursor(QTextCursor::End);
    QTest::keyClick(&indented, Qt::Key_Return); // auto-indent keeps four spaces
    QTest::keyClicks(&indented, "  x");
    QTRY_VERIFY(qAbs(margin(1) - 6 * space) < 0.5);
    while (indented.document()->isUndoAvailable()) indented.undo();
    QCOMPARE(indented.toPlainText(), text);
    QTRY_VERIFY(qAbs(margin(0) - 4 * space) < 0.5);
    QVERIFY(!indented.document()->isModified());
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

    // Visit every page, then return to page 1: trimming keeps only pages near the view.
    viewer.setZoom(1);
    QTest::qWait(250); // the DPI change settles
    // repaint() queues the visible tiles; renderingIdle() alone is true before the first paint.
    auto show = [&](int page)
    {
        viewer.goToPage(page);
        viewer.viewport()->repaint();
        QTRY_VERIFY_WITH_TIMEOUT(viewer.renderingIdle(), 5000);
    };
    for (int page = 1; page <= 8; ++page) show(page);
    show(1);
    int full = viewer.cachedTiles();
    viewer.trimCache();
    QVERIFY2(viewer.cachedTiles() < full, qPrintable(QString("%1 tiles before, %2 after").arg(full).arg(viewer.cachedTiles())));
    QVERIFY(viewer.cachedTiles() > 0);
    // What is on screen survives: repainting needs no new render.
    quint64 requests = viewer.renderRequests();
    viewer.viewport()->repaint();
    QTest::qWait(100);
    QCOMPARE(viewer.renderRequests(), requests);
}

void Tests::darkPdfPages()
{
    QTemporaryDir dir;
    QString path = dir.filePath("dark.pdf");
    {
        QPdfWriter writer(path);
        writer.setResolution(72);
        QPainter painter(&writer);
        painter.drawText(QPointF(60, 80), "Dark pages");
    }
    PdfView viewer;
    viewer.resize(700, 650);
    viewer.show();
    QSignalSpy loaded(&viewer, &PdfView::documentReady);
    viewer.open(path);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 1, 5000);
    viewer.viewport()->repaint();
    QTRY_VERIFY_WITH_TIMEOUT(viewer.renderingIdle(), 5000);
    auto paper = [&] { return viewer.viewport()->grab().toImage().pixelColor(350, 400); };
    viewer.viewport()->repaint();
    QCOMPARE(paper(), QColor(Qt::white));
    viewer.setDarkPages(true);
    QVERIFY(paper().lightness() < 60);
    viewer.setDarkPages(false);
    QCOMPARE(paper(), QColor(Qt::white));
}

void Tests::navigationHistory()
{
    QTemporaryDir dir;
    QByteArray text;
    for (int i = 1; i <= 80; ++i) text += "line " + QByteArray::number(i) + "\n";
    write(dir.filePath("a.tex"), text);
    write(dir.filePath("b.tex"), "other\n");
    Window window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QToolButton *back = nullptr, *forward = nullptr;
    for (auto *button : window.findChildren<QToolButton *>("navButton")) (button->toolTip().startsWith("Go back") ? back : forward) = button;
    QVERIFY(back && forward);
    QVERIFY(window.openSource(dir.filePath("a.tex")));
    auto *editor = window.currentEditor();
    editor->goToLine(2);
    editor->goToLine(5); // a small move is not a place to return to
    QVERIFY(!back->isEnabled());
    editor->goToLine(60);
    QVERIFY(back->isEnabled());
    QVERIFY(!forward->isEnabled());
    back->click();
    QCOMPARE(editor->lineNumber(), 5);
    QVERIFY(forward->isEnabled());
    forward->click();
    QCOMPARE(editor->lineNumber(), 60);
    // Switching tabs counts as a move; back returns to the first tab, and a closed tab is skipped.
    QVERIFY(window.openSource(dir.filePath("b.tex")));
    QVERIFY(window.currentEditor() != editor);
    back->click();
    QCOMPARE(window.currentEditor(), editor);
    QCOMPARE(editor->lineNumber(), 60);
}

void Tests::quickOpenAndCommands()
{
    QCOMPARE(Palette::score("Build / stop", "xyz"), 0);
    QVERIFY(Palette::score("Build / stop", "bs") > 0);
    QVERIFY(Palette::score("xpdf", "pdf") > Palette::score("xpxdxf", "pdf")); // consecutive letters win
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkpath("chapters"));
    QVERIFY(QDir(dir.path()).mkpath(".folitex-build"));
    write(dir.filePath("main.tex"), "\\section{Main}\n");
    write(dir.filePath("chapters/one.tex"), "one");
    write(dir.filePath(".folitex-build/hidden.tex"), "hidden");
    write(dir.filePath("picture.png"), "x");
    Window window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.openWorkspace(dir.path());
    auto trigger = [&](const QString &name)
    {
        for (QAction *action : window.actions()) if (action->text() == name) { action->trigger(); return true; }
        return false;
    };
    auto *palette = window.findChild<Palette *>();
    auto *list = palette->findChild<QListWidget *>();
    auto *input = palette->findChild<QLineEdit *>();
    QVERIFY(trigger("Quick open…"));
    QVERIFY(palette->isVisible());
    QStringList names;
    for (int row = 0; row < list->count(); ++row) names << list->item(row)->text();
    QVERIFY(names.contains("main.tex") && names.contains("chapters/one.tex"));
    QVERIFY(!names.join(' ').contains("hidden") && !names.join(' ').contains("picture"));
    QTest::keyClicks(input, "one");
    QCOMPARE(list->item(0)->text(), QString("chapters/one.tex"));
    QTest::keyClick(input, Qt::Key_Return);
    QVERIFY(!palette->isVisible());
    QCOMPARE(window.currentEditor()->property("path").toString(), dir.filePath("chapters/one.tex"));
    // Commands come from the menus and run the real action.
    QVERIFY(trigger("Command palette…"));
    QTest::keyClicks(input, "word wrap");
    QVERIFY(list->count() >= 1);
    bool wrapped = window.wordWrap();
    QTest::keyClick(input, Qt::Key_Return);
    QCOMPARE(window.wordWrap(), !wrapped);
    QTest::keyClick(input, Qt::Key_Escape);
    QVERIFY(!palette->isVisible());
    QTest::keyClick(&window, Qt::Key_K, Qt::ControlModifier); // the real shortcut, not the action
    QVERIFY(palette->isVisible());
    QTest::keyClick(input, Qt::Key_Escape);
}

void Tests::selectionBarAndFocusMode()
{
    Editor editor;
    editor.resize(500, 300);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    editor.setPlainText("hello world");
    auto *bar = editor.findChild<QFrame *>("selectionBar");
    QVERIFY(bar && !bar->isVisible());
    auto cursor = editor.textCursor();
    cursor.setPosition(0);
    cursor.setPosition(5, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);
    QTest::mouseRelease(editor.viewport(), Qt::LeftButton);
    QVERIFY(bar->isVisible());
    QToolButton *bold = nullptr;
    for (auto *button : bar->findChildren<QToolButton *>()) if (button->text() == "B") bold = button;
    QVERIFY(bold);
    bold->click();
    QCOMPARE(editor.toPlainText(), QString("\\textbf{hello} world"));
    QVERIFY(!bar->isVisible());
    // Typing hides it again.
    cursor = editor.textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    editor.setTextCursor(cursor);
    QTest::mouseRelease(editor.viewport(), Qt::LeftButton);
    QTest::keyClick(&editor, Qt::Key_Right);
    QVERIFY(!bar->isVisible());

    Window window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.statusBar()->isVisible());
    auto toggle = [&]
    {
        for (QAction *action : window.actions()) if (action->text() == "Focus") { action->trigger(); return; }
        QFAIL("no Focus action");
    };
    toggle();
    QVERIFY(!window.statusBar()->isVisible());
    QVERIFY(window.currentEditor() == nullptr || window.currentEditor()->isVisible());
    toggle();
    QVERIFY(window.statusBar()->isVisible());

    // "Name · Key" tooltips show the key as a chip; Esc leaves focus mode from the editor.
    auto *build = window.findChild<QPushButton *>("editorAction");
    QVERIFY(build && build->toolTip().contains("F5"));
    QHelpEvent help(QEvent::ToolTip, QPoint(5, 5), build->mapToGlobal(QPoint(5, 5)));
    QApplication::sendEvent(build, &help);
    QVERIFY(QToolTip::text().contains("background-color") && QToolTip::text().contains("F5"));
    QToolTip::hideText();
    QTemporaryDir focusDir;
    write(focusDir.filePath("f.tex"), "text\n");
    QVERIFY(window.openSource(focusDir.filePath("f.tex")));
    toggle();
    QVERIFY(!window.statusBar()->isVisible());
    window.currentEditor()->setFocus();
    QTest::keyClick(window.currentEditor(), Qt::Key_Escape);
    QVERIFY(window.statusBar()->isVisible());
}

void Tests::honestEmptyStates()
{
    Window window;
    window.resize(1200, 800);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    // Before any build the Problems panel promises nothing and shrinks to its tab strip.
    auto *panel = window.findChild<QTabWidget *>("bottomPanel");
    QCOMPARE(window.findChild<QLabel *>("emptyTitle")->text(), QString("No problems yet"));
    QVERIFY(panel->isVisible() && panel->height() < 80);
    QTest::mouseClick(panel->tabBar(), Qt::LeftButton, {}, panel->tabBar()->tabRect(0).center());
    QTRY_VERIFY(panel->height() > 100); // a click on its tab opens it
    // No PDF: the page counter says so and the zoom controls are off.
    auto *bar = window.findChild<QWidget *>("pdfBar");
    QVERIFY(bar);
    for (auto *child : bar->findChildren<QToolButton *>()) QVERIFY(!child->isVisible());
    // An empty Untitled tab offers the ways in; typing hides them.
    for (QAction *action : window.actions()) if (action->text() == "New document" && !window.currentEditor()) action->trigger();
    QVERIFY(window.currentEditor());
    auto *welcome = window.findChild<QFrame *>("welcome");
    QVERIFY(welcome && welcome->isVisible());
    QTest::keyClicks(window.currentEditor(), "x");
    QVERIFY(!welcome->isVisible());
}

void Tests::buildFailureEmptyProblemsState()
{
    Window window;
    window.resize(1200, 800);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *title = window.findChild<QLabel *>("emptyTitle");
    auto *text = window.findChild<QLabel *>("emptyText");
    QVERIFY(title && text);

    QCOMPARE(title->text(), QString("No problems yet"));
    QVERIFY(window.m_panelCollapsed);

    // Uncollapse the panel
    window.setPanelCollapsed(false);
    QVERIFY(!window.m_panelCollapsed);

    // Build failure with empty problems
    window.setProblems({}, "/fake/dir", false);
    QCOMPARE(title->text(), QString("Build failed"));
    QCOMPARE(text->text(), QString("The build failed without reporting specific file errors. Check the Output tab for details."));
    QVERIFY(!window.m_panelCollapsed);

    // Build success with empty problems
    window.setProblems({}, "/fake/dir", true);
    QCOMPARE(title->text(), QString("No problems"));
    QVERIFY(window.m_panelCollapsed);
}

void Tests::panelMaximizeAndRestore()
{
    Window window;
    window.resize(1000, 700);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(window.m_tabs->isVisible());
    QVERIFY(window.m_panel->isVisible());
    QVERIFY(window.m_panelMaximize);

    // Initial state: not maximized
    QCOMPARE(window.m_panelMaximize->toolTip(), QString("Maximize panel"));

    // Check actions on Output tab
    window.m_panel->setCurrentIndex(1); // Output tab
    QVERIFY(window.m_panelMaximize->isVisible());
    if (window.m_problemFilter) QVERIFY(!window.m_problemFilter->isVisible());
    if (window.m_collapseProblems) QVERIFY(!window.m_collapseProblems->isVisible());

    // Maximize panel
    window.toggleMaximizePanel();
    QVERIFY(!window.m_tabs->isVisible());
    QVERIFY(!window.m_panelCollapsed);
    QCOMPARE(window.m_panelMaximize->toolTip(), QString("Restore panel"));
    QVERIFY(window.m_panel->height() > 200);

    // Restore panel
    window.toggleMaximizePanel();
    QVERIFY(window.m_tabs->isVisible());
    QCOMPARE(window.m_panelMaximize->toolTip(), QString("Maximize panel"));
    QVERIFY(window.m_tabs->height() > 50);

    // Maximize panel, then switch back to editor via openSource
    window.toggleMaximizePanel();
    QVERIFY(!window.m_tabs->isVisible());
    QTemporaryDir dir;
    QString path = dir.filePath("test.tex");
    write(path, "hello\n");
    QVERIFY(window.openSource(path));
    QVERIFY(window.m_tabs->isVisible());
    QCOMPARE(window.m_panelMaximize->toolTip(), QString("Maximize panel"));
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

void Tests::buildBoundaryCases_data()
{
    QTest::addColumn<QString>("engine");
    QTest::addColumn<QString>("executable");
    QTest::newRow("pdfLaTeX") << QString("pdfLaTeX") << QString("pdflatex");
    QTest::newRow("LuaLaTeX") << QString("LuaLaTeX") << QString("lualatex");
    QTest::newRow("XeLaTeX") << QString("XeLaTeX") << QString("xelatex");
}

void Tests::buildBoundaryCases()
{
    QFETCH(QString, engine);
    QFETCH(QString, executable);
    if (QStandardPaths::findExecutable("latexmk").isEmpty()) QSKIP("latexmk unavailable");
    if (QStandardPaths::findExecutable(executable).isEmpty()) QSKIP(qPrintable(executable + " unavailable"));
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkdir("chapters"));
    const QString chapter = dir.filePath("chapters/intro.tex");
    write(chapter, "% !TeX root = ../missing.tex\n");
    QVERIFY(BuildRunner::rootFile(chapter).isEmpty());
    BuildRunner runner;
    QSignalSpy done(&runner, &BuildRunner::finished);
    QVERIFY(!runner.start(chapter, engine));
    QCOMPARE(done.size(), 1);
    QVERIFY(!done.first().at(0).toBool());
    QVERIFY(!runner.running());

    const QByteArray valid = "\\documentclass{article}\n\\begin{document}\nHello.\n\\end{document}\n";
    const QString unicodeRoot = dir.filePath("Bản thảo.tex");
    write(unicodeRoot, valid);
    write(chapter, QString("% !TeX root = ../Bản thảo.tex\n").toUtf8());
    QCOMPARE(BuildRunner::rootFile(chapter), unicodeRoot);

    const QString main = dir.filePath("main file.tex");
    const QList<QByteArray> sources = {valid, valid,
        "\\documentclass{article}\n\\begin{document}\n\\undefinedAuditCommand\n\\end{document}\n",
        "\\documentclass{article}\n\\usepackage{folitex-audit-nonexistent-package}\n\\begin{document}Hello.\\end{document}\n"};
    for (int i = 0; i < sources.size(); ++i)
    {
        if (i == 1) QVERIFY(QDir(dir.filePath(".folitex-build")).removeRecursively());
        write(main, sources[i]);
        done.clear();
        QVERIFY(runner.start(main, engine));
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 12000);
        QCOMPARE(done.first().at(0).toBool(), i < 2);
        QVERIFY(!runner.running());
        if (i < 2) QVERIFY(QFileInfo::exists(BuildRunner::pdfPath(main)));
        else
        {
            QFile log(dir.filePath(".folitex-build/main file.log"));
            QVERIFY(log.open(QIODevice::ReadOnly));
            const QByteArray text = log.readAll();
            QVERIFY(text.contains(i == 2 ? "Undefined control sequence" : "folitex-audit-nonexistent-package"));
        }
    }
}

void Tests::cancellationKeepsUnrelatedProcess()
{
#ifdef Q_OS_UNIX
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString command = dir.filePath("latexmk");
    write(command, "#!/bin/sh\ntrap '' TERM\nexec sleep 30\n");
    QVERIFY(QFile::setPermissions(command, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    const QString source = dir.filePath("hung.tex");
    write(source, "\\documentclass{article}\n");
    QProcess unrelated;
    unrelated.start("sleep", {"30"});
    QVERIFY(unrelated.waitForStarted(3000));
    auto cleanup = qScopeGuard([&] { unrelated.kill(); unrelated.waitForFinished(3000); });
    const QByteArray oldPath = qgetenv("PATH");
    auto restore = qScopeGuard([&] { qputenv("PATH", oldPath); });
    qputenv("PATH", dir.path().toUtf8() + ':' + oldPath);
    BuildRunner runner;
    QSignalSpy done(&runner, &BuildRunner::finished);
    QVERIFY(runner.start(source, "pdfLaTeX"));
    for (int i = 0; i < 5; ++i) QVERIFY(!runner.start(source, "pdfLaTeX"));
    QTest::qWait(100);
    QVERIFY(runner.running());
    runner.stop();
    runner.stop();
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 5000);
    QVERIFY(!runner.running());
    QVERIFY(runner.cancelled());
    QCOMPARE(unrelated.state(), QProcess::Running);
#else
    QSKIP("Process-group cancellation is Unix-specific");
#endif
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

void Tests::mixedNewlinesRejected()
{
    QTemporaryDir dir;
    Window window;
    window.currentEditor()->setPlainText("keep this buffer");
    for (const QByteArray &bytes : {QByteArray("first\r\nsecond\n"), QByteArray("first\rsecond\n"),
            QByteArray("first\r\nsecond\r")})
    {
        const QString path = dir.filePath("mixed.tex");
        write(path, bytes);
        QVERIFY(!window.openSource(path));
        QCOMPARE(window.currentEditor()->toPlainText(), "keep this buffer");
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), bytes);
    }
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

void Tests::selfSaveNoFalseConflict()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("doc.tex");
    write(path, "initial\n");
    Window window;
    QVERIFY(window.openSource(path));
    auto *editor = window.currentEditor();
    QVERIFY(editor);

    // Save and immediately type again before a delayed file-watcher event arrives.
    editor->moveCursor(QTextCursor::End);
    editor->insertPlainText("saved\n");
    QVERIFY(window.save(editor));
    editor->insertPlainText("unsaved\n");
    window.m_changedSources.insert(path);
    window.reloadSource(editor);
    QVERIFY(!editor->property("conflict").toBool());
    QCOMPARE(editor->toPlainText(), QString("initial\nsaved\nunsaved\n"));

    // A genuine external edit while modified still marks a conflict.
    write(path, "external\n");
    window.reloadSource(editor);
    QVERIFY(editor->property("conflict").toBool());
    QCOMPARE(editor->toPlainText(), QString("initial\nsaved\nunsaved\n"));
}

void Tests::shortcutGuardsUnfocusedEditor()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("main.tex");
    write(path, "hello world\n");
    Window window;
    window.resize(900, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.openSource(path));
    auto *editor = window.currentEditor();
    QVERIFY(editor);

    // Focus a non-editor widget and simulate a shortcut-triggered editor action.
    window.m_pdf->setFocus();
    QTRY_VERIFY(!editor->hasFocus());

    QAction *toggleComment = nullptr;
    for (auto *action : window.findChildren<QAction *>())
        if (action->text() == "Toggle comment") { toggleComment = action; break; }
    QVERIFY(toggleComment);

    window.m_viaShortcut = true;
    toggleComment->trigger();
    window.m_viaShortcut = false;
    QCOMPARE(editor->toPlainText(), QString("hello world\n"));

    // Menu invocation (not via shortcut) still works.
    toggleComment->trigger();
    QCOMPARE(editor->toPlainText(), QString("% hello world\n"));

#ifdef FOLITEX_TERMINAL
    // ShortcutOverride inside QTermWidget is accepted so control chords reach the terminal.
    auto *term = new QTermWidget(0, window.m_agent);
    QKeyEvent overrideEvt(QEvent::ShortcutOverride, Qt::Key_C, Qt::ControlModifier);
    overrideEvt.ignore();
    QVERIFY(window.eventFilter(term, &overrideEvt));
    QVERIFY(overrideEvt.isAccepted());
    delete term;
#endif
}

void Tests::buildPreparesReferencedSubdirs()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkpath("building"));
    QVERIFY(QDir(dir.path()).mkpath("sections/sub"));
    QVERIFY(QDir(dir.path()).mkpath("unrelated/deep"));
    write(dir.filePath("building/ch1.tex"), "\\input{sections/sub/part}\n");
    write(dir.filePath("sections/sub/part.tex"), "hello\n");
    write(dir.filePath("main.tex"), "\\documentclass{article}\n\\begin{document}\n\\include{building/ch1}\n\\end{document}\n");

    BuildRunner runner;
    QVERIFY(runner.start(dir.filePath("main.tex"), "pdflatex"));
    runner.stop();
    QDir buildDir(dir.filePath(".folitex-build"));
    QVERIFY(buildDir.exists("building"));
    QVERIFY(buildDir.exists("sections/sub"));
    QVERIFY(!buildDir.exists("unrelated"));
}

void Tests::inverseSyncRelativePath()
{
#ifdef Q_OS_WIN
    QSKIP("shell script stand-in for synctex");
#else
    QTemporaryDir bin, project;
    QVERIFY(QDir(project.path()).mkpath("chapters"));
    QVERIFY(QDir(project.path()).mkpath(".folitex-build"));
    const QString chapter = project.filePath("chapters/ch1.tex");
    write(chapter, "line one\nline two\nline three\n");
    const QString pdfPath = project.filePath(".folitex-build/main.pdf");
    write(pdfPath, "%PDF-1.4\n");

    QFile fake(bin.filePath("synctex"));
    QVERIFY(fake.open(QIODevice::WriteOnly));
    fake.write("#!/bin/sh\nprintf 'Input:chapters/ch1.tex\\nLine:2\\n'\n");
    fake.close();
    fake.setPermissions(fake.permissions() | QFileDevice::ExeOwner);

    const QByteArray oldPath = qgetenv("PATH");
    auto restore = qScopeGuard([&] { qputenv("PATH", oldPath); });
    qputenv("PATH", bin.path().toUtf8() + ":" + oldPath);

    Window window;
    window.m_pdf->open(pdfPath);
    window.inverseSync(1, 10, 10, {}, -1);
    QTRY_VERIFY_WITH_TIMEOUT(window.currentEditor() && window.currentEditor()->property("path").toString() == QFileInfo(chapter).absoluteFilePath(), 3000);
    QCOMPARE(window.currentEditor()->textCursor().blockNumber(), 1);
#endif
}

void Tests::presentationPrefetchRespectsCacheBudget()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("slides.pdf");
    {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QSizeF(160, 90), QPageSize::Millimeter));
        QPainter painter(&writer);
        writer.newPage();
        writer.newPage();
    }
    PdfView viewer;
    // 5K-class viewport where 3 slides exceed the 128 MiB cache ceiling; prefetch must not thrash.
    viewer.resize(5120, 2880);
    viewer.setPresentation(true);
    viewer.goToPage(2);
    QSignalSpy loaded(&viewer, &PdfView::documentReady);
    viewer.show();
    QVERIFY(QTest::qWaitForWindowExposed(&viewer));
    viewer.open(path);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(viewer.renderingIdle(), 10000);
    QVERIFY(viewer.cacheKiB() <= viewer.cacheLimitKiB());
    const quint64 requestsAfterIdle = viewer.renderRequests();
    viewer.viewport()->update();
    QTest::qWait(80);
    QVERIFY(viewer.renderingIdle());
    QCOMPARE(viewer.renderRequests(), requestsAfterIdle);
}

void Tests::pdfReloadPreservesViewAndSwitchResets()
{
    QTemporaryDir dir;
    auto makePdf = [](const QString &path, int pages)
    {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        for (int i = 1; i < pages; ++i) writer.newPage();
    };
    const QString a = dir.filePath("a.pdf");
    const QString b = dir.filePath("b.pdf");
    makePdf(a, 3);
    makePdf(b, 2);

    PdfView viewer;
    viewer.resize(500, 400);
    QSignalSpy loaded(&viewer, &PdfView::documentReady);
    QSignalSpy failed(&viewer, &PdfView::loadFailed);
    viewer.show();
    QVERIFY(QTest::qWaitForWindowExposed(&viewer));
    viewer.open(a);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 1, 5000);
    viewer.goToPage(2);
    QVERIFY(viewer.verticalScrollBar()->value() > 0);
    QTRY_VERIFY_WITH_TIMEOUT(viewer.renderingIdle() && viewer.cachedTiles() > 0, 5000);

    // Corrupt same-path reload keeps existing pages and cache intact.
    write(a, "not a pdf");
    viewer.open(a);
    QCOMPARE(viewer.pageCount(), 3);
    QVERIFY(viewer.cachedTiles() > 0);
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 5000);
    QCOMPARE(viewer.pageCount(), 3);
    QVERIFY(viewer.cachedTiles() > 0);

    // Valid same-path reload succeeds cleanly.
    makePdf(a, 3);
    viewer.open(a);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 2, 5000);
    QCOMPARE(viewer.pageCount(), 3);

    // Switching to a different PDF resets scroll position and presentation slide.
    viewer.setPresentation(true);
    viewer.goToPage(3);
    QCOMPARE(viewer.currentPage(), 3);
    viewer.setPresentation(false);
    viewer.goToPage(2);
    QVERIFY(viewer.verticalScrollBar()->value() > 0);
    viewer.open(b);
    QCOMPARE(viewer.verticalScrollBar()->value(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 3, 5000);
    QCOMPARE(viewer.pageCount(), 2);
    viewer.setPresentation(true);
    QCOMPARE(viewer.currentPage(), 1);
}

void Tests::programmaticScrollStopsSmoothScroll()
{
    // Editor: goToLine stops an in-flight wheel animation.
    Editor editor;
    editor.resize(600, 300);
    editor.setPlainText(QString("line\n").repeated(500));
    editor.show();
    QTest::qWait(30);
    QWheelEvent wheel(QPointF(30, 30), QPointF(30, 30), QPoint(), QPoint(0, -120), Qt::NoButton,
        Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(editor.viewport(), &wheel);
    editor.goToLine(300);
    const int editorTarget = editor.verticalScrollBar()->value();
    QVERIFY(editorTarget > 500);
    QTest::qWait(150);
    QCOMPARE(editor.verticalScrollBar()->value(), editorTarget);

    // PdfView: goToPage and setZoom stop an in-flight wheel animation.
    QTemporaryDir dir;
    const QString path = dir.filePath("multi.pdf");
    {
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        writer.newPage();
        writer.newPage();
        writer.newPage();
    }
    PdfView viewer;
    viewer.resize(500, 400);
    QSignalSpy loaded(&viewer, &PdfView::documentReady);
    viewer.show();
    QVERIFY(QTest::qWaitForWindowExposed(&viewer));
    viewer.open(path);
    QTRY_COMPARE_WITH_TIMEOUT(loaded.size(), 1, 5000);

    QApplication::sendEvent(viewer.viewport(), &wheel);
    viewer.goToPage(3);
    const int pdfTarget = viewer.verticalScrollBar()->value();
    QVERIFY(pdfTarget > 400);
    QTest::qWait(150);
    QCOMPARE(viewer.verticalScrollBar()->value(), pdfTarget);

    QApplication::sendEvent(viewer.viewport(), &wheel);
    viewer.setZoom(1.5);
    const int zoomTarget = viewer.verticalScrollBar()->value();
    QTest::qWait(150);
    QCOMPARE(viewer.verticalScrollBar()->value(), zoomTarget);
}

void Tests::gutterGeometryTracksFontAndLines()
{
    Editor editor;
    editor.resize(500, 300);
    editor.show();
    QTest::qWait(20);
    QWidget *gutter = nullptr;
    for (auto *child : editor.findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
        if (child != editor.viewport() && !qobject_cast<QScrollBar *>(child) && child->isVisible()
            && child->x() == 0 && child->height() == editor.viewport()->height())
        { gutter = child; break; }
    QVERIFY(gutter);

    editor.setPlainText("1\n2\n3\n");
    const int smallWidth = gutter->width();
    QCOMPARE(smallWidth, editor.viewport()->x());

    editor.setPlainText(QString("x\n").repeated(1200));
    const int manyLinesWidth = gutter->width();
    QCOMPARE(manyLinesWidth, editor.viewport()->x());
    QVERIFY(manyLinesWidth > smallWidth);

    QFont bigger = editor.font();
    bigger.setPointSize(bigger.pointSize() + 10);
    editor.setEditorFont(bigger);
    const int bigFontWidth = gutter->width();
    QCOMPARE(bigFontWidth, editor.viewport()->x());
    QVERIFY(bigFontWidth > manyLinesWidth);
}

void Tests::fontZoomDoesNotWipeRedo()
{
    Editor editor;
    editor.resize(400, 300);
    editor.setWordWrap(true);
    editor.show();
    editor.setPlainText("    first line\nsecond");
    QTest::qWait(20);
    editor.moveCursor(QTextCursor::End);
    QTest::keyClicks(&editor, " edit");
    QTest::qWait(20);
    const QString edited = editor.toPlainText();

    editor.undoText();
    QTest::qWait(20);
    QVERIFY(editor.document()->isRedoAvailable());

    QFont bigger = editor.font();
    bigger.setPointSize(bigger.pointSize() + 4);
    editor.setEditorFont(bigger);
    QTest::qWait(20);
    QVERIFY(editor.document()->isRedoAvailable());

    editor.redoText();
    QCOMPARE(editor.toPlainText(), edited);
}

void Tests::closingBracketSkipWhenBalanced()
{
    Editor editor;
    editor.setPlainText("\\frac{a{b}");
    auto cursor = editor.textCursor();
    cursor.setPosition(9); // before the single '}'
    editor.setTextCursor(cursor);
    QTest::keyClicks(&editor, "}");
    QCOMPARE(editor.toPlainText(), QString("\\frac{a{b}}"));

    editor.setPlainText("\\frac{a}");
    cursor = editor.textCursor();
    cursor.setPosition(7); // before balanced '}'
    editor.setTextCursor(cursor);
    QTest::keyClicks(&editor, "}");
    QCOMPARE(editor.toPlainText(), QString("\\frac{a}"));
    QCOMPARE(editor.textCursor().position(), 8);
}

void Tests::insertTemplateLiteralBulletAndMultiline()
{
    Editor editor;
    editor.setPlainText("    item • one\n    item • two");
    editor.selectAll();
    editor.insertTemplate("\\textbf{•}");
    QCOMPARE(editor.toPlainText(), QString("\\textbf{    item • one\n    item • two}"));

    // Multi-line selection on an indented line must not be double-indented by template newlines.
    editor.setPlainText("    first\n    second");
    auto cursor = editor.textCursor();
    cursor.setPosition(4);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);
    editor.insertTemplate("\\begin{quote}\n    •\n\\end{quote}");
    QCOMPARE(editor.toPlainText(), QString("    \\begin{quote}\n        first\n    second\n    \\end{quote}"));
}

void Tests::beamerEnvironmentCompletion()
{
    Editor editor;
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    auto *completer = editor.findChild<QCompleter *>();
    QVERIFY(completer);
    editor.setPlainText("\\documentclass{beamer}\n\\begin{itemi");
    editor.moveCursor(QTextCursor::End);
    QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier);
    QVERIFY(completer->popup()->isVisible());
    emit completer->activated(QString("itemize<%1>").arg(QChar(0x2022)));
    QCOMPARE(editor.toPlainText(), QString("\\documentclass{beamer}\n\\begin{itemize}<>\n    \n\\end{itemize}"));
}

void Tests::enterAutoIndentCapAndFoldedHeader()
{
    Editor editor;
    editor.setPlainText("    hello");
    auto cursor = editor.textCursor();
    cursor.setPosition(2); // middle of 4 leading spaces
    editor.setTextCursor(cursor);
    QTest::keyClick(&editor, Qt::Key_Return);
    QCOMPARE(editor.toPlainText(), QString("  \n    hello"));

    // Pressing Enter at the end of a folded header unfolds its body instead of orphaning it.
    editor.setPlainText("\\section{A}\nbody\n\\section{B}\n");
    editor.toggleFold(0);
    QVERIFY(!editor.document()->findBlockByNumber(1).isVisible());
    cursor = editor.textCursor();
    cursor.setPosition(QString("\\section{A}").size());
    editor.setTextCursor(cursor);
    QTest::keyClick(&editor, Qt::Key_Return);
    QVERIFY(editor.document()->findBlockByNumber(1).isVisible());
}

QTEST_MAIN(Tests)
#include "tests.moc"

void Tests::agentPanel()
{
#ifdef Q_OS_WIN
    const QString executableSuffix = ".cmd";
#else
    const QString executableSuffix;
#endif
    QTemporaryDir bin;
    QFile fake(bin.filePath("codex" + executableSuffix));
    QVERIFY(fake.open(QIODevice::WriteOnly));
    fake.write("#!/bin/sh\n");
    fake.close();
    fake.setPermissions(fake.permissions() | QFileDevice::ExeOwner);
    const QByteArray path = qgetenv("PATH");
    qputenv("PATH", bin.path().toUtf8());
    const auto agents = AgentPanel::installedAgents();
    qputenv("PATH", path);
    bool found = false;
    for (const auto &agent : agents) found |= agent.name == "Codex" && agent.program == fake.fileName();
    QVERIFY(found);

    Window window;
    QTemporaryDir project;
    window.openWorkspace(project.path());
    QCOMPARE(window.m_panel->tabText(2), QString("Agent"));
    window.m_panel->setCurrentIndex(2); // refreshes; the hint names the folder the agent runs in
    QVERIFY(!window.m_agent->running());
    QCOMPARE(window.m_agent->directory(), project.path());

    // A first prompt goes where each CLI takes it.
    QFile gemini(bin.filePath("gemini" + executableSuffix));
    QVERIFY(gemini.open(QIODevice::WriteOnly));
    gemini.close();
    gemini.setPermissions(gemini.permissions() | QFileDevice::ExeOwner);
    qputenv("PATH", bin.path().toUtf8());
    window.m_agent->refresh();
    qputenv("PATH", path);
    auto *combo = window.m_agent->m_agents;
    combo->setCurrentIndex(combo->findText("Gemini CLI"));
    QCOMPARE(window.m_agent->command("fix it"), QStringList({"-i", "fix it"}));
    combo->setCurrentIndex(combo->findText("Codex"));
    QCOMPARE(window.m_agent->command("fix it"), QStringList({"fix it"}));
    QVERIFY(window.m_agent->command({}).isEmpty());

    // Text an agent changes on disk is tinted until the next key press.
    const QString source = project.filePath("main.tex");
    write(source, "a\nb\nc\n");
    QVERIFY(window.openSource(source));
    auto *editor = window.currentEditor();
    auto tinted = [editor]
    {
        for (const auto &selection : editor->extraSelections())
            if (selection.cursor.selectedText().contains("agent")) return true;
        return false;
    };
    write(source, "a\nb by agent\nc\n");
    QTRY_VERIFY_WITH_TIMEOUT(editor->toPlainText().contains("by agent"), 3000);
    QVERIFY(tinted());
    QTest::keyClick(editor, Qt::Key_Right);
    QVERIFY(!tinted());
}

void Tests::agentTerminal()
{
#ifndef FOLITEX_TERMINAL
    QSKIP("built without qtermwidget; Start would open an external terminal");
#else
    // A stand-in agent: records its arguments, folder and every submitted line; "quit" exits.
    QTemporaryDir bin, project;
    const QString log = bin.filePath("log");
    QFile fake(bin.filePath("claude"));
    QVERIFY(fake.open(QIODevice::WriteOnly));
    fake.write("#!/bin/sh\n"
               "printf '%s\\n' \"$@\" > \"$FOLITEX_FAKE_LOG.args\"\n"
               "pwd > \"$FOLITEX_FAKE_LOG.cwd\"\n"
               "echo \"$TERM\" > \"$FOLITEX_FAKE_LOG.term\"\n"
               "while IFS= read -r line; do\n"
               "  [ \"$line\" = quit ] && exit 0\n"
               "  printf '%s\\n' \"$line\" >> \"$FOLITEX_FAKE_LOG.input\"\n"
               "done\n");
    fake.close();
    fake.setPermissions(fake.permissions() | QFileDevice::ExeOwner);
    qputenv("FOLITEX_FAKE_LOG", log.toUtf8());
    auto read = [](const QString &path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString(); };

    Window window;
    window.resize(1000, 700);
    window.show();
    window.openWorkspace(project.path());
    const QByteArray path = qgetenv("PATH");
    // Kept for the whole test: showing the Agent tab re-scans PATH, and must find the stand-in again.
    qputenv("PATH", bin.path().toUtf8() + ":" + path);
    window.m_agent->refresh();
    auto *combo = window.m_agent->m_agents;
    combo->setCurrentIndex(combo->findText("Claude Code"));
    QCOMPARE(window.m_agent->program(), fake.fileName());

    // Fix with agent: unsaved edits are saved first, then the agent starts with the prompt.
    const QString source = project.filePath("main.tex");
    write(source, "\\documentclass{article}\n\\begin{document}\n\\foo\n\\end{document}\n");
    QVERIFY(window.openSource(source));
    auto *editor = window.currentEditor();
    editor->moveCursor(QTextCursor::End);
    editor->insertPlainText("% unsaved\n");
    window.m_problems = {Problem{source, 3, true, "Undefined control sequence.\n\\foo"}};
    window.fixWithAgent(0);
    QVERIFY(!editor->document()->isModified());
    QVERIFY(read(source).contains("% unsaved"));
    // The panel grows so the agent's TUI gets usable room, and the terminal follows its size.
    QVERIFY(window.m_vertical->sizes().value(1) * 100 >= (window.m_vertical->sizes().value(0) + window.m_vertical->sizes().value(1)) * 44);
    auto *terminal = qobject_cast<QTermWidget *>(window.m_agent->m_terminal.data());
    QVERIFY(terminal);
    QTRY_VERIFY_WITH_TIMEOUT(terminal->screenLinesCount() >= 12, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!read(log + ".args").isEmpty(), 5000);
    QVERIFY(window.m_agent->running());
    QCOMPARE(window.m_panel->currentWidget(), window.m_agent);
    const QString args = read(log + ".args");
    QCOMPARE(args.count('\n'), 1); // one argument, on one line
    QVERIFY(args.contains("error at main.tex:3: 'Undefined control sequence. \\foo'"));
    QVERIFY(args.contains("latexmk -cd -pdf -synctex=1 -interaction=nonstopmode -outdir=.folitex-build main.tex"));
    QCOMPARE(read(log + ".cwd").trimmed(), QFileInfo(project.path()).canonicalFilePath());
    QCOMPARE(read(log + ".term").trimmed(), QString("xterm-256color"));

    // Later prompts are typed into the running agent and submitted, on one line.
    window.askAgent("first part\n  second part");
    QTRY_COMPARE_WITH_TIMEOUT(read(log + ".input"), QString("first part second part\n"), 5000);
    window.askAgent("next question");
    QTRY_COMPARE_WITH_TIMEOUT(read(log + ".input"), QString("first part second part\nnext question\n"), 5000);

    // When the agent exits the panel shows Start again; a new start replaces the old args.
    window.m_agent->send("quit");
    QTRY_VERIFY_WITH_TIMEOUT(!window.m_agent->running(), 5000);
    QCOMPARE(window.m_agent->m_start->text(), QString("Start"));
    window.m_agent->start();
    QTRY_VERIFY_WITH_TIMEOUT(read(log + ".args").isEmpty() || read(log + ".args") == "\n", 5000); // no prompt this time
    QVERIFY(window.m_agent->running());
    window.m_agent->stop();
    QVERIFY(!window.m_agent->running());
    qputenv("PATH", path);
    qunsetenv("FOLITEX_FAKE_LOG");
#endif
}

// --- Task A: editor jumps, selection bar labels, Go to definition ---

void Tests::problemJumpFollowsEdits()
{
    QTemporaryDir dir;
    const QString source = dir.filePath("doc.tex");
    write(source, "line 1\nline 2\nline 3\nline 4\nline 5\nline 6\nline 7\n");

    Window window;
    QVERIFY(window.openSource(source));
    auto *editor = window.currentEditor();
    window.setProblems({Problem{source, 5, true, "Undefined control sequence."}}, dir.path(), false);
    QCOMPARE(editor->markLine(5), 5);
    QCOMPARE(editor->markLine(99), -1);

    // Insert two lines above line 5; both markLine() and F8 / goToProblem follow the shift.
    QTextCursor top(editor->document());
    top.setPosition(0);
    top.insertText("added 1\nadded 2\n");
    QCOMPARE(editor->markLine(5), 7);
    window.goToProblem(0);
    QCOMPARE(editor->lineNumber(), 7);

    // Without a mark (e.g. file changed on disk since the build), goToProblem falls back to build line.
    editor->setProblems({});
    QCOMPARE(editor->markLine(5), -1);
    window.goToProblem(0);
    QCOMPARE(editor->lineNumber(), 5);
}

void Tests::goToLineCentresOffscreen()
{
    Editor editor;
    editor.resize(600, 300);
    editor.setPlainText(QString("line\n").repeated(300));
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    QTest::qWait(20);

    editor.goToLine(200);
    QCOMPARE(editor.lineNumber(), 200);
    const int height = editor.viewport()->height();
    const int centreDown = editor.cursorRect().center().y();
    QVERIFY2(centreDown >= height / 3 && centreDown <= 2 * height / 3,
        qPrintable(QString("expected centre in [%1, %2], got %3").arg(height / 3).arg(2 * height / 3).arg(centreDown)));

    // Jumping to a line already inside the viewport leaves the scrollbar untouched.
    const int scrollBefore = editor.verticalScrollBar()->value();
    editor.goToLine(201);
    QCOMPARE(editor.lineNumber(), 201);
    QCOMPARE(editor.verticalScrollBar()->value(), scrollBefore);

    // Jumping upward to an off-screen line also centres it.
    editor.goToLine(50);
    QCOMPARE(editor.lineNumber(), 50);
    const int centreUp = editor.cursorRect().center().y();
    QVERIFY2(centreUp >= height / 3 && centreUp <= 2 * height / 3,
        qPrintable(QString("expected centre in [%1, %2], got %3").arg(height / 3).arg(2 * height / 3).arg(centreUp)));
}

void Tests::selectionBarLabels()
{
    Editor editor;
    auto *bar = editor.findChild<QFrame *>("selectionBar");
    QVERIFY(bar);
    QToolButton *emphasis = nullptr;
    QToolButton *typewriter = nullptr;
    for (auto *button : bar->findChildren<QToolButton *>())
    {
        QVERIFY(button->text() != "E" && button->text() != "tt");
        if (button->accessibleName() == "Emphasis · Ctrl+Shift+E") emphasis = button;
        if (button->accessibleName() == "Typewriter · Ctrl+Shift+T") typewriter = button;
    }
    QVERIFY(emphasis && typewriter);
    QCOMPARE(emphasis->text(), QString("e"));
    QCOMPARE(emphasis->toolTip(), QString("Emphasis · Ctrl+Shift+E"));
    QVERIFY(emphasis->font().italic());
    QCOMPARE(typewriter->text(), QString("T"));
    QCOMPARE(typewriter->toolTip(), QString("Typewriter · Ctrl+Shift+T"));
    QVERIFY(typewriter->font().styleHint() == QFont::Monospace && typewriter->font().fixedPitch());

    editor.setPlainText("hello world");
    auto cursor = editor.textCursor();
    cursor.setPosition(0);
    cursor.setPosition(5, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);
    emphasis->click();
    QCOMPARE(editor.toPlainText(), QString("\\emph{hello} world"));

    editor.setPlainText("hello world");
    cursor = editor.textCursor();
    cursor.setPosition(6);
    cursor.setPosition(11, QTextCursor::KeepAnchor);
    editor.setTextCursor(cursor);
    typewriter->click();
    QCOMPARE(editor.toPlainText(), QString("hello \\texttt{world}"));
}

void Tests::goToDefinition()
{
    QTemporaryDir dir;
    const QString rootPath = QDir::cleanPath(dir.filePath("root.tex"));
    const QString secPath = QDir::cleanPath(dir.filePath("sec.tex"));
    const QString bibPath = QDir::cleanPath(dir.filePath("refs.bib"));
    write(rootPath,
        "\\documentclass{article}\n"
        "\\bibliography{refs}\n"
        "\\begin{document}\n"
        "\\input{sec}\n"
        "See \\ref{eq:a}, \\eqref{eq:a}, \\autoref{eq:a}, \\cref{eq:a}, \\pageref{eq:a}.\n"
        "Cite \\cite{x,knuth84}, \\citep[see][p. 2]{knuth84}, \\textcite{knuth84}.\n"
        "Missing \\ref{no:such} and \\cite{no:bib}.\n"
        "\\end{document}\n");
    write(secPath,
        "\\section{Intro}\n"
        "\\begin{equation}\n"
        "  x = 1 \\label{eq:a}\n"
        "\\end{equation}\n");
    write(bibPath,
        "% bibliography\n"
        "@book{other,\n"
        "  title={Other}\n"
        "}\n"
        "@article{knuth84,\n"
        "  title={Literate programming}\n"
        "}\n");

    Window window;
    QVERIFY(window.openSource(rootPath));
    auto *rootEditor = window.currentEditor();
    auto placeOn = [](Editor *ed, const QString &needle, int offset = 0)
    {
        const int pos = ed->toPlainText().indexOf(needle);
        QVERIFY2(pos >= 0, qPrintable("missing needle: " + needle));
        QTextCursor c(ed->document());
        c.setPosition(pos + offset);
        ed->setTextCursor(c);
    };
    QAction *goDef = nullptr;
    for (QAction *action : window.actions())
        if (action->text() == "Go to definition") goDef = action;
    QVERIFY(goDef);
    QCOMPARE(goDef->shortcut(), QKeySequence("F12"));

    // \ref and variants resolve to sec.tex line 3.
    QString path;
    int line = 0;
    for (const QString &needle : {"\\ref{eq:a}", "\\eqref{eq:a}", "\\autoref{eq:a}", "\\cref{eq:a}", "\\pageref{eq:a}"})
    {
        placeOn(rootEditor, needle, needle.indexOf("eq:a") + 2);
        QVERIFY(rootEditor->definitionAt(path, line));
        QCOMPARE(path, secPath);
        QCOMPARE(line, 3);
    }

    // Multi-key \cite{x,knuth84}: caret in knuth84 resolves to refs.bib line 5; caret in x fails.
    placeOn(rootEditor, "\\cite{x,knuth84}", 10); // inside knuth84
    QVERIFY(rootEditor->definitionAt(path, line));
    QCOMPARE(path, bibPath);
    QCOMPARE(line, 5);
    placeOn(rootEditor, "\\cite{x,knuth84}", 6); // on x
    QVERIFY(!rootEditor->definitionAt(path, line));

    // Optional-arg and biblatex cite variants.
    placeOn(rootEditor, "\\citep[see][p. 2]{knuth84}", 20);
    QVERIFY(rootEditor->definitionAt(path, line));
    QCOMPARE(path, bibPath);
    QCOMPARE(line, 5);
    placeOn(rootEditor, "\\textcite{knuth84}", 12);
    QVERIFY(rootEditor->definitionAt(path, line));
    QCOMPARE(path, bibPath);
    QCOMPARE(line, 5);

    // Unknown keys return false and F12 reports "No definition found".
    placeOn(rootEditor, "\\ref{no:such}", 7);
    QVERIFY(!rootEditor->definitionAt(path, line));
    goDef->trigger();
    QCOMPARE(window.currentEditor(), rootEditor);
    QCOMPARE(window.m_status->text(), QString("No definition found"));

    // F12 opens sec.tex at line 3 and follows unsaved edits in open tabs.
    placeOn(rootEditor, "\\ref{eq:a}", 7);
    goDef->trigger();
    auto *secEditor = window.currentEditor();
    QCOMPARE(QDir::cleanPath(secEditor->property("path").toString()), secPath);
    QCOMPARE(secEditor->lineNumber(), 3);

    QTextCursor top(secEditor->document());
    top.setPosition(0);
    top.insertText("% extra 1\n% extra 2\n"); // moves \label{eq:a} to line 5 (unsaved)
    QVERIFY(window.openSource(rootPath));
    placeOn(rootEditor, "\\ref{eq:a}", 7);
    goDef->trigger();
    QCOMPARE(window.currentEditor(), secEditor);
    QCOMPARE(secEditor->lineNumber(), 5);

    // F12 on \cite{x,knuth84} opens refs.bib at line 5.
    QVERIFY(window.openSource(rootPath));
    placeOn(rootEditor, "\\cite{x,knuth84}", 10);
    goDef->trigger();
    QCOMPARE(QDir::cleanPath(window.currentEditor()->property("path").toString()), bibPath);
    QCOMPARE(window.currentEditor()->lineNumber(), 5);
}

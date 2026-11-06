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
#ifdef Q_OS_UNIX
#include <signal.h>
#endif

class Tests : public QObject
{
    Q_OBJECT
private slots:
    void rootFiles();
    void editorCommentUndo();
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
    QTemporaryDir dir(QDir::tempPath() + "/texlite file spaces-XXXXXX");
    QVERIFY(dir.isValid());
    QString path = dir.filePath("main file.tex");
    write(path, "\\documentclass{article}\n\\begin{document}\nHello $x^2+1$.\n\\end{document}\n");
    BuildRunner runner;
    QSignalSpy done(&runner, &BuildRunner::finished);
    QVERIFY(runner.start(path, "pdfLaTeX"));
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 30000);
    QVERIFY(done.at(0).at(0).toBool());
    QVERIFY(QFileInfo::exists(BuildRunner::pdfPath(path)));
    QVERIFY(QFileInfo::exists(QFileInfo(path).absolutePath() + "/.texlite-build/main file.synctex.gz"));
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

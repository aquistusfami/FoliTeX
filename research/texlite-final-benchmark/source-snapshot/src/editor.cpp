#include "editor.h"
#include <QWheelEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QFontDatabase>
#include <QApplication>
#include <algorithm>

SmoothScroll::SmoothScroll(QAbstractScrollArea *area)
    : QObject(area), m_area(area), m_animation(area->verticalScrollBar(), "value", this)
{
    m_animation.setDuration(140);
    m_animation.setEasingCurve(QEasingCurve::OutCubic);
    area->viewport()->installEventFilter(this);
    auto *bar = area->verticalScrollBar();
    connect(bar, &QScrollBar::sliderPressed, &m_animation, &QPropertyAnimation::stop);
}

bool SmoothScroll::eventFilter(QObject *, QEvent *event)
{
    if (event->type() != QEvent::Wheel) return false;
    auto *wheel = static_cast<QWheelEvent *>(event);
    if (wheel->modifiers().testFlag(Qt::ControlModifier)) return false;
    auto *bar = m_area->verticalScrollBar();
    if (!wheel->pixelDelta().isNull())
    {
        // Touchpads already provide pixel deltas and kinetic motion; do not animate them twice.
        m_animation.stop();
        if (wheel->pixelDelta().y() != 0)
            bar->setValue(bar->value() - wheel->pixelDelta().y());
        if (wheel->pixelDelta().x() != 0)
        {
            auto *horizontal = m_area->horizontalScrollBar();
            horizontal->setValue(horizontal->value() - wheel->pixelDelta().x());
        }
        wheel->accept();
        return true;
    }
    if (wheel->angleDelta().y() == 0) return false;
    int previousTarget = m_animation.state() == QAbstractAnimation::Running
        ? m_animation.endValue().toInt() : bar->value();
    int distance = qRound(wheel->angleDelta().y() / 120.0 * 72);
    int target = std::clamp(previousTarget - distance, bar->minimum(), bar->maximum());
    m_animation.stop();
    m_animation.setStartValue(bar->value());
    m_animation.setEndValue(target);
    m_animation.start();
    wheel->accept();
    return true;
}

LatexHighlighter::LatexHighlighter(QTextDocument *document) : QSyntaxHighlighter(document) {}

void LatexHighlighter::highlightBlock(const QString &text)
{
    static const QRegularExpression commands(R"(\\(?:[A-Za-z@]+|.))");
    static const QRegularExpression braces(R"([{}\[\]])");
    static const QRegularExpression comment(R"((?<!\\)(?:\\\\)*\K%.*$)");
    auto mark = [this, &text](const QRegularExpression &expression, const QColor &color)
    {
        auto matches = expression.globalMatch(text);
        while (matches.hasNext())
        {
            auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), color);
        }
    };
    mark(commands, QColor("#93B9D2"));
    mark(braces, QColor("#8996A5"));
    auto match = comment.match(text);
    if (match.hasMatch()) setFormat(match.capturedStart(), match.capturedLength(), QColor("#8793A2"));
}

Editor::Editor(QWidget *parent) : QTextEdit(parent)
{
    setAcceptRichText(false);
    setLineWrapMode(QTextEdit::NoWrap);
    setFrameShape(QFrame::NoFrame);
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * 4);
    document()->setDocumentMargin(18);
    QFont mono("Input Mono");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSizeF(11);
    setFont(mono);
    new LatexHighlighter(document());
    new SmoothScroll(this);
    m_completion = new QCompleter(QStringList{
        "\\begin", "\\end", "\\section", "\\subsection", "\\subsubsection", "\\chapter",
        "\\textbf", "\\textit", "\\emph", "\\label", "\\ref", "\\eqref", "\\cite", "\\include",
        "\\input", "\\includegraphics", "\\documentclass", "\\usepackage", "\\newcommand",
        "\\frac", "\\sqrt", "\\sum", "\\prod", "\\int", "\\lim", "\\infty", "\\partial",
        "\\alpha", "\\beta", "\\gamma", "\\delta", "\\epsilon", "\\theta", "\\lambda",
        "\\pi", "\\sigma", "\\phi", "\\omega", "\\left", "\\right", "\\mathrm", "\\mathbf"
    }, this);
    m_completion->setWidget(this);
    m_completion->setCaseSensitivity(Qt::CaseSensitive);
    m_completion->setCompletionMode(QCompleter::PopupCompletion);
    connect(m_completion, qOverload<const QString &>(&QCompleter::activated), this, &Editor::complete);
}

void Editor::complete(const QString &text)
{
    auto cursor = textCursor();
    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, m_completion->completionPrefix().size());
    cursor.insertText(text);
    setTextCursor(cursor);
}

void Editor::keyPressEvent(QKeyEvent *event)
{
    if (m_completion->popup()->isVisible() &&
        (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return || event->key() == Qt::Key_Escape || event->key() == Qt::Key_Tab))
    {
        event->ignore();
        return;
    }
    if (event->key() == Qt::Key_Tab && !event->modifiers())
    {
        textCursor().insertText("    ");
        return;
    }
    bool request = event->key() == Qt::Key_Space && event->modifiers() == Qt::ControlModifier;
    if (!request) QTextEdit::keyPressEvent(event);
    auto cursor = textCursor();
    QString before = cursor.block().text().left(cursor.positionInBlock());
    static const QRegularExpression prefix(R"((\\[A-Za-z@]*)$)");
    auto match = prefix.match(before);
    if (!match.hasMatch() || (!request && match.capturedLength() < 3))
    {
        m_completion->popup()->hide();
        return;
    }
    m_completion->setCompletionPrefix(match.captured());
    m_completion->complete(cursorRect());
}

void Editor::goToLine(int line, int column)
{
    auto block = document()->findBlockByNumber(std::max(0, line - 1));
    if (!block.isValid()) return;
    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, std::clamp(column, 0, block.length() - 1));
    setTextCursor(cursor);
    ensureCursorVisible();
    setFocus();
}

int Editor::lineNumber() const { return textCursor().blockNumber() + 1; }

void Editor::toggleComment()
{
    auto cursor = textCursor();
    int start = cursor.selectionStart(), end = cursor.selectionEnd();
    auto first = document()->findBlock(start);
    auto last = document()->findBlock(end);
    if (end > start && last.position() == end) last = last.previous();
    bool remove = true;
    for (auto block = first; block.isValid(); block = block.next())
    {
        if (!block.text().startsWith('%')) remove = false;
        if (block == last) break;
    }
    cursor.beginEditBlock();
    for (auto block = first; block.isValid(); block = block.next())
    {
        QTextCursor line(block);
        if (remove) line.deleteChar(); else line.insertText("% ");
        if (remove && block.text().startsWith(' ')) line.deleteChar();
        if (block == last) break;
    }
    cursor.endEditBlock();
}

#include "editor.h"
#include <QWheelEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QFontDatabase>
#include <QApplication>
#include <QStringListModel>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QHash>
#include <QProcess>
#include <QPainter>
#include <QMenu>
#include <QAbstractTextDocumentLayout>
#include <algorithm>

// U+2022 marks a placeholder in completion templates; it is never inserted into the source.
static const QChar placeholderMark(0x2022);

static bool escaped(const QTextDocument *document, int position) { return document->characterAt(position - 1) == '\\'; }

static const QStringList builtinCommands = {
    "\\documentclass{•}", "\\documentclass[•]{•}", "\\usepackage{•}", "\\usepackage[•]{•}", "\\begin{•}", "\\end{•}",
    "\\part{•}", "\\chapter{•}", "\\chapter*{•}", "\\section{•}", "\\section*{•}", "\\subsection{•}",
    "\\subsection*{•}", "\\subsubsection{•}", "\\subsubsection*{•}", "\\paragraph{•}", "\\subparagraph{•}",
    "\\title{•}", "\\author{•}", "\\date{•}", "\\today", "\\maketitle", "\\tableofcontents", "\\listoffigures",
    "\\listoftables", "\\appendix", "\\frontmatter", "\\mainmatter", "\\backmatter", "\\input{•}", "\\include{•}",
    "\\includeonly{•}", "\\bibliography{•}", "\\bibliographystyle{•}", "\\addbibresource{•}", "\\printbibliography",
    "\\cite{•}", "\\citep{•}", "\\citet{•}", "\\parencite{•}", "\\textcite{•}", "\\autocite{•}", "\\nocite{•}",
    "\\label{•}", "\\ref{•}", "\\eqref{•}", "\\pageref{•}", "\\autoref{•}", "\\cref{•}", "\\Cref{•}",
    "\\footnote{•}", "\\footnotemark", "\\footnotetext{•}", "\\caption{•}", "\\caption[•]{•}", "\\centering",
    "\\raggedright", "\\raggedleft", "\\includegraphics{•}", "\\includegraphics[width=•]{•}", "\\newcommand{•}{•}",
    "\\newcommand{•}[•]{•}", "\\renewcommand{•}{•}", "\\newenvironment{•}{•}{•}", "\\DeclareMathOperator{•}{•}",
    "\\newtheorem{•}{•}", "\\setlength{•}{•}", "\\addtolength{•}{•}", "\\setcounter{•}{•}", "\\addtocounter{•}{•}",
    "\\newpage", "\\clearpage", "\\cleardoublepage", "\\newline", "\\linebreak", "\\pagebreak", "\\noindent",
    "\\indent", "\\par", "\\hspace{•}", "\\hspace*{•}", "\\vspace{•}", "\\vspace*{•}", "\\hfill", "\\vfill",
    "\\smallskip", "\\medskip", "\\bigskip", "\\quad", "\\qquad", "\\item", "\\item[•]", "\\hline", "\\cline{•}",
    "\\multicolumn{•}{•}{•}", "\\multirow{•}{•}{•}", "\\toprule", "\\midrule", "\\bottomrule", "\\url{•}",
    "\\href{•}{•}", "\\textbf{•}", "\\textit{•}", "\\textsl{•}", "\\textsc{•}", "\\texttt{•}", "\\textsf{•}",
    "\\textrm{•}", "\\textmd{•}", "\\textup{•}", "\\textnormal{•}", "\\emph{•}", "\\underline{•}",
    "\\textcolor{•}{•}", "\\color{•}", "\\colorbox{•}{•}", "\\tiny", "\\scriptsize", "\\footnotesize", "\\small",
    "\\normalsize", "\\large", "\\Large", "\\LARGE", "\\huge", "\\Huge", "\\bfseries", "\\itshape", "\\ttfamily",
    "\\mbox{•}", "\\fbox{•}", "\\makebox[•]{•}", "\\parbox{•}{•}", "\\raisebox{•}{•}", "\\rule{•}{•}", "\\LaTeX",
    "\\TeX", "\\ldots", "\\dots", "\\textbackslash", "\\pagestyle{•}", "\\thispagestyle{•}", "\\pagenumbering{•}",
    "\\linewidth", "\\textwidth", "\\columnwidth", "\\baselineskip", "\\parindent", "\\parskip",
    "\\frac{•}{•}", "\\dfrac{•}{•}", "\\tfrac{•}{•}", "\\sqrt{•}", "\\sqrt[•]{•}", "\\binom{•}{•}", "\\sum",
    "\\sum_{•}^{•}", "\\prod", "\\prod_{•}^{•}", "\\int", "\\int_{•}^{•}", "\\iint", "\\iiint", "\\oint",
    "\\lim_{•}", "\\limsup", "\\liminf", "\\sup", "\\inf", "\\max", "\\min", "\\arg", "\\det", "\\exp",
    "\\log", "\\ln", "\\sin", "\\cos", "\\tan", "\\cot", "\\sec", "\\csc", "\\arcsin", "\\arccos",
    "\\arctan", "\\sinh", "\\cosh", "\\tanh", "\\infty", "\\partial", "\\nabla", "\\cdot", "\\cdots",
    "\\vdots", "\\ddots", "\\times", "\\div", "\\pm", "\\mp", "\\leq", "\\geq", "\\neq", "\\approx",
    "\\equiv", "\\sim", "\\simeq", "\\propto", "\\ll", "\\gg", "\\subset", "\\subseteq", "\\supset",
    "\\supseteq", "\\in", "\\notin", "\\ni", "\\cup", "\\cap", "\\setminus", "\\emptyset", "\\varnothing",
    "\\forall", "\\exists", "\\neg", "\\land", "\\lor", "\\implies", "\\iff", "\\to", "\\mapsto",
    "\\rightarrow", "\\leftarrow", "\\Rightarrow", "\\Leftarrow", "\\Leftrightarrow", "\\leftrightarrow",
    "\\uparrow", "\\downarrow", "\\left(", "\\right)", "\\left[", "\\right]", "\\left\\{", "\\right\\}",
    "\\left|", "\\right|", "\\langle", "\\rangle", "\\lfloor", "\\rfloor", "\\lceil", "\\rceil",
    "\\mathrm{•}", "\\mathbf{•}", "\\mathit{•}", "\\mathsf{•}", "\\mathtt{•}", "\\mathcal{•}", "\\mathbb{•}",
    "\\mathfrak{•}", "\\boldsymbol{•}", "\\operatorname{•}", "\\text{•}", "\\hat{•}", "\\widehat{•}", "\\bar{•}",
    "\\overline{•}", "\\tilde{•}", "\\widetilde{•}", "\\vec{•}", "\\dot{•}", "\\ddot{•}", "\\overbrace{•}^{•}",
    "\\underbrace{•}_{•}", "\\overset{•}{•}", "\\underset{•}{•}", "\\stackrel{•}{•}", "\\pmod{•}", "\\bmod",
    "\\mid", "\\parallel", "\\perp", "\\angle", "\\circ", "\\bullet", "\\star", "\\ast", "\\oplus",
    "\\otimes", "\\dagger", "\\ell", "\\hbar", "\\Re", "\\Im", "\\aleph", "\\prime", "\\displaystyle",
    "\\limits", "\\nonumber", "\\notag", "\\tag{•}", "\\intertext{•}", "\\substack{•}",
    "\\alpha", "\\beta", "\\gamma", "\\delta", "\\epsilon", "\\varepsilon", "\\zeta", "\\eta", "\\theta",
    "\\vartheta", "\\iota", "\\kappa", "\\lambda", "\\mu", "\\nu", "\\xi", "\\pi", "\\varpi", "\\rho",
    "\\varrho", "\\sigma", "\\tau", "\\upsilon", "\\phi", "\\varphi", "\\chi", "\\psi", "\\omega",
    "\\Gamma", "\\Delta", "\\Theta", "\\Lambda", "\\Xi", "\\Pi", "\\Sigma", "\\Upsilon", "\\Phi", "\\Psi",
    "\\Omega"
};

static const QStringList builtinEnvironments = {
    "document", "abstract", "itemize", "enumerate", "description", "figure", "figure*", "table", "table*",
    "tabular{•}", "tabular*{•}{•}", "tabularx{•}{•}", "longtable{•}", "array{•}", "center", "flushleft", "flushright",
    "quote", "quotation", "verse", "verbatim", "minipage{•}", "equation", "equation*", "align", "align*", "gather",
    "gather*", "multline", "multline*", "split", "aligned", "cases", "matrix", "pmatrix", "bmatrix", "Bmatrix",
    "vmatrix", "Vmatrix", "smallmatrix", "theorem", "lemma", "proof", "definition", "corollary", "proposition",
    "remark", "example", "frame", "frame{•}", "columns", "column{•}", "block{•}", "thebibliography{•}", "tikzpicture",
    "subfigure{•}", "lstlisting", "minted{•}", "titlepage"
};

// Package and class names come from the TeX Live ls-R databases, read once on first use.
static QStringList texFiles(const QString &suffix)
{
    static QHash<QString, QStringList> cache;
    if (cache.isEmpty())
    {
        cache.insert(".sty", {}); // Non-empty from here on, so a missing kpsewhich is not retried on every keystroke.
        QProcess kpsewhich;
        kpsewhich.start("kpsewhich", {"--all", "ls-R"});
        if (!kpsewhich.waitForFinished(3000)) return {};
        for (const auto &path : QString::fromLocal8Bit(kpsewhich.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts))
        {
            QFile file(path.trimmed());
            if (!file.open(QIODevice::ReadOnly)) continue;
            while (!file.atEnd())
            {
                QString name = QString::fromUtf8(file.readLine()).trimmed();
                if (name.endsWith(".sty") || name.endsWith(".cls")) cache[name.right(4)] << name.chopped(4);
            }
        }
    }
    return cache.value(suffix);
}

// shortcut: only .bib files next to open sources are read, upgrade by resolving \bibliography/\addbibresource paths.
static QStringList bibKeys(const QStringList &directories)
{
    static QHash<QString, QPair<QDateTime, QStringList>> cache;
    static const QRegularExpression entry(R"(@(?!string|comment|preamble)[A-Za-z]+\s*[{(]\s*([^,\s{}()]+)\s*,)",
        QRegularExpression::CaseInsensitiveOption);
    QStringList keys;
    for (const auto &directory : directories)
        for (const auto &info : QDir(directory).entryInfoList({"*.bib"}, QDir::Files))
        {
            auto &cached = cache[info.absoluteFilePath()];
            if (cached.first != info.lastModified())
            {
                QFile file(info.absoluteFilePath());
                if (!file.open(QIODevice::ReadOnly)) continue;
                cached = {info.lastModified(), {}};
                for (auto it = entry.globalMatch(QString::fromUtf8(file.readAll())); it.hasNext();)
                    cached.second << it.next().captured(1);
            }
            keys += cached.second;
        }
    return keys;
}

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
    mark(commands, QColor("#569CD6"));
    mark(braces, QColor("#FFD700"));
    auto match = comment.match(text);
    if (match.hasMatch()) setFormat(match.capturedStart(), match.capturedLength(), QColor("#6A9955"));
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
    m_gutter = new QWidget(this);
    m_gutter->installEventFilter(this);
    auto refreshGutter = [this] { setViewportMargins(gutterWidth(), 0, 0, 0); m_gutter->update(); };
    connect(document(), &QTextDocument::blockCountChanged, this, refreshGutter);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, m_gutter, qOverload<>(&QWidget::update));
    connect(this, &QTextEdit::textChanged, m_gutter, qOverload<>(&QWidget::update));
    connect(this, &QTextEdit::cursorPositionChanged, this, [this] { m_gutter->update(); updateHighlights(); });
    refreshGutter();
    updateHighlights();
    m_completion = new QCompleter(new QStringListModel(this), this);
    m_completion->setWidget(this);
    m_completion->setCaseSensitivity(Qt::CaseSensitive);
    m_completion->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    m_completion->setCompletionMode(QCompleter::PopupCompletion);
    m_completion->setMaxVisibleItems(12);
    connect(m_completion, qOverload<const QString &>(&QCompleter::activated), this, &Editor::complete);
}

void Editor::complete(const QString &text)
{
    auto cursor = textCursor();
    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, m_completion->completionPrefix().size());
    cursor.removeSelectedText();
    bool closed = document()->characterAt(cursor.position()) == '}';
    if (m_context == Context::Command)
    {
        setTextCursor(cursor);
        insertTemplate(text);
        if (text.startsWith("\\begin{")) QTimer::singleShot(0, this, [this] { updateCompletion(true); });
        return;
    }
    if (m_context == Context::Environment)
    {
        if (closed) cursor.deleteChar();
        setTextCursor(cursor);
        int arguments = text.indexOf(QRegularExpression(R"([{\[])"));
        QString name = arguments < 0 ? text : text.left(arguments);
        insertTemplate(name + "}" + (arguments < 0 ? QString() : text.mid(arguments)) + "\n    •\n\\end{" + name + "}");
        return;
    }
    cursor.insertText(text);
    if (closed) cursor.movePosition(QTextCursor::Right);
    setTextCursor(cursor);
}

void Editor::insertTemplate(QString text)
{
    auto cursor = textCursor();
    QString selected = cursor.selectedText().replace(QChar::ParagraphSeparator, '\n');
    if (!selected.isEmpty() && text.contains(placeholderMark)) text.replace(text.indexOf(placeholderMark), 1, selected);
    QString line = document()->findBlock(cursor.selectionStart()).text();
    static const QRegularExpression firstText(R"(\S|$)");
    text.replace('\n', '\n' + line.left(line.indexOf(firstText)));
    QString plain;
    QList<int> offsets;
    for (QChar c : text)
    {
        if (c == placeholderMark) offsets << plain.size(); else plain += c;
    }
    cursor.insertText(plain);
    int start = cursor.position() - plain.size();
    m_placeholders.clear();
    m_placeholder = -1;
    setTextCursor(cursor);
    if (offsets.isEmpty()) return;
    offsets << plain.size();
    for (int offset : offsets)
    {
        QTextCursor position(document());
        position.setPosition(start + offset);
        m_placeholders << position;
    }
    jumpPlaceholder(1);
}

bool Editor::jumpPlaceholder(int step)
{
    int next = m_placeholder + step;
    if (m_placeholders.isEmpty() || next < 0 || next >= m_placeholders.size()) return false;
    m_placeholder = next;
    setTextCursor(m_placeholders[next]);
    // The last mark is the end of the snippet; reaching it ends placeholder navigation.
    if (next == m_placeholders.size() - 1) { m_placeholders.clear(); m_placeholder = -1; }
    return true;
}

void Editor::insertEnvironment()
{
    auto cursor = textCursor();
    // shortcut: a selection is not wrapped in the environment, upgrade if wrapping is requested.
    cursor.clearSelection();
    setTextCursor(cursor);
    insertTemplate("\\begin{•}");
    updateCompletion(true);
}

QString Editor::closingFor(const QString &textBefore)
{
    static const QRegularExpression token(R"(%[^\n]*|\\begin\s*\{([^{}]*)\}|\\end\s*\{([^{}]*)\}|\\.|[{}])");
    QStringList open;
    for (auto it = token.globalMatch(textBefore); it.hasNext();)
    {
        auto match = it.next();
        if (match.hasCaptured(1)) open << "\\end{" + match.captured(1) + "}";
        else if (match.hasCaptured(2))
        {
            int index = open.lastIndexOf("\\end{" + match.captured(2) + "}");
            if (index >= 0) open.resize(index);
        }
        else if (match.captured() == "{") open << "}";
        else if (match.captured() == "}" && !open.isEmpty() && open.last() == "}") open.removeLast();
    }
    return open.isEmpty() ? QString() : open.last();
}

void Editor::closeEnvironment()
{
    QString closing = closingFor(toPlainText().left(textCursor().position()));
    if (!closing.isEmpty()) textCursor().insertText(closing);
}

// shortcut: rescans the whole document after each edit when completion needs it, upgrade to incremental scanning for huge files.
void Editor::scan()
{
    if (m_scanRevision == document()->revision()) return;
    m_scanRevision = document()->revision();
    m_macros.clear(); m_environments.clear(); m_labels.clear();
    static const QRegularExpression definition(
        R"(\\(?:(?:re)?newcommand|providecommand|DeclareMathOperator)\*?\s*\{?\s*(\\[A-Za-z@]+)\s*\}?\s*(?:\[(\d)\])?)"
        R"(|\\def\s*(\\[A-Za-z@]+)|\\newenvironment\*?\s*\{([A-Za-z*]+)\}\s*(?:\[(\d)\])?|\\label\{([^{}]+)\})");
    for (auto it = definition.globalMatch(toPlainText()); it.hasNext();)
    {
        auto match = it.next();
        if (match.hasCaptured(1)) m_macros << match.captured(1) + QString("{•}").repeated(match.captured(2).toInt());
        else if (match.hasCaptured(3)) m_macros << match.captured(3);
        else if (match.hasCaptured(4)) m_environments << match.captured(4) + QString("{•}").repeated(match.captured(5).toInt());
        else m_labels << match.captured(6);
    }
}

void Editor::updateCompletion(bool request)
{
    auto cursor = textCursor();
    QString before = cursor.block().text().left(cursor.positionInBlock());
    static const QRegularExpression environment(R"(\\begin\{([A-Za-z*]*)$)");
    static const QRegularExpression reference(R"(\\[A-Za-z]*ref\*?\{([^{},]*)$)");
    static const QRegularExpression citation(R"(\\[A-Za-z]*cite[A-Za-z]*\*?(?:\[[^\]]*\])*\{(?:[^{}]*,)?\s*([^{},\s]*)$)");
    static const QRegularExpression package(R"(\\(?:usepackage|RequirePackage)(?:\[[^\]]*\])?\{(?:[^{}]*,)?\s*([^{},\s]*)$)");
    static const QRegularExpression documentClass(R"(\\documentclass(?:\[[^\]]*\])?\{([^{},\s]*)$)");
    static const QRegularExpression command(R"(\\[A-Za-z@]*$)");
    // Open tabs form the project: their macros, environments, labels and .bib folders are offered.
    auto editors = window()->findChildren<Editor *>();
    if (!editors.contains(this)) editors << this;
    auto collect = [&editors](QStringList Editor::*list)
    {
        QStringList all;
        for (auto *editor : editors) { editor->scan(); all += editor->*list; }
        return all;
    };
    QStringList words;
    QRegularExpressionMatch match;
    if ((match = environment.match(before)).hasMatch())
    {
        m_context = Context::Environment;
        words = builtinEnvironments + collect(&Editor::m_environments);
    }
    else if ((match = reference.match(before)).hasMatch())
    {
        m_context = Context::Reference;
        words = collect(&Editor::m_labels);
    }
    else if ((match = citation.match(before)).hasMatch())
    {
        m_context = Context::Citation;
        QStringList directories;
        for (auto *editor : editors)
            if (!editor->property("path").toString().isEmpty())
                directories << QFileInfo(editor->property("path").toString()).absolutePath();
        directories.removeDuplicates();
        words = bibKeys(directories);
    }
    else if ((match = package.match(before)).hasMatch())
    {
        m_context = Context::Name;
        words = texFiles(".sty");
    }
    else if ((match = documentClass.match(before)).hasMatch())
    {
        m_context = Context::Name;
        words = texFiles(".cls");
    }
    else if ((match = command.match(before)).hasMatch() && (request || match.capturedLength() >= 3))
    {
        m_context = Context::Command;
        words = builtinCommands + collect(&Editor::m_macros);
    }
    else
    {
        m_completion->popup()->hide();
        return;
    }
    words.sort();
    words.removeDuplicates();
    auto *model = static_cast<QStringListModel *>(m_completion->model());
    if (model->stringList() != words) model->setStringList(words);
    m_completion->setCompletionPrefix(m_context == Context::Command ? match.captured() : match.captured(1));
    if (!m_completion->completionCount())
    {
        m_completion->popup()->hide();
        return;
    }
    auto *popup = m_completion->popup();
    QRect rect = cursorRect();
    rect.setWidth(popup->sizeHintForColumn(0) + popup->verticalScrollBar()->sizeHint().width() + 16);
    m_completion->complete(rect);
    popup->setCurrentIndex(m_completion->completionModel()->index(0, 0));
}

void Editor::keyPressEvent(QKeyEvent *event)
{
    if (m_completion->popup()->isVisible() &&
        (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return || event->key() == Qt::Key_Escape || event->key() == Qt::Key_Tab))
    {
        event->ignore();
        return;
    }
    bool control = event->modifiers() == Qt::ControlModifier;
    if (event->key() == Qt::Key_Escape) { m_placeholders.clear(); m_placeholder = -1; }
    if (((event->key() == Qt::Key_Tab && !event->modifiers()) || (control && event->key() == Qt::Key_Right)) && jumpPlaceholder(1))
        return;
    if (control && event->key() == Qt::Key_Left && jumpPlaceholder(-1)) return;
    if (event->key() == Qt::Key_Tab && !event->modifiers())
    {
        textCursor().insertText("    ");
        return;
    }
    static const QString opening = "{[(", closing = "}])";
    auto cursor = textCursor();
    QChar next = document()->characterAt(cursor.position()), previous = document()->characterAt(cursor.position() - 1);
    QString typed = event->text();
    if (typed.size() == 1 && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)))
    {
        int open = opening.indexOf(typed[0]);
        if (closing.contains(typed[0]) && next == typed[0] && !cursor.hasSelection())
        {
            moveCursor(QTextCursor::NextCharacter);
            return;
        }
        // Brackets auto-close unless escaped (\{) or typed directly before a word.
        if (open >= 0 && previous != '\\' && (cursor.hasSelection() || !next.isLetterOrNumber()))
        {
            QString selected = cursor.selectedText().replace(QChar::ParagraphSeparator, '\n');
            cursor.insertText(typed + selected + closing[open]);
            if (selected.isEmpty()) cursor.movePosition(QTextCursor::PreviousCharacter);
            setTextCursor(cursor);
            updateCompletion(false);
            return;
        }
    }
    if (event->key() == Qt::Key_Backspace && !event->modifiers() && !cursor.hasSelection()
        && opening.indexOf(previous) >= 0 && closing.indexOf(next) == opening.indexOf(previous) && !escaped(document(), cursor.position() - 1))
    {
        cursor.beginEditBlock();
        cursor.deletePreviousChar();
        cursor.deleteChar();
        cursor.endEditBlock();
        updateCompletion(false);
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && !event->modifiers())
    {
        static const QRegularExpression indentation(R"(^\s*)");
        QString indent = indentation.match(cursor.block().text()).captured();
        cursor.insertText("\n" + indent);
        setTextCursor(cursor);
        m_completion->popup()->hide();
        return;
    }
    bool request = event->key() == Qt::Key_Space && control;
    if (!request) QTextEdit::keyPressEvent(event);
    updateCompletion(request);
}

int Editor::gutterWidth() const
{
    int digits = std::max(3, int(QString::number(document()->blockCount()).size()));
    return fontMetrics().horizontalAdvance(QString(digits, '9')) + 20;
}

void Editor::resizeEvent(QResizeEvent *event)
{
    QTextEdit::resizeEvent(event);
    auto area = contentsRect();
    m_gutter->setGeometry(area.left(), area.top(), gutterWidth(), area.height());
}

bool Editor::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_gutter && event->type() == QEvent::Paint) { paintGutter(); return true; }
    return QTextEdit::eventFilter(object, event);
}

void Editor::paintGutter()
{
    QPainter painter(m_gutter);
    painter.fillRect(m_gutter->rect(), palette().base());
    painter.setFont(font());
    int scroll = verticalScrollBar()->value();
    int current = textCursor().blockNumber();
    auto *layout = document()->documentLayout();
    // Only visible blocks are painted, so the cost does not grow with document length.
    for (auto block = cursorForPosition(QPoint(0, 0)).block(); block.isValid(); block = block.next())
    {
        QRectF rect = layout->blockBoundingRect(block).translated(0, -scroll);
        if (rect.top() > m_gutter->height()) break;
        painter.setPen(block.blockNumber() == current ? QColor("#CCCCCC") : QColor("#6E7681"));
        painter.drawText(QRectF(0, rect.top(), m_gutter->width() - 12, rect.height()), Qt::AlignRight | Qt::AlignTop,
            QString::number(block.blockNumber() + 1));
    }
}

// shortcut: scans at most 20000 characters and ignores comments, upgrade with a tokenizer if long-range matching matters.
static int matchingBracket(const QTextDocument *document, int position)
{
    static const QString opening = "{[(", closing = "}])";
    QChar bracket = document->characterAt(position);
    int forward = opening.indexOf(bracket), backward = closing.indexOf(bracket);
    if ((forward < 0 && backward < 0) || escaped(document, position)) return -1;
    QChar other = forward >= 0 ? closing[forward] : opening[backward];
    int step = forward >= 0 ? 1 : -1, depth = 0;
    for (int i = position, n = 0; i >= 0 && i < document->characterCount() && n < 20000; i += step, ++n)
    {
        QChar c = document->characterAt(i);
        if ((c == bracket || c == other) && !escaped(document, i) && !(depth += c == bracket ? 1 : -1)) return i;
    }
    return -1;
}

void Editor::updateHighlights()
{
    QList<QTextEdit::ExtraSelection> selections;
    QTextEdit::ExtraSelection line;
    line.format.setBackground(QColor("#282828"));
    line.format.setProperty(QTextFormat::FullWidthSelection, true);
    line.cursor = textCursor();
    line.cursor.clearSelection();
    selections << line;
    int position = textCursor().position();
    for (int candidate : {position - 1, position})
    {
        int match = candidate >= 0 ? matchingBracket(document(), candidate) : -1;
        if (match < 0) continue;
        for (int at : {candidate, match})
        {
            QTextEdit::ExtraSelection bracket;
            bracket.format.setBackground(QColor(0, 100, 0, 110));
            bracket.cursor = QTextCursor(document());
            bracket.cursor.setPosition(at);
            bracket.cursor.setPosition(at + 1, QTextCursor::KeepAnchor);
            selections << bracket;
        }
        break;
    }
    setExtraSelections(selections);
}

QList<OutlineEntry> Editor::outline(const QString &text)
{
    static const QStringList levels = {"part", "chapter", "section", "subsection", "subsubsection", "paragraph", "subparagraph"};
    static const QRegularExpression command(
        R"(\\(part|chapter|section|subsection|subsubsection|paragraph|subparagraph|label|input|include|subfile|todo)\b\*?\s*(?:\[[^\]]*\])?\{((?:[^{}]|\{[^{}]*\})*)\})");
    static const QRegularExpression todoComment(R"(^%\s*TODO:?\s*(.*))", QRegularExpression::CaseInsensitiveOption);
    QList<OutlineEntry> entries;
    int number = 0;
    for (const auto &line : QStringView(text).split('\n'))
    {
        ++number;
        if (!line.contains('\\') && !line.contains('%')) continue;
        qsizetype comment = -1;
        for (qsizetype i = 0; i < line.size() && comment < 0; ++i)
            if (line[i] == '\\') ++i; else if (line[i] == '%') comment = i;
        if (comment >= 0)
        {
            auto todo = todoComment.matchView(line.mid(comment));
            if (todo.hasMatch()) entries.append({OutlineEntry::Todo, 7, number, todo.captured(1).trimmed()});
        }
        for (auto it = command.globalMatchView(comment < 0 ? line : line.left(comment)); it.hasNext();)
        {
            auto match = it.next();
            QString name = match.captured(1), argument = match.captured(2).trimmed();
            int level = levels.indexOf(name);
            if (level >= 0) entries.append({OutlineEntry::Section, level, number, argument});
            else if (name == "label") entries.append({OutlineEntry::Label, 7, number, argument});
            else if (name == "todo") entries.append({OutlineEntry::Todo, 7, number, argument});
            else entries.append({OutlineEntry::Include, 7, number, argument});
        }
    }
    return entries;
}

void Editor::setWordWrap(bool on)
{
    setLineWrapMode(on ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
    // Long LaTeX tokens (URLs, \\verb) still break when no word boundary fits.
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
}

void Editor::contextMenuEvent(QContextMenuEvent *event)
{
    std::unique_ptr<QMenu> menu(createStandardContextMenu(event->pos()));
    menu->addSeparator();
    auto *wrap = menu->addAction("Word Wrap\tAlt+Z");
    wrap->setCheckable(true);
    wrap->setChecked(wordWrap());
    connect(wrap, &QAction::toggled, this, &Editor::wordWrapToggled);
    menu->exec(event->globalPos());
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

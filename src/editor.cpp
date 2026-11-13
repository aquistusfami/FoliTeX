#include "editor.h"
#include <QWheelEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QListView>
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
#include <future>
#include <QCryptographicHash>
#include <QLabel>
#include <QScreen>
#include <QImageReader>
#include <QTemporaryDir>
#include <QHBoxLayout>
#include <QToolButton>
#include <QToolTip>
#include "window.h"
#include "completiondata.h"
#include <QMenu>
#include <QHelpEvent>
#include <QAbstractTextDocumentLayout>
#include <algorithm>
#include <cmath>

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

// Package and class names (and where each .sty lives) from the TeX Live ls-R databases.
struct TexTree
{
    QStringList styles, classes;
    QHash<QString, QString> styleDirectories; // package name -> directory; directory strings are shared
};

static TexTree loadTexTree()
{
    TexTree tree;
    QProcess kpsewhich;
    kpsewhich.start("kpsewhich", {"--all", "ls-R"});
    if (!kpsewhich.waitForFinished(3000)) return tree;
    for (const auto &path : QString::fromLocal8Bit(kpsewhich.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts))
    {
        QFile file(path.trimmed());
        if (!file.open(QIODevice::ReadOnly)) continue;
        QString root = QFileInfo(file.fileName()).absolutePath(), directory = root;
        // ls-R lists "path:" lines followed by the names in that directory.
        while (!file.atEnd())
        {
            QString name = QString::fromUtf8(file.readLine()).trimmed();
            if (name.endsWith(':')) directory = QDir::cleanPath(root + '/' + name.chopped(1));
            else if (name.endsWith(".sty") && !tree.styleDirectories.contains(name.chopped(4)))
            {
                tree.styles << name.chopped(4);
                tree.styleDirectories.insert(name.chopped(4), directory);
            }
            else if (name.endsWith(".cls")) tree.classes << name.chopped(4);
        }
    }
    return tree;
}

// Started in the background by the first editor, so the first \usepackage{ does not wait for it.
static std::shared_future<TexTree> &texTreeLoad()
{
    static std::shared_future<TexTree> load = std::async(std::launch::async, loadTexTree).share();
    return load;
}

static const TexTree &texTree() { return texTreeLoad().get(); }

// Public commands and environments of a package without curated data: \newcommand-style
// definitions in its .sty and in the packages it requires (one level). \def and @-names are skipped.
static PackageWords scannedPackage(const QString &name, int depth = 0)
{
    static QHash<QString, PackageWords> cache;
    if (auto found = cache.constFind(name); found != cache.cend()) return *found;
    PackageWords words;
    QString directory = texTree().styleDirectories.value(name);
    QFile file(directory + '/' + name + ".sty");
    if (!directory.isEmpty() && file.open(QIODevice::ReadOnly))
    {
        static const QRegularExpression definition(
            R"(\\(newcommand|providecommand|DeclareRobustCommand|DeclareMathOperator|NewDocumentCommand|DeclareDocumentCommand|NewExpandableDocumentCommand)\*?\s*\{?\s*\\([A-Za-z]+)(?![A-Za-z@])\s*\}?\s*(?:\[(\d)\]|\{([^{}]*)\})?)"
            R"(|\\(?:newenvironment|NewDocumentEnvironment)\*?\s*\{([A-Za-z*]+)\})"
            R"(|\\RequirePackage(?:\[[^\]]*\])?\{([^{}]+)\})");
        QString text = QString::fromLatin1(file.readAll());
        for (auto it = definition.globalMatch(text); it.hasNext();)
        {
            auto match = it.next();
            if (match.hasCaptured(2))
            {
                // [n] counts arguments; an xparse spec counts its mandatory "m" arguments.
                int arguments = match.captured(3).toInt();
                if (match.captured(1).contains("DocumentCommand")) arguments = match.captured(4).count('m');
                words.commands << "\\" + match.captured(2) + QString("{•}").repeated(arguments);
            }
            else if (match.hasCaptured(5)) words.environments << match.captured(5);
            else if (depth == 0)
                for (const auto &required : match.captured(6).split(',', Qt::SkipEmptyParts))
                {
                    PackageWords inner = scannedPackage(required.trimmed(), 1);
                    words.commands += inner.commands;
                    words.environments += inner.environments;
                }
        }
        words.commands.removeDuplicates();
        words.environments.removeDuplicates();
    }
    cache.insert(name, words);
    return words;
}

// Source text: the open tab's (unsaved edits included), else the file on disk.
static QString sourceText(const QString &path, const QList<Editor *> &editors)
{
    for (auto *editor : editors)
        if (editor->property("path").toString() == path) return editor->toPlainText();
    static QHash<QString, QPair<QDateTime, QString>> cache;
    QFileInfo info(path);
    if (!info.isFile()) return {};
    auto &cached = cache[path];
    if (cached.first != info.lastModified())
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        cached = {info.lastModified(), QString::fromUtf8(file.readAll())};
    }
    return cached.second;
}

struct ProjectRefs { QStringList labels, bibFiles; };

// Follows \input/\include/\subfile from the root as the build does, collecting labels and bibliographies.
static ProjectRefs projectRefs(const QString &root, const QList<Editor *> &editors)
{
    static const QRegularExpression command(
        R"(\\(input|include|subfile|bibliography|addbibresource|label)\s*(?:\[[^\]]*\])?\{([^{}]+)\})");
    static const QRegularExpression comment(R"((?<!\\)(?:\\\\)*\K%.*$)", QRegularExpression::MultilineOption);
    QDir base = QFileInfo(root).absoluteDir();
    ProjectRefs refs;
    QStringList queue{root};
    QSet<QString> seen;
    while (!queue.isEmpty() && seen.size() < 300)
    {
        QString path = queue.takeFirst();
        if (seen.contains(path)) continue;
        seen.insert(path);
        QString text = sourceText(path, editors);
        text.remove(comment);
        for (auto it = command.globalMatch(text); it.hasNext();)
        {
            auto match = it.next();
            QString kind = match.captured(1);
            if (kind == "label") { refs.labels << match.captured(2).trimmed(); continue; }
            bool bib = kind == "bibliography" || kind == "addbibresource";
            QString suffix = bib ? ".bib" : ".tex";
            for (const auto &name : match.captured(2).split(',', Qt::SkipEmptyParts))
            {
                QString file = QDir::cleanPath(base.absoluteFilePath(name.trimmed()));
                if (!file.endsWith(suffix) && QFileInfo(file + suffix).isFile()) file += suffix; // TeX tries name.tex first
                (bib ? refs.bibFiles : queue) << file;
            }
        }
    }
    return refs;
}

static QStringList bibKeys(const QStringList &files)
{
    static QHash<QString, QPair<QDateTime, QStringList>> cache;
    static const QRegularExpression entry(R"(@(?!string|comment|preamble)[A-Za-z]+\s*[{(]\s*([^,\s{}()]+)\s*,)",
        QRegularExpression::CaseInsensitiveOption);
    QStringList keys;
    for (const auto &path : files)
    {
        QFileInfo info(path);
        if (!info.isFile()) continue;
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

static bool &lightFlag()
{
    static bool light = false;
    return light;
}

void setLightTheme(bool on) { lightFlag() = on; }
bool lightTheme() { return lightFlag(); }

// Dark Modern (the default) -> Light Modern, as in VS Code.
static const QHash<QString, QString> &lightColors()
{
    static const QHash<QString, QString> colors = {
        {"#0E1013", "#E4E4E4"}, {"#121417", "#EBEBEB"}, {"#16181B", "#FFFFFF"}, {"#1B1F24", "#F8F8F8"},
        {"#1C2025", "#FFFFFF"}, {"#1D2024", "#F2F4F7"}, {"#20252B", "#E8E8E8"}, {"#22272E", "#E0E0E0"},
        {"#29333B", "#DCE6F2"}, {"#303842", "#CECECE"}, {"#35434F", "#ADD6FF"}, {"#4F6F6B", "#3F7A73"},
        {"#5C807B", "#4E8D85"}, {"#7C8794", "#69727C"}, {"#6F9692", "#3F7A73"}, {"#7DA6A0", "#3F7A73"},
        {"#8793A2", "#6A737D"}, {"#9AA6B5", "#57606A"}, {"#ADB7C4", "#3B3B3B"}, {"#B9C3CF", "#1F1F1F"},
        {"#D2D7DE", "#1F1F1F"}, {"#E0E5EC", "#000000"}, {"#7FA8C9", "#2F5F8A"}, {"#79BFD6", "#1C6E8C"},
        {"#B294BB", "#7A4E8C"}, {"#A074C4", "#8250DF"}, {"#CCA700", "#9A6700"}, {"#C8B466", "#7A6420"},
        {"#8DC149", "#4D8C1A"}, {"#CBCB41", "#8A8A00"}, {"#F14C4C", "#E51400"}, {"#F48771", "#C72E0F"},
        {"#7DC4B4", "#1F7A70"}, {"#DCDCAA", "#795E26"}, {"#C9A28A", "#9A4A2E"}, {"#A8C98A", "#3D7A2E"},
        {"#CDB483", "#9A6700"}, {"#6F9080", "#4A7460"}, {"#B084B0", "#8A5A8A"}, {"#6B9DB8", "#3E6C8A"},
        {"#1A1E23", "#F8F8F8"}, {"#1F252B", "#F2F4F7"}, {"#464E58", "#B4BAC2"},
        {"#555555", "#555555"}, {"#CC3E44", "#CC3E44"}}; // the last two read on both themes
    return colors;
}

QColor themeColor(const QString &dark)
{
    return QColor(lightFlag() ? lightColors().value(dark.toUpper(), dark) : dark);
}

QString themed(QString text)
{
    if (!lightFlag()) return text;
    for (auto it = lightColors().cbegin(); it != lightColors().cend(); ++it) text.replace(it.key(), it.value(), Qt::CaseInsensitive);
    return text;
}

SmoothScroll::SmoothScroll(QAbstractScrollArea *area, int wheelStep, double touchpadFactor)
    : QObject(area), m_area(area), m_animation(area->verticalScrollBar(), "value", this), m_wheelStep(wheelStep),
      m_touchpadFactor(touchpadFactor)
{
    m_animation.setEasingCurve(QEasingCurve::OutCubic);
    area->viewport()->installEventFilter(this);
    auto *bar = area->verticalScrollBar();
    connect(bar, &QScrollBar::sliderPressed, &m_animation, &QPropertyAnimation::stop);
}

bool SmoothScroll::eventFilter(QObject *, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) { m_animation.stop(); return false; }
    if (event->type() != QEvent::Wheel) return false;
    auto *wheel = static_cast<QWheelEvent *>(event);
    if (wheel->modifiers().testFlag(Qt::ControlModifier)) return false;
    auto *bar = m_area->verticalScrollBar();
    qint64 time = qint64(wheel->timestamp());
    switch (wheel->phase())
    {
    case Qt::ScrollBegin:
        m_animation.stop();
        m_samples.clear();
        m_systemMomentum = false;
        break;
    case Qt::ScrollMomentum:
        m_systemMomentum = true; // the platform animates inertia itself (macOS)
        break;
    case Qt::ScrollEnd:
    {
        // Wayland/libinput stop when the fingers lift; continue with the finger's velocity.
        qint64 first = m_samples.isEmpty() ? time : m_samples.first().first;
        bool moving = !m_samples.isEmpty() && time - m_samples.last().first < 60;
        double pixels = 0;
        for (const auto &sample : m_samples) pixels += sample.second;
        double velocity = pixels / std::max<qint64>(16, time - first); // px per ms
        m_samples.clear();
        if (!m_systemMomentum && moving && std::abs(velocity) > 0.3)
        {
            constexpr int duration = 800;
            // OutCubic starts at 3 × distance / duration, so this distance matches the finger speed.
            int target = std::clamp(bar->value() - qRound(velocity * duration / 3), bar->minimum(), bar->maximum());
            m_animation.stop();
            m_animation.setDuration(duration);
            m_animation.setStartValue(bar->value());
            m_animation.setEndValue(target);
            m_animation.start();
        }
        wheel->accept();
        return true;
    }
    default:
        break;
    }
    // Qt on Wayland gives mouse wheel notches a small pixelDelta (15) as well as angleDelta (120);
    // only phased events come from touchpads, so unphased notches use the wheel step.
    bool notch = wheel->phase() == Qt::NoScrollPhase && wheel->angleDelta().y() != 0;
    if (!wheel->pixelDelta().isNull() && !notch)
    {
        m_animation.stop();
        if (wheel->pixelDelta().y() != 0)
        {
            int pixels = qRound(wheel->pixelDelta().y() * m_touchpadFactor);
            bar->setValue(bar->value() - pixels);
            m_samples.append({time, pixels});
            while (m_samples.size() > 1 && time - m_samples.first().first > 100) m_samples.removeFirst();
        }
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
    int distance = qRound(wheel->angleDelta().y() / 120.0 * m_wheelStep);
    int target = std::clamp(previousTarget - distance, bar->minimum(), bar->maximum());
    m_animation.stop();
    m_animation.setDuration(140);
    m_animation.setStartValue(bar->value());
    m_animation.setEndValue(target);
    m_animation.start();
    wheel->accept();
    return true;
}

LatexHighlighter::LatexHighlighter(QTextDocument *document) : QSyntaxHighlighter(document) {}

namespace
{
// Block state: brace depth (bits 0-7), mode (bits 8-11), environment index (bits 12-19).
enum Mode { Text, InlineMath, DisplayMath, MathEnvironment, Verbatim };
const QStringList mathEnvironments = {"equation", "equation*", "align", "align*", "gather", "gather*", "multline", "multline*",
    "flalign", "flalign*", "alignat", "alignat*", "eqnarray", "eqnarray*", "displaymath", "math"};
const QStringList verbatimEnvironments = {"verbatim", "verbatim*", "Verbatim", "lstlisting", "minted", "comment", "filecontents"};
const QStringList keywordCommands = {"begin", "end", "item", "part", "chapter", "section", "subsection", "subsubsection",
    "paragraph", "subparagraph", "frametitle", "framesubtitle", "documentclass", "usepackage", "input", "include",
    "maketitle", "tableofcontents", "newcommand", "renewcommand", "def", "usetheme", "title", "author", "date", "label",
    "ref", "eqref", "cite"};
const QStringList headingCommands = {"part", "chapter", "section", "subsection", "subsubsection", "paragraph",
    "subparagraph", "frametitle", "framesubtitle", "title", "caption"};
const QStringList keyCommands = {"label", "ref", "eqref", "pageref", "autoref", "cref", "Cref", "nameref", "cite", "citep",
    "citet", "parencite", "textcite", "autocite", "footcite", "nocite", "includegraphics", "input", "include",
    "usepackage", "documentclass", "bibliography", "addbibresource", "usetheme", "url", "href"};
const QStringList textInMath = {"text", "textrm", "textbf", "textit", "mbox", "intertext", "operatorname"};

struct Formats
{
    QTextCharFormat command, keyword, heading, environment, key, math, mathCommand, special, comment, plain;
    QTextCharFormat braces[3];
};

const Formats &formats()
{
    static const Formats formats = []
    {
        // LaTeX Workshop roles in muted hues that sit with the teal chrome; themeColor() maps them for the light theme.
        Formats f;
        auto color = [](QTextCharFormat &format, const char *hex) { format.setForeground(themeColor(hex)); };
        color(f.command, "#7FA8C9");
        color(f.keyword, "#B294BB");
        color(f.heading, "#DCDCAA");
        f.heading.setFontWeight(QFont::Bold);
        color(f.environment, "#7DC4B4");
        color(f.key, "#C9A28A");
        color(f.math, "#A8C98A");
        color(f.mathCommand, "#79BFD6");
        color(f.special, "#CDB483");
        color(f.comment, "#6F9080");
        f.comment.setFontItalic(true);
        color(f.braces[0], "#C8B466");
        color(f.braces[1], "#B084B0");
        color(f.braces[2], "#6B9DB8");
        return f;
    }();
    return formats;
}
}

void LatexHighlighter::highlightBlock(const QString &text)
{
    const Formats &f = formats();
    int state = std::max(0, previousBlockState());
    int depth = state & 0xFF, mode = (state >> 8) & 0xF, environment = (state >> 12) & 0xFF;
    // A blank line ends inline math (as in TeX) and resets bracket depth, so one stray $ or { only
    // recolours its paragraph, not every later line of a long file.
    if (text.trimmed().isEmpty())
    {
        depth = 0;
        if (mode == InlineMath) mode = Text;
    }
    bool math = mode == InlineMath || mode == DisplayMath || mode == MathEnvironment;
    // Arguments coloured after the scan: headings and keys over plain text, \text{} back to plain in math.
    enum Argument { None, Heading, Key, EnvironmentName, PlainText };
    struct Range { int start, end; Argument kind; };
    QList<Range> ranges;
    Argument pending = None;
    int pendingDepth = 0; // brace depth where the command stood; text inside its [options] keeps it pending
    int argumentStart = -1, argumentDepth = -1;
    Argument argumentKind = None;
    const int n = int(text.size());
    auto isSpecial = [](QChar c) { return c == '\\' || c == '%' || c == '$' || c == '{' || c == '}' || c == '[' || c == ']' || c == '&' || c == '~'; };
    for (int i = 0; i < n;)
    {
        if (mode == Verbatim)
        {
            // Raw until \end{name}; that line continues normally from the \end.
            int close = int(text.indexOf("\\end{" + verbatimEnvironments.value(environment) + "}", i));
            if (close < 0) break;
            mode = Text;
            i = close;
            continue;
        }
        const QChar c = text[i];
        if (c == '%')
        {
            setFormat(i, n - i, f.comment);
            break;
        }
        if (c == '\\' && i + 1 < n && text[i + 1].isLetter())
        {
            int end = i + 1;
            while (end < n && (text[end].isLetter() || text[end] == '@')) ++end;
            const QString name = text.mid(i + 1, end - i - 1);
            if (end < n && text[end] == '*') ++end;
            setFormat(i, end - i, keywordCommands.contains(name) ? f.keyword : math ? f.mathCommand : f.command);
            pendingDepth = depth;
            if (name == "begin" || name == "end") pending = EnvironmentName;
            else if (headingCommands.contains(name) && !math) pending = Heading;
            else if (keyCommands.contains(name)) pending = Key;
            else if (math && textInMath.contains(name)) pending = PlainText;
            else if (name == "verb" && end < n)
            {
                // \verb|…|: the delimiter is the next character.
                int close = int(text.indexOf(text[end], end + 1));
                end = close < 0 ? n : close + 1;
                pending = None;
            }
            else pending = None;
            if (headingCommands.contains(name) && !math && name != "title" && name != "caption")
                setFormat(i, end - i, f.heading); // \section and friends: bold, in the heading colour
            i = end;
            continue;
        }
        if (c == '\\' && i + 1 < n)
        {
            // Control symbols: \\ \[ \] \( \) and escapes such as \{ \$ \%.
            const QChar next = text[i + 1];
            if (!math && (next == '[' || next == '(')) { mode = next == '[' ? DisplayMath : InlineMath; math = true; setFormat(i, 2, f.math); }
            else if (math && (next == ']' || next == ')') && mode != MathEnvironment) { setFormat(i, 2, f.math); mode = Text; math = false; }
            else setFormat(i, 2, next == '\\' ? f.special : math ? f.mathCommand : f.command);
            i += 2;
            pending = None;
            continue;
        }
        if (c == '$')
        {
            bool twice = i + 1 < n && text[i + 1] == '$';
            if (!math) { mode = twice ? DisplayMath : InlineMath; math = true; }
            else if (mode != MathEnvironment) { mode = Text; math = false; }
            setFormat(i, twice ? 2 : 1, f.math);
            i += twice ? 2 : 1;
            pending = None;
            continue;
        }
        if (c == '{' || c == '[')
        {
            setFormat(i, 1, f.braces[depth % 3]);
            ++depth;
            if (c == '{' && pending != None && argumentStart < 0)
            {
                argumentStart = i + 1;
                argumentDepth = depth;
                argumentKind = pending;
                pending = None;
            }
            ++i;
            continue;
        }
        if (c == '}' || c == ']')
        {
            if (c == '}' && argumentStart >= 0 && depth == argumentDepth)
            {
                ranges.append({argumentStart, i, argumentKind});
                if (argumentKind == EnvironmentName)
                {
                    // \begin{equation} starts math, \begin{verbatim} raw text; the matching \end ends them.
                    const QString name = text.mid(argumentStart, i - argumentStart);
                    const bool opening = text.lastIndexOf("\\begin", argumentStart) > text.lastIndexOf("\\end", argumentStart);
                    if (opening && !math && mathEnvironments.contains(name)) { mode = MathEnvironment; environment = int(mathEnvironments.indexOf(name)); math = true; }
                    else if (opening && !math && verbatimEnvironments.contains(name)) { mode = Verbatim; environment = int(verbatimEnvironments.indexOf(name)); }
                    else if (!opening && mode == MathEnvironment && mathEnvironments.value(environment) == name) { mode = Text; math = false; }
                }
                argumentStart = -1;
            }
            depth = std::max(0, depth - 1);
            setFormat(i, 1, f.braces[depth % 3]);
            ++i;
            continue;
        }
        if (c == '&' || c == '~')
        {
            setFormat(i, 1, f.special);
            ++i;
            continue;
        }
        // A run of ordinary characters; at least one, so a lone \ at the end of a line moves on.
        int end = i + 1;
        while (end < n && !isSpecial(text[end])) ++end;
        if (math) setFormat(i, end - i, f.math);
        if (pending != None && depth == pendingDepth && !text.mid(i, end - i).trimmed().isEmpty()) pending = None;
        i = end;
    }
    if (argumentStart >= 0) ranges.append({argumentStart, n, argumentKind}); // continues on the next line
    for (const auto &range : ranges)
        for (int j = range.start; j < range.end; ++j)
        {
            const QTextCharFormat current = format(j);
            if (range.kind == PlainText) { if (current == f.math) setFormat(j, 1, f.plain); }
            else if (range.kind == EnvironmentName && (current == QTextCharFormat() || current == f.math))
                setFormat(j, 1, f.environment); // heading and key arguments stay plain text
        }
    setCurrentBlockState(std::min(depth, 0xFF) | mode << 8 | environment << 12);
}

Editor::Editor(QWidget *parent) : QTextEdit(parent)
{
    setAcceptRichText(false);
    setLineWrapMode(QTextEdit::NoWrap);
    // A preview that finished rendering shows if the pointer still rests on its formula or figure.
    m_previewPopup = new QLabel(this, Qt::ToolTip | Qt::FramelessWindowHint);
    m_previewPopup->setObjectName("preview");
    m_previewPopup->setStyleSheet(themed("QLabel#preview { background:white; border:1px solid #303842; border-radius:4px; padding:8px; }"));
    viewport()->setMouseTracking(true);
    connect(this, &Editor::previewReady, this, [this]
    {
        if (m_hoverPoint.isNull() || !isActiveWindow()) return;
        QPixmap pixmap = preview(cursorForPosition(viewport()->mapFromGlobal(m_hoverPoint)).position());
        if (!pixmap.isNull()) showPreview(pixmap, m_hoverPoint);
    });
    setFrameShape(QFrame::NoFrame);
    document()->setDocumentMargin(8);
    QFont mono;
    mono.setFamilies({"JetBrains Mono", "JetBrainsMono Nerd Font Mono", "Input Mono", "monospace"});
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSizeF(10.5);
    setFont(mono);
    setTabStopDistance(QFontMetricsF(mono).horizontalAdvance(' ') * 4);
    new LatexHighlighter(document());
    new SmoothScroll(this);
    m_gutter = new QWidget(this);
    m_gutter->installEventFilter(this);
    auto refreshGutter = [this] { setViewportMargins(gutterWidth(), 0, 0, 0); m_gutter->update(); };
    connect(document(), &QTextDocument::blockCountChanged, this, refreshGutter);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, m_gutter, qOverload<>(&QWidget::update));
    connect(this, &QTextEdit::textChanged, m_gutter, qOverload<>(&QWidget::update));
    connect(this, &QTextEdit::cursorPositionChanged, this, [this]
    {
        // Moving into folded lines (find, go to line, arrows) opens the folds around them.
        for (QTextBlock block = textCursor().block(); !block.isVisible() && block.isValid(); block = textCursor().block())
        {
            QTextBlock owner = block;
            while (owner.isValid() && !owner.isVisible()) owner = owner.previous();
            if (!owner.isValid() || !toggleFold(owner.blockNumber())) break;
        }
        m_gutter->update();
        updateHighlights();
    });
    // Wrapped lines continue under the first non-blank character; applied after the edit settles.
    connect(document(), &QTextDocument::contentsChange, this, [this](int position, int, int added)
    {
        int from = document()->findBlock(position).blockNumber();
        int to = document()->findBlock(position + added).blockNumber();
        if (to < 0) to = document()->blockCount() - 1;
        if (m_indentFrom < 0) QTimer::singleShot(0, this, &Editor::applyWrapIndent);
        m_indentFrom = m_indentFrom < 0 ? from : std::min(m_indentFrom, from);
        m_indentTo = std::max(m_indentTo, to);
    });
    m_selectionBar = new QFrame(viewport());
    m_selectionBar->setObjectName("selectionBar");
    auto *barLayout = new QHBoxLayout(m_selectionBar);
    barLayout->setContentsMargins(3, 3, 3, 3);
    barLayout->setSpacing(1);
    // Same templates as the Insert menu, so selected text is wrapped exactly as Ctrl+B and friends do.
    for (const auto &[label, tip, text] : {std::tuple{"B", "Bold · Ctrl+B", "\\textbf{\u2022}"}, {"I", "Italic · Ctrl+I", "\\textit{\u2022}"},
             {"E", "Emphasis · Ctrl+Shift+E", "\\emph{\u2022}"}, {"tt", "Typewriter · Ctrl+Shift+T", "\\texttt{\u2022}"},
             {"$", "Inline math · Ctrl+Shift+M", "$\u2022$"}, {"%", "Toggle comment · Ctrl+/", ""}})
    {
        if (QString(label) == "$")
        {
            auto *divider = new QFrame;
            divider->setFixedWidth(1);
            divider->setStyleSheet(themed("background:#303842;"));
            barLayout->addWidget(divider);
        }
        auto *button = new QToolButton;
        button->setText(label);
        button->setToolTip(tip);
        button->setFocusPolicy(Qt::NoFocus); // the selection and focus stay in the editor
        connect(button, &QToolButton::clicked, this, [this, text = QString::fromUtf8(text)]
        {
            m_selectionBar->hide();
            if (text.isEmpty()) toggleComment(); else insertTemplate(text);
            setFocus();
        });
        barLayout->addWidget(button);
    }
    m_selectionBar->hide();
    connect(this, &QTextEdit::selectionChanged, this, [this] { if (!textCursor().hasSelection()) m_selectionBar->hide(); });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, m_selectionBar, &QWidget::hide);
    refreshGutter();
    updateHighlights();
    texTreeLoad(); // starts reading TeX Live's file lists in the background
    m_scanTimer.setSingleShot(true);
    m_scanTimer.setInterval(400);
    connect(&m_scanTimer, &QTimer::timeout, this, &Editor::scan);
    connect(document(), &QTextDocument::contentsChanged, this, [this] { if (document()->blockCount() >= 5000) m_scanTimer.start(); });
    m_completion = new QCompleter(new QStringListModel(this), this);
    m_completion->setWidget(this);
    if (auto *list = qobject_cast<QListView *>(m_completion->popup())) list->setUniformItemSizes(true); // no per-row measuring
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
    m_macros.clear(); m_environments.clear(); m_labels.clear(); m_packages.clear();
    static const QRegularExpression definition(
        R"(\\(?:(?:re)?newcommand|providecommand|DeclareMathOperator)\*?\s*\{?\s*(\\[A-Za-z@]+)\s*\}?\s*(?:\[(\d)\])?)"
        R"(|\\def\s*(\\[A-Za-z@]+)|\\newenvironment\*?\s*\{([A-Za-z*]+)\}\s*(?:\[(\d)\])?|\\label\{([^{}]+)\})"
        R"(|\\(?:usepackage|RequirePackage|documentclass)(?:\[[^\]]*\])?\{([^{}]+)\})");
    for (auto it = definition.globalMatch(toPlainText()); it.hasNext();)
    {
        auto match = it.next();
        if (match.hasCaptured(1)) m_macros << match.captured(1) + QString("{•}").repeated(match.captured(2).toInt());
        else if (match.hasCaptured(3)) m_macros << match.captured(3);
        else if (match.hasCaptured(4)) m_environments << match.captured(4) + QString("{•}").repeated(match.captured(5).toInt());
        else if (match.hasCaptured(6)) m_labels << match.captured(6);
        else for (const auto &name : match.captured(7).split(',', Qt::SkipEmptyParts)) m_packages << name.trimmed();
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
    auto roots = [&editors]
    {
        QStringList all;
        for (auto *editor : editors)
            if (!editor->property("path").toString().isEmpty()) all << BuildRunner::rootFile(editor->property("path").toString());
        all.removeDuplicates();
        return all;
    };
    auto collect = [&editors](QStringList Editor::*list)
    {
        QStringList all;
        for (auto *editor : editors)
        {
            // Small documents are rescanned at once; large ones when typing pauses (m_scanTimer).
            if (editor->m_scanRevision < 0 || editor->document()->blockCount() < 5000) editor->scan();
            all += editor->*list;
        }
        return all;
    };
    QStringList words;
    QRegularExpressionMatch match;
    // Words of the packages and classes the open documents load: curated, else read from the .sty.
    auto packageWords = [&](QStringList PackageWords::*list)
    {
        QStringList all;
        for (const auto &name : collect(&Editor::m_packages))
            all += curatedPackages().contains(name) ? curatedPackages().value(name).*list : scannedPackage(name).*list;
        return all;
    };
    if ((match = environment.match(before)).hasMatch())
    {
        m_context = Context::Environment;
        words = builtinEnvironments + collect(&Editor::m_environments) + packageWords(&PackageWords::environments);
    }
    else if ((match = reference.match(before)).hasMatch())
    {
        m_context = Context::Reference;
        words = collect(&Editor::m_labels);
        for (const auto &root : roots()) words += projectRefs(root, editors).labels;
    }
    else if ((match = citation.match(before)).hasMatch())
    {
        m_context = Context::Citation;
        // The bibliographies the project names, plus any .bib next to an open source.
        QStringList files;
        for (const auto &root : roots()) files += projectRefs(root, editors).bibFiles;
        for (auto *editor : editors)
            if (!editor->property("path").toString().isEmpty())
                for (const auto &info : QFileInfo(editor->property("path").toString()).absoluteDir().entryInfoList({"*.bib"}, QDir::Files))
                    files << QDir::cleanPath(info.absoluteFilePath());
        files.removeDuplicates();
        words = bibKeys(files);
    }
    else if ((match = package.match(before)).hasMatch())
    {
        m_context = Context::Name;
        words = texTree().styles;
    }
    else if ((match = documentClass.match(before)).hasMatch())
    {
        m_context = Context::Name;
        words = texTree().classes;
    }
    else if ((match = command.match(before)).hasMatch() && (request || match.capturedLength() >= 3))
    {
        m_context = Context::Command;
        words = builtinCommands + collect(&Editor::m_macros) + packageWords(&PackageWords::commands);
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
    // Width from the first rows only: measuring all ~6,000 package names took ~50 ms.
    int width = 0;
    QFontMetrics metrics(popup->font());
    auto *matches = m_completion->completionModel();
    for (int row = 0; row < std::min(matches->rowCount(), 200); ++row)
        width = std::max(width, metrics.horizontalAdvance(matches->index(row, 0).data().toString()));
    rect.setWidth(width + popup->verticalScrollBar()->sizeHint().width() + 24);
    m_completion->complete(rect);
    popup->setCurrentIndex(m_completion->completionModel()->index(0, 0));
}

void Editor::keyPressEvent(QKeyEvent *event)
{
    m_previewPopup->hide();
    m_selectionBar->hide();
    if (m_completion->popup()->isVisible() &&
        (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return || event->key() == Qt::Key_Escape || event->key() == Qt::Key_Tab))
    {
        event->ignore();
        return;
    }
    if (event->matches(QKeySequence::Undo)) { undoText(); return; }
    if (event->matches(QKeySequence::Redo)) { redoText(); return; }
    bool control = event->modifiers() == Qt::ControlModifier;
    if (event->key() == Qt::Key_Escape) { m_placeholders.clear(); m_placeholder = -1; }
    if (event->key() == Qt::Key_Backtab) { shiftLines(true); return; }
    auto selection = textCursor();
    bool lines = selection.hasSelection() && document()->findBlock(selection.selectionStart()) != document()->findBlock(selection.selectionEnd());
    if (event->key() == Qt::Key_Tab && !event->modifiers() && lines) { shiftLines(false); return; }
    if (((event->key() == Qt::Key_Tab && !event->modifiers()) || (control && event->key() == Qt::Key_Right)) && jumpPlaceholder(1))
        return;
    if (control && event->key() == Qt::Key_Left && jumpPlaceholder(-1)) return;
    if (event->key() == Qt::Key_Tab && !event->modifiers())
    {
        // Up to the next multiple of four columns, as in VS Code.
        int column = selection.selectionStart() - document()->findBlock(selection.selectionStart()).position();
        selection.insertText(QString(4 - column % 4, ' '));
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
    // Shift+Enter continues a list: a new \item (or \task) at the indentation of the item above.
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == Qt::ShiftModifier)
    {
        static const QRegularExpression item(R"(^(\s*)(\\(?:item|task))(?![A-Za-z]))");
        static const QRegularExpression list(R"(^(\s*)\\begin\{(itemize|enumerate|description|tasks|compactitem|compactenum)\*?\})");
        static const QRegularExpression begins(R"(\\begin\s*\{)"), ends(R"(\\end\s*\{)");
        QString insert = "\n" + QRegularExpression(R"(^\s*)").match(cursor.block().text()).captured();
        // Walking up, nested environments (\end ... \begin) are skipped, so the item found is the
        // one at the cursor's own level; reaching the list's \begin first means its first item.
        int depth = 0;
        for (auto block = cursor.block(); block.isValid(); block = block.previous())
        {
            QString text = block.text();
            if (auto match = item.match(text); depth == 0 && match.hasMatch())
            {
                insert = "\n" + match.captured(1) + match.captured(2) + ' ';
                break;
            }
            static const QRegularExpression comment(R"((?<!\\)(?:\\\\)*\K%.*$)"); // as LatexHighlighter
            QString code = QString(text).remove(comment); // "% \end{itemize}" is not an end
            depth += int(code.count(ends)) - int(code.count(begins));
            if (depth < 0)
            {
                if (auto open = list.match(text); open.hasMatch())
                    insert = "\n" + open.captured(1) + "  " + (open.captured(2) == "tasks" ? "\\task " : "\\item ");
                break;
            }
        }
        cursor.insertText(insert);
        setTextCursor(cursor);
        m_completion->popup()->hide();
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
    return fontMetrics().horizontalAdvance(QString(digits, '9')) + 22; // fold column, then the numbers
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
    if (object == m_gutter && event->type() == QEvent::MouseButtonPress)
    {
        // A click beside a foldable line folds or unfolds it.
        int y = int(static_cast<QMouseEvent *>(event)->position().y());
        toggleFold(cursorForPosition(QPoint(0, y)).blockNumber());
        return true;
    }
    return QTextEdit::eventFilter(object, event);
}

// The line without its comment.
static QString codeOf(const QString &line)
{
    for (int i = 0; i < line.size(); ++i)
        if (line[i] == '\\') ++i;
        else if (line[i] == '%') return line.left(i);
    return line;
}

static int headingLevel(const QString &code)
{
    static const QRegularExpression heading(R"(^\s*\\(part|chapter|section|subsection|subsubsection|paragraph)\b\*?\s*[\[{])");
    static const QStringList levels = {"part", "chapter", "section", "subsection", "subsubsection", "paragraph"};
    auto match = heading.match(code);
    return match.hasMatch() ? int(levels.indexOf(match.captured(1))) : -1;
}

// The environment a line opens and does not close on the same line, or empty.
static QString openedEnvironment(const QString &code)
{
    static const QRegularExpression begin(R"(\\begin\{([^{}]+)\})");
    for (auto it = begin.globalMatch(code); it.hasNext();)
    {
        auto match = it.next();
        QString name = match.captured(1);
        if (code.count("\\begin{" + name + "}") > code.count("\\end{" + name + "}")) return name;
    }
    return {};
}

// Cheap test for the gutter, which paints every visible line; foldEnd() finds the real range.
static bool looksFoldable(const QString &line)
{
    if (!line.contains('\\')) return false;
    QString code = codeOf(line);
    return headingLevel(code) >= 0 || !openedEnvironment(code).isEmpty();
}

int Editor::foldEnd(const QTextBlock &start) const
{
    QString code = codeOf(start.text());
    int end = -1;
    if (QString name = openedEnvironment(code); !name.isEmpty())
    {
        // Through the line before the matching \end, which stays visible.
        const QString open = "\\begin{" + name + "}", close = "\\end{" + name + "}";
        int depth = 0;
        for (QTextBlock block = start; block.isValid() && block.blockNumber() - start.blockNumber() < 20000; block = block.next())
        {
            QString line = codeOf(block.text());
            depth += int(line.count(open)) - int(line.count(close));
            if (depth <= 0) { end = block.blockNumber() - 1; break; }
        }
    }
    else if (int level = headingLevel(code); level >= 0)
    {
        // Up to the next heading of the same or a higher level, or the end of the document.
        QTextBlock block = start.next();
        for (; block.isValid(); block = block.next())
        {
            QString line = codeOf(block.text());
            int other = headingLevel(line);
            if ((other >= 0 && other <= level) || line.contains("\\end{document}")) break;
        }
        QTextBlock last = block.isValid() ? block.previous() : document()->lastBlock();
        while (last.blockNumber() > start.blockNumber() && last.text().trimmed().isEmpty()) last = last.previous();
        end = last.blockNumber();
    }
    return end > start.blockNumber() ? end : -1;
}

bool Editor::isFolded(int blockNumber) const
{
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    return block.isValid() && block.isVisible() && block.next().isValid() && !block.next().isVisible();
}

void Editor::refreshFold(const QTextBlock &from, const QTextBlock &to)
{
    // Relayout only; visibility is not part of the text or the undo history.
    document()->markContentsDirty(from.position(), to.position() + to.length() - from.position());
    setViewportMargins(gutterWidth(), 0, 0, 0);
    m_gutter->update();
    viewport()->update();
}

bool Editor::toggleFold(int blockNumber)
{
    QTextBlock start = document()->findBlockByNumber(blockNumber);
    if (!start.isValid() || !start.isVisible()) return false;
    if (isFolded(blockNumber))
    {
        // Everything hidden after the line comes back, folds nested inside included.
        QTextBlock block = start.next(), last = block;
        for (; block.isValid() && !block.isVisible(); block = block.next()) { block.setVisible(true); last = block; }
        refreshFold(start, last);
        return true;
    }
    int end = foldEnd(start);
    if (end < 0) return false;
    QTextBlock block = start.next(), last = block;
    for (; block.isValid() && block.blockNumber() <= end; block = block.next()) { block.setVisible(false); last = block; }
    if (textCursor().blockNumber() > blockNumber && textCursor().blockNumber() <= end)
    {
        QTextCursor cursor(start);
        cursor.movePosition(QTextCursor::EndOfBlock);
        setTextCursor(cursor);
    }
    refreshFold(start, last);
    return true;
}

void Editor::foldAtCursor()
{
    // The innermost fold that contains the cursor line.
    const int line = textCursor().blockNumber();
    for (QTextBlock block = textCursor().block(); block.isValid() && line - block.blockNumber() < 5000; block = block.previous())
        if (block.isVisible() && !isFolded(block.blockNumber()) && looksFoldable(block.text()))
            if (int end = foldEnd(block); end >= line || block.blockNumber() == line)
                if (end >= 0 && toggleFold(block.blockNumber())) return;
}

void Editor::unfoldAll()
{
    bool any = false;
    for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next())
        if (!block.isVisible()) { block.setVisible(true); any = true; }
    if (any) refreshFold(document()->firstBlock(), document()->lastBlock());
}

void Editor::paintEvent(QPaintEvent *event)
{
    QTextEdit::paintEvent(event);
    // A ⋯ badge after each folded line, as in VS Code.
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    QPointF scroll(horizontalScrollBar()->value(), verticalScrollBar()->value());
    auto *layout = document()->documentLayout();
    for (auto block = cursorForPosition(QPoint(0, 0)).block(); block.isValid(); block = block.next())
    {
        QRectF rect = layout->blockBoundingRect(block).translated(-scroll);
        if (rect.top() > viewport()->height()) break;
        if (!isFolded(block.blockNumber()) || !block.layout() || block.layout()->lineCount() == 0) continue;
        QTextLine line = block.layout()->lineAt(block.layout()->lineCount() - 1);
        QRectF badge(rect.left() + line.naturalTextRect().right() + 8, rect.top() + line.y() + 2,
                     fontMetrics().horizontalAdvance("⋯") + 10, line.height() - 4);
        painter.setPen(Qt::NoPen);
        painter.setBrush(themeColor("#29333B"));
        painter.drawRoundedRect(badge, 3, 3);
        painter.setPen(themeColor("#ADB7C4"));
        painter.drawText(badge, Qt::AlignCenter, "⋯");
    }
}

void Editor::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier))
    {
        // Whole notches only, so a touchpad does not zoom on every pixel.
        static int travel = 0;
        travel += event->angleDelta().y();
        if (std::abs(travel) >= 120) { emit fontZoomRequested(travel / 120); travel %= 120; }
        event->accept();
        return;
    }
    QTextEdit::wheelEvent(event);
}

void Editor::setEditorFont(const QFont &font)
{
    setFont(font);
    setTabStopDistance(QFontMetricsF(font).horizontalAdvance(' ') * 4);
    setViewportMargins(gutterWidth(), 0, 0, 0);
    m_indentFrom = 0;
    m_indentTo = document()->blockCount() - 1;
    applyWrapIndent();
    m_gutter->update();
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
        if (!block.isVisible()) continue;
        QRectF rect = layout->blockBoundingRect(block).translated(0, -scroll);
        if (rect.top() > m_gutter->height()) break;
        const bool here = block.blockNumber() == current;
        if (here) painter.fillRect(QRectF(0, rect.top(), m_gutter->width(), rect.height()), themeColor("#1D2024")); // the current-line band runs through the gutter
        QFont number = font();
        number.setBold(here);
        painter.setFont(number);
        painter.setPen(here ? themeColor("#B9C3CF") : themeColor("#8793A2"));
        painter.drawText(QRectF(0, rect.top(), m_gutter->width() - 6, fontMetrics().lineSpacing()), Qt::AlignRight | Qt::AlignVCenter,
            QString::number(block.blockNumber() + 1));
        // Fold column, left of the number: › on folded lines, ⌄ (dim) on lines that start a fold.
        bool folded = isFolded(block.blockNumber());
        if (!folded && !looksFoldable(block.text())) continue;
        double size = 3.5, x = 10, y = rect.top() + fontMetrics().lineSpacing() / 2.0;
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(folded ? themeColor("#D2D7DE") : themeColor("#7C8794"), 1.3));
        if (folded) painter.drawPolyline(QPolygonF({QPointF(x - size / 2, y - size), QPointF(x + size / 2, y), QPointF(x - size / 2, y + size)}));
        else painter.drawPolyline(QPolygonF({QPointF(x - size, y - size / 2), QPointF(x, y + size / 2), QPointF(x + size, y - size / 2)}));
        painter.restore();
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
    line.format.setBackground(themeColor("#1D2024"));
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
    for (const auto &mark : m_marks)
    {
        QTextBlock block = mark.cursor.block();
        QTextEdit::ExtraSelection squiggle;
        squiggle.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        squiggle.format.setUnderlineColor(mark.error ? themeColor("#F14C4C") : themeColor("#CCA700"));
        squiggle.cursor = QTextCursor(block);
        // From the first non-blank character to the end of the line, as VS Code underlines.
        int indent = 0;
        while (indent < block.text().size() && block.text()[indent].isSpace()) ++indent;
        squiggle.cursor.setPosition(block.position() + indent);
        squiggle.cursor.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
        selections << squiggle;
    }
    setExtraSelections(selections);
}

void Editor::setProblems(const QList<std::tuple<int, bool, QString>> &problems)
{
    m_marks.clear();
    for (const auto &[line, error, message] : problems)
    {
        QTextBlock block = document()->findBlockByNumber(line - 1);
        if (block.isValid()) m_marks << Mark{QTextCursor(block), error, message};
    }
    updateHighlights();
}

bool Editor::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        auto *help = static_cast<QHelpEvent *>(event);
        int block = cursorForPosition(help->pos()).blockNumber();
        QStringList messages;
        for (const auto &mark : m_marks)
            if (mark.cursor.blockNumber() == block) messages << (mark.error ? "Error: " : "Warning: ") + mark.message;
        m_hoverPoint = help->globalPos();
        QPixmap pixmap = messages.isEmpty() ? preview(cursorForPosition(help->pos()).position()) : QPixmap();
        if (!pixmap.isNull()) { QToolTip::hideText(); showPreview(pixmap, help->globalPos()); return true; }
        m_previewPopup->hide();
        if (messages.isEmpty()) QToolTip::hideText();
        else QToolTip::showText(help->globalPos(), messages.join('\n'), viewport());
        return true;
    }
    if (event->type() == QEvent::MouseMove)
    {
        m_hoverPoint = static_cast<QMouseEvent *>(event)->globalPosition().toPoint();
        if (m_previewPopup->isVisible() && (m_hoverPoint - m_previewPoint).manhattanLength() > 12) m_previewPopup->hide();
    }
    else if (event->type() == QEvent::Leave || event->type() == QEvent::MouseButtonPress || event->type() == QEvent::Wheel)
    {
        m_hoverPoint = {};
        m_previewPopup->hide();
    }
    return QTextEdit::viewportEvent(event);
}

// The popup is a window of its own: it does not hide when its tab is switched away.
void Editor::mouseReleaseEvent(QMouseEvent *event)
{
    QTextEdit::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton) showSelectionBar();
}

void Editor::showSelectionBar()
{
    auto cursor = textCursor();
    if (!cursor.hasSelection()) { m_selectionBar->hide(); return; }
    QTextCursor start(document());
    start.setPosition(cursor.selectionStart());
    const QRect line = cursorRect(start);
    m_selectionBar->adjustSize();
    int x = std::clamp(line.left() - 8, 4, std::max(4, viewport()->width() - m_selectionBar->width() - 4));
    int y = line.top() - m_selectionBar->height() - 6;
    if (y < 2) y = line.bottom() + 6; // no room above the first visible line
    m_selectionBar->move(x, y);
    m_selectionBar->show();
    m_selectionBar->raise();
}

void Editor::hideEvent(QHideEvent *event)
{
    m_previewPopup->hide();
    m_selectionBar->hide();
    QTextEdit::hideEvent(event);
}

void Editor::focusOutEvent(QFocusEvent *event)
{
    m_previewPopup->hide();
    QTextEdit::focusOutEvent(event);
}

void Editor::showPreview(const QPixmap &pixmap, const QPoint &global)
{
    m_previewPopup->setPixmap(pixmap);
    m_previewPopup->adjustSize();
    // Below and right of the pointer, kept on the screen.
    QRect area = screen()->availableGeometry();
    QPoint at = global + QPoint(12, 18);
    if (at.x() + m_previewPopup->width() > area.right()) at.setX(area.right() - m_previewPopup->width());
    if (at.y() + m_previewPopup->height() > area.bottom()) at.setY(global.y() - 8 - m_previewPopup->height());
    m_previewPopup->move(at);
    m_previewPopup->show();
    m_previewPoint = global;
}

QString Editor::mathAt(int position) const
{
    QTextBlock block = document()->findBlock(position);
    if (!block.isValid()) return {};
    // Inline: $…$, $$…$$ and \(…\) on this line, before any comment.
    const QString line = block.text();
    const int at = position - block.position();
    int open = -1;
    QString opener;
    for (int i = 0; i < line.size(); ++i)
    {
        if (line[i] == '%') break;
        if (line[i] == '\\' && i + 1 < line.size())
        {
            if (line[i + 1] == '(' && open < 0) { open = i; opener = "\\("; }
            else if (line[i + 1] == ')' && opener == "\\(")
            {
                if (at >= open && at <= i + 1) return line.mid(open, i + 2 - open);
                open = -1; opener.clear();
            }
            ++i; // \$, \%, \\ and other escapes
            continue;
        }
        if (line[i] != '$') continue;
        QString token = line.mid(i, 2) == "$$" ? "$$" : "$";
        if (open < 0) { open = i; opener = token; }
        else if (opener == token)
        {
            int end = i + token.size();
            if (at >= open && at < end)
                return token == "$" ? line.mid(open, end - open) : "\\[" + line.mid(open + 2, i - open - 2) + "\\]";
            open = -1; opener.clear();
        }
        i += token.size() - 1;
    }
    // Display: \[…\] and math environments, which may span lines; the nearest one enclosing position.
    QTextBlock first = block, last = block;
    for (int i = 0; i < 40 && first.previous().isValid(); ++i) first = first.previous();
    for (int i = 0; i < 40 && last.next().isValid(); ++i) last = last.next();
    QTextCursor range(document());
    range.setPosition(first.position());
    range.setPosition(last.position() + last.length() - 1, QTextCursor::KeepAnchor);
    const QString text = range.selectedText().replace(QChar::ParagraphSeparator, '\n');
    const int offset = position - first.position();
    static const QRegularExpression display(
        R"(\\begin\{(equation|align|alignat|gather|multline|flalign|eqnarray|displaymath|math)(\*?)\}|(?<!\\)\\\[)");
    QString found;
    for (auto it = display.globalMatch(text); it.hasNext();)
    {
        auto match = it.next();
        if (match.capturedStart() > offset) break;
        QString closer = match.captured(1).isEmpty() ? "\\]" : "\\end{" + match.captured(1) + match.captured(2) + "}";
        int close = int(text.indexOf(closer, match.capturedEnd()));
        if (close >= 0 && close + closer.size() > offset) found = text.mid(match.capturedStart(), close + closer.size() - match.capturedStart());
    }
    return found;
}

QString Editor::imageAt(int position) const
{
    QTextBlock block = document()->findBlock(position);
    static const QRegularExpression graphic(R"(\\includegraphics\*?\s*(?:\[[^\]]*\])?\s*\{([^{}]+)\})");
    const int at = position - block.position();
    QString name;
    for (auto it = graphic.globalMatch(block.text()); it.hasNext();)
    {
        auto match = it.next();
        if (at >= match.capturedStart() && at <= match.capturedEnd()) name = match.captured(1).trimmed();
    }
    QString path = property("path").toString();
    if (name.isEmpty() || path.isEmpty()) return {};
    // Relative to the root, as the build sees it, then each \graphicspath folder.
    QString root = BuildRunner::rootFile(path);
    QDir base = QFileInfo(root).absoluteDir();
    QStringList folders{""};
    static const QRegularExpression graphicsPath(R"(\\graphicspath\s*\{((?:\s*\{[^{}]*\})+)\s*\})");
    static const QRegularExpression folder(R"(\{([^{}]*)\})");
    for (const QString &text : {toPlainText(), sourceText(root, window()->findChildren<Editor *>())})
        if (auto match = graphicsPath.match(text); match.hasMatch())
            for (auto it = folder.globalMatch(match.captured(1)); it.hasNext();) folders << it.next().captured(1);
    for (const auto &directory : folders)
        for (const char *suffix : {"", ".pdf", ".png", ".jpg", ".jpeg"})
        {
            QString file = base.absoluteFilePath(directory + name + suffix);
            if (QFileInfo(file).isFile()) return QDir::cleanPath(file);
        }
    return {};
}

// At most 360 logical pixels, drawn 1:1 on screen pixels: dpr is the screen's, scale the image's pixels per logical pixel.
static QPixmap previewPixmap(const QString &path, double scale, double dpr)
{
    QImageReader reader(path);
    QSize size = reader.size();
    if (!size.isValid()) return {};
    QSizeF shown = QSizeF(size) / scale;
    if (shown.width() > 360 || shown.height() > 360) shown.scale(360, 360, Qt::KeepAspectRatio);
    QSize pixels = (shown * dpr).toSize();
    if (pixels != size) reader.setScaledSize(pixels);
    QPixmap pixmap = QPixmap::fromImage(reader.read());
    pixmap.setDevicePixelRatio(dpr);
    return pixmap;
}

QPixmap Editor::preview(int position)
{
    QString formula = mathAt(position);
    QString image = formula.isEmpty() ? imageAt(position) : QString();
    if (formula.isEmpty() && image.isEmpty()) return {};
    QString source;
    if (!formula.isEmpty())
    {
        // The document's own math packages and macro definitions, so \R or \norm render too.
        QString path = property("path").toString();
        QString text = path.isEmpty() ? toPlainText() : sourceText(BuildRunner::rootFile(path), window()->findChildren<Editor *>());
        if (text.isEmpty()) text = toPlainText();
        QString preamble = text.left(std::max(0, int(text.indexOf("\\begin{document}"))));
        static const QStringList mathPackages = {"amsmath", "amssymb", "amsfonts", "mathtools", "bm", "physics", "siunitx",
            "esint", "mathrsfs", "cancel", "braket", "nicefrac", "xfrac", "dsfont", "bbm", "stmaryrd", "upgreek", "amsthm"};
        static const QRegularExpression package(R"(^\s*\\usepackage(?:\[[^\]]*\])?\{([^{}]+)\})", QRegularExpression::MultilineOption);
        static const QRegularExpression definition(R"(^\s*\\(?:(?:re)?newcommand|providecommand|DeclareMathOperator|def)\b.*$)",
            QRegularExpression::MultilineOption);
        QStringList packages{"amsmath", "amssymb"};
        for (auto it = package.globalMatch(preamble); it.hasNext();)
            for (const auto &name : it.next().captured(1).split(',', Qt::SkipEmptyParts))
                if (mathPackages.contains(name.trimmed())) packages << name.trimmed();
        packages.removeDuplicates();
        source = "\\documentclass[border=3pt,varwidth]{standalone}\n";
        // Vietnamese and other accented text inside \text{…} needs T5 under pdfLaTeX.
        if (std::any_of(formula.cbegin(), formula.cend(), [](QChar c) { return c.unicode() > 127; })) source += "\\usepackage[T5]{fontenc}\n";
        source += "\\usepackage{" + packages.join(',') + "}\n";
        for (auto it = definition.globalMatch(preamble); it.hasNext();) source += it.next().captured(0) + '\n';
        source += "\\begin{document}\n" + formula + "\n\\end{document}\n";
    }
    QString key = formula.isEmpty() ? image + '\n' + QFileInfo(image).lastModified().toString(Qt::ISODateWithMs) : source;
    if (auto *cached = m_previews.object(key)) return *cached;
    if (!formula.isEmpty() || image.endsWith(".pdf", Qt::CaseInsensitive))
    {
        if (!m_rendering.contains(key)) renderPreview(key, source, image);
        return {};
    }
    return remember(key, previewPixmap(image, 1, devicePixelRatioF())); // bitmaps need no rendering
}

QPixmap Editor::remember(const QString &key, const QPixmap &pixmap)
{
    // Bounded like the PDF tile cache: least recently shown previews go first.
    m_previews.insert(key, new QPixmap(pixmap), std::max<qsizetype>(1, pixmap.width() * pixmap.height() * 4 / 1024));
    return pixmap;
}

// pdfLaTeX for a formula, then pdftocairo for its (or a PDF figure's) first page, all in the background.
void Editor::renderPreview(const QString &key, const QString &source, const QString &image)
{
    static QTemporaryDir directory; // removed when the app exits
    if (!directory.isValid()) return;
    m_rendering.insert(key);
    const QString name = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5).toHex();
    const double dpr = devicePixelRatioF();
    std::function<void(bool)> finish = [this, key, png = directory.filePath(name + ".png"), dpr](bool ok)
    {
        m_rendering.remove(key);
        remember(key, ok ? previewPixmap(png, dpr, dpr) : QPixmap());
        emit previewReady();
    };
    auto run = [this](const QString &program, const QStringList &arguments, std::function<void(bool)> done)
    {
        auto *process = new QProcess(this);
        process->setWorkingDirectory(directory.path());
        connect(process, &QProcess::finished, this, [process, done](int code, QProcess::ExitStatus status)
        {
            process->deleteLater();
            done(code == 0 && status == QProcess::NormalExit);
        });
        connect(process, &QProcess::errorOccurred, this, [process, done](QProcess::ProcessError error)
        {
            if (error == QProcess::FailedToStart) { process->deleteLater(); done(false); }
        });
        QTimer::singleShot(15000, process, &QProcess::kill);
        process->start(program, arguments);
    };
    const QStringList firstPage = {"-png", "-singlefile", "-f", "1", "-l", "1"};
    if (source.isEmpty())
    {
        run("pdftocairo", firstPage + QStringList{"-scale-to", QString::number(qRound(360 * dpr)), image, name}, finish);
        return;
    }
    QFile tex(directory.filePath(name + ".tex"));
    if (!tex.open(QIODevice::WriteOnly) || tex.write(source.toUtf8()) < 0) { finish(false); return; }
    tex.close();
    run("pdflatex", {"-interaction=nonstopmode", "-halt-on-error", name + ".tex"}, [run, finish, firstPage, name, dpr](bool ok)
    {
        // 140 dpi per logical pixel: formulas a little larger than the 11 pt editor text, like Overleaf's.
        if (ok) run("pdftocairo", firstPage + QStringList{"-r", QString::number(140 * dpr), name + ".pdf", name}, finish);
        else finish(false);
    });
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

// Hanging indent: margin = leading whitespace, first line pulled back by the same amount, so
// unwrapped lines look unchanged. Block formats never reach the saved text (toPlainText).
void Editor::applyWrapIndent()
{
    int from = m_indentFrom, to = std::min(m_indentTo, document()->blockCount() - 1);
    m_indentFrom = m_indentTo = -1;
    QFontMetricsF metrics(font());
    // shortcut: indents wider than 40 columns are capped so deep nesting keeps a usable width.
    double cap = metrics.horizontalAdvance(' ') * 40;
    bool modified = document()->isModified();
    bool fresh = !document()->isUndoAvailable() && !document()->isRedoAvailable();
    QTextCursor cursor(document());
    bool editing = false;
    for (auto block = document()->findBlockByNumber(std::max(0, from)); block.isValid() && block.blockNumber() <= to; block = block.next())
    {
        double width = 0;
        for (QChar ch : block.text())
        {
            if (ch == '\t') width = (std::floor(width / tabStopDistance()) + 1) * tabStopDistance();
            else if (ch == ' ') width += metrics.horizontalAdvance(' ');
            else break;
        }
        width = std::min(width, cap);
        // 111% of the font's own spacing, about 1.55 em as in the design; set here with the indent so each block is touched once.
        if (qAbs(block.blockFormat().leftMargin() - width) < 0.01 && block.blockFormat().lineHeight() == 111) continue;
        // Joined to the edit that caused it, so undo restores text and indent together.
        if (!editing) { fresh ? cursor.beginEditBlock() : cursor.joinPreviousEditBlock(); editing = true; }
        cursor.setPosition(block.position());
        QTextBlockFormat format = block.blockFormat();
        format.setLeftMargin(width);
        format.setTextIndent(-width);
        format.setLineHeight(111, QTextBlockFormat::ProportionalHeight);
        cursor.setBlockFormat(format);
    }
    if (!editing) return;
    cursor.endEditBlock();
    if (fresh) document()->clearUndoRedoStacks(); // a freshly loaded file starts with an empty undo history
    document()->setModified(modified);
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
    for (auto *action : menu->actions())
    {
        if (action->objectName() == "edit-undo") { action->disconnect(); connect(action, &QAction::triggered, this, &Editor::undoText); }
        if (action->objectName() == "edit-redo") { action->disconnect(); connect(action, &QAction::triggered, this, &Editor::redoText); }
    }
    menu->addSeparator();
    auto *wrap = menu->addAction("Word Wrap\tAlt+Z");
    wrap->setCheckable(true);
    wrap->setChecked(wordWrap());
    connect(wrap, &QAction::toggled, this, &Editor::wordWrapToggled);
    menu->exec(event->globalPos());
}

namespace
{
struct Token { QString text; int position; };

// Letter/digit runs (marks included for decomposed accents); command names after '\\' are skipped.
QList<Token> tokens(const QString &text, int offset = 0)
{
    QList<Token> result;
    for (int i = 0; i < text.size();)
    {
        auto inWord = [&](int k) { return text[k].isLetterOrNumber() || text[k].isMark(); };
        if (!inWord(i)) { ++i; continue; }
        int start = i;
        while (i < text.size() && inWord(i)) ++i;
        if (start == 0 || text[start - 1] != '\\') result.append({text.mid(start, i - start), offset + start});
    }
    return result;
}

// "hoá”." -> "hoá", "ﬁle" -> "file": the longest letter run of the PDF word.
QString key(const QString &word)
{
    QString best;
    for (const auto &token : tokens(word.normalized(QString::NormalizationForm_KC)))
        if (token.text.size() > best.size()) best = token.text;
    return best;
}
}

void Editor::goToWord(int line, const QStringList &words, int clicked)
{
    goToLine(line);
    if (clicked < 0 || clicked >= words.size()) return;
    QString target = key(words[clicked]);
    if (target.isEmpty()) return;
    // The enclosing frame, or just the line outside frames.
    const QTextBlock at = textCursor().block();
    QTextBlock first = at, last = at;
    for (QTextBlock block = at; block.isValid() && at.blockNumber() - block.blockNumber() < 400; block = block.previous())
    {
        if (block.text().contains("\\begin{frame}")) { first = block; break; }
        if (block != at && block.text().contains("\\end{frame}")) break;
    }
    if (first != at || at.text().contains("\\begin{frame}"))
        for (QTextBlock block = at; block.isValid() && block.blockNumber() - at.blockNumber() < 400; block = block.next())
            if (block.text().contains("\\end{frame}")) { last = block; break; }
    QTextCursor range(first);
    range.setPosition(last.position() + last.length() - 1, QTextCursor::KeepAnchor);
    QList<Token> source = tokens(range.selectedText().replace(QChar::ParagraphSeparator, '\n'), first.position());
    int best = -1, bestScore = -1;
    for (int i = 0; i < source.size(); ++i)
    {
        if (source[i].text != target) continue;
        // A neighbour counts if it sits at the same offset in the source, give or take one token.
        int score = 0;
        for (int k = 0; k < words.size(); ++k)
        {
            QString word = key(words[k]);
            int at = i + k - clicked;
            for (int j = std::max(0, at - 1); k != clicked && !word.isEmpty() && j <= std::min(int(source.size()) - 1, at + 1); ++j)
                if (source[j].text == word) { ++score; break; }
        }
        if (score > bestScore) { bestScore = score; best = i; }
    }
    if (best < 0) return;
    QTextCursor cursor(document());
    cursor.setPosition(source[best].position);
    setTextCursor(cursor);
    ensureCursorVisible();
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

// applyWrapIndent's block formats become separate undo steps when the edit before them was
// not an edit block (typing, a single insert). Undoing such a step alone changes no text and
// made applyWrapIndent re-add it, which cleared redo and trapped Ctrl+Z.
void Editor::undoText()
{
    const QString before = toPlainText();
    while (document()->isUndoAvailable() && toPlainText() == before) undo();
}

void Editor::redoText()
{
    const QString before = toPlainText();
    while (document()->isRedoAvailable() && toPlainText() == before) redo();
    // Then the format steps recorded right after that text change; stop before the next one.
    while (document()->isRedoAvailable())
    {
        const QString text = toPlainText();
        redo();
        if (toPlainText() != text) { undo(); break; }
    }
}

void Editor::shiftLines(bool outdent)
{
    auto cursor = textCursor();
    int start = cursor.selectionStart(), end = cursor.selectionEnd();
    auto first = document()->findBlock(start), last = document()->findBlock(end);
    // A selection ending at the start of a line does not include that line.
    if (end > start && last.position() == end && last != first) last = last.previous();
    bool multiLine = first != last;
    cursor.beginEditBlock();
    for (auto block = first; block.isValid(); block = block.next())
    {
        QTextCursor line(block);
        QString text = block.text();
        if (!outdent && (!text.isEmpty() || !multiLine)) line.insertText("    ");
        if (outdent)
        {
            int remove = text.startsWith('\t') ? 1 : 0;
            while (remove < 4 && remove < text.size() && text[remove] == ' ' && !text.startsWith('\t')) ++remove;
            line.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, remove);
            line.removeSelectedText();
        }
        if (block == last) break;
    }
    cursor.endEditBlock();
    if (!multiLine) return;
    // Keep whole lines selected so Tab / Shift+Tab can be repeated.
    cursor.setPosition(first.position());
    cursor.setPosition(last.position() + last.length() - 1, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
}

// Like VS Code: searching wraps around the document.
bool Editor::findText(const QString &text, QTextDocument::FindFlags flags)
{
    if (text.isEmpty()) return false;
    if (find(text, flags)) return true;
    auto cursor = textCursor();
    cursor.movePosition(flags & QTextDocument::FindBackward ? QTextCursor::End : QTextCursor::Start);
    setTextCursor(cursor);
    return find(text, flags);
}

// Replaces the selected match, if any, then selects the next one.
bool Editor::replaceNext(const QString &text, const QString &with, QTextDocument::FindFlags flags)
{
    if (text.isEmpty()) return false;
    flags &= ~QTextDocument::FindBackward;
    auto cursor = textCursor();
    if (cursor.hasSelection())
    {
        // The selection counts as a match only if a search from its start finds exactly it.
        auto match = document()->find(text, cursor.selectionStart(), flags);
        if (match.selectionStart() == cursor.selectionStart() && match.selectionEnd() == cursor.selectionEnd())
            cursor.insertText(with);
    }
    setTextCursor(cursor);
    return findText(text, flags);
}

// One undo step for all replacements.
int Editor::replaceAll(const QString &text, const QString &with, QTextDocument::FindFlags flags)
{
    if (text.isEmpty()) return 0;
    flags &= ~QTextDocument::FindBackward;
    int count = 0;
    QTextCursor edit(document());
    edit.beginEditBlock();
    // After insertText the cursor sits behind the replacement, so it is never searched again.
    for (auto match = document()->find(text, 0, flags); !match.isNull(); match = document()->find(text, match, flags))
    {
        match.insertText(with);
        ++count;
    }
    edit.endEditBlock();
    return count;
}

// Total matches; *current is the 0-based index of the selected match, or -1.
int Editor::countMatches(const QString &text, QTextDocument::FindFlags flags, int *current) const
{
    if (current) *current = -1;
    if (text.isEmpty()) return 0;
    flags &= ~QTextDocument::FindBackward;
    auto selection = textCursor();
    int count = 0;
    for (auto match = document()->find(text, 0, flags); !match.isNull(); match = document()->find(text, match, flags), ++count)
        if (current && match.selectionStart() == selection.selectionStart() && match.selectionEnd() == selection.selectionEnd())
            *current = count;
    return count;
}

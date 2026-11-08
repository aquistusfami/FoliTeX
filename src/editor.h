#pragma once
#include <QTextEdit>
#include <QPropertyAnimation>
#include <QSyntaxHighlighter>
#include <QCompleter>
#include <tuple>
#include <QTimer>
#include <QTextBlock>
#include <QCache>

class QLabel;

// Colours are written for the dark theme; with the light theme they map to light equivalents.
// Chosen once at start-up (switching needs a restart).
void setLightTheme(bool on);
bool lightTheme();
QColor themeColor(const QString &dark);
QString themed(QString text); // every mapped #RRGGBB in a style sheet or SVG

class SmoothScroll : public QObject
{
    Q_OBJECT
public:
    explicit SmoothScroll(QAbstractScrollArea *area, int wheelStep = 72, double touchpadFactor = 1);
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    QAbstractScrollArea *m_area;
    QPropertyAnimation m_animation;
    int m_wheelStep;
    double m_touchpadFactor;
    QList<QPair<qint64, int>> m_samples; // touchpad (timestamp ms, vertical pixels) of the last 100 ms
    bool m_systemMomentum = false;
};

class LatexHighlighter : public QSyntaxHighlighter
{
public:
    explicit LatexHighlighter(QTextDocument *document);
protected:
    void highlightBlock(const QString &text) override;
};

struct OutlineEntry
{
    enum Kind { Section, Label, Include, Todo };
    Kind kind;
    int level; // 0 = \\part … 6 = \\subparagraph; non-sections use 7
    int line;  // 1-based
    QString title;
};

class Editor : public QTextEdit
{
    Q_OBJECT
public:
    explicit Editor(QWidget *parent = nullptr);
    void goToLine(int line, int column = 0);
    // Inverse search: SyncTeX puts everything in a beamer frame on \end{frame}, so find the clicked
    // PDF word (scored by its neighbours) inside the frame around the line; falls back to the line.
    void goToWord(int line, const QStringList &words, int clicked);
    int lineNumber() const;
    void toggleComment();
    // Undo/redo one text change; the wrap-indent format steps around it go along (see applyWrapIndent).
    void undoText();
    void redoText();
    // Indents (or outdents) every line touched by the selection by one level.
    void shiftLines(bool outdent);
    // flags: QTextDocument::FindBackward, FindCaseSensitively, FindWholeWords.
    bool findText(const QString &text, QTextDocument::FindFlags flags = {});
    bool replaceNext(const QString &text, const QString &with, QTextDocument::FindFlags flags = {});
    int replaceAll(const QString &text, const QString &with, QTextDocument::FindFlags flags = {});
    int countMatches(const QString &text, QTextDocument::FindFlags flags, int *current = nullptr) const;
    // Inserts text whose U+2022 marks are placeholders; a selection fills the first one.
    void insertTemplate(QString text);
    void insertEnvironment();
    void closeEnvironment();
    static QString closingFor(const QString &textBefore);
    static QList<OutlineEntry> outline(const QString &text);
    void setWordWrap(bool on);
    // Build problems shown as squiggles with a tooltip: (1-based line, is error, message).
    void setProblems(const QList<std::tuple<int, bool, QString>> &problems);
    bool wordWrap() const { return lineWrapMode() != QTextEdit::NoWrap; }
    // The formula around position ($…$, \(…\), \[…\], equation-like environments), or empty.
    QString mathAt(int position) const;
    // The \includegraphics file under position, resolved like the build would, or empty.
    QString imageAt(int position) const;
    // Hover preview of the formula or image at position, at the screen's pixel density; null while it
    // renders in the background (previewReady follows), when it failed, or when there is nothing to show.
    QPixmap preview(int position);
    // Font, tab width, gutter and wrap indents together.
    void setEditorFont(const QFont &font);
    // Folding: \begin…\end environments and sectioning commands. Hidden lines stay in the text;
    // the cursor entering one unfolds it.
    int foldEnd(const QTextBlock &start) const; // last line a fold at start would hide, -1 if none
    bool isFolded(int blockNumber) const;
    bool toggleFold(int blockNumber);
    void foldAtCursor();
    void unfoldAll();
signals:
    void wordWrapToggled(bool on);
    void previewReady();
    void fontZoomRequested(int steps); // Ctrl+wheel; the window resizes every editor
protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool viewportEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    enum class Context { Command, Environment, Reference, Citation, Name };
    QCompleter *m_completion;
    Context m_context = Context::Command;
    QList<QTextCursor> m_placeholders;
    int m_placeholder = -1;
    int m_scanRevision = -1;
    QStringList m_macros;
    QStringList m_environments;
    QStringList m_labels;
    QStringList m_packages; // \usepackage and \documentclass names
    QTimer m_scanTimer;
    void complete(const QString &text);
    void updateCompletion(bool request);
    bool jumpPlaceholder(int step);
    void scan();
    QWidget *m_gutter;
    int gutterWidth() const;
    void paintGutter();
    void updateHighlights();
    struct Mark
    {
        QTextCursor cursor; // at the start of the line, so it moves with edits above it
        bool error;
        QString message;
    };
    QList<Mark> m_marks;
    QCache<QString, QPixmap> m_previews{16 * 1024}; // key -> image (cost in KiB), null when rendering failed
    QPixmap remember(const QString &key, const QPixmap &pixmap);
    QSet<QString> m_rendering;
    QLabel *m_previewPopup; // a tooltip-like window that shows the pixmap 1:1, unlike QToolTip's scaled <img>
    QPoint m_previewPoint; // global position the popup was shown for
    QPoint m_hoverPoint; // global pointer position while it rests over the text, null otherwise
    void showPreview(const QPixmap &pixmap, const QPoint &global);
    void refreshFold(const QTextBlock &from, const QTextBlock &to);
    void renderPreview(const QString &key, const QString &formula, const QString &image);
    void applyWrapIndent();
    int m_indentFrom = -1; // block range whose wrap indent is pending, -1 when none
    int m_indentTo = -1;
};

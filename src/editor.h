#pragma once
#include <QTextEdit>
#include <QPropertyAnimation>
#include <QSyntaxHighlighter>
#include <QCompleter>
#include <tuple>
#include <QTimer>

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
    int lineNumber() const;
    void toggleComment();
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
signals:
    void wordWrapToggled(bool on);
protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool viewportEvent(QEvent *event) override;
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
    void applyWrapIndent();
    int m_indentFrom = -1; // block range whose wrap indent is pending, -1 when none
    int m_indentTo = -1;
};

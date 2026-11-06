#pragma once
#include <QTextEdit>
#include <QPropertyAnimation>
#include <QSyntaxHighlighter>
#include <QCompleter>

class SmoothScroll : public QObject
{
    Q_OBJECT
public:
    explicit SmoothScroll(QAbstractScrollArea *area);
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    QAbstractScrollArea *m_area;
    QPropertyAnimation m_animation;
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
    // Inserts text whose U+2022 marks are placeholders; a selection fills the first one.
    void insertTemplate(QString text);
    void insertEnvironment();
    void closeEnvironment();
    static QString closingFor(const QString &textBefore);
    static QList<OutlineEntry> outline(const QString &text);
    void setWordWrap(bool on);
    bool wordWrap() const { return lineWrapMode() != QTextEdit::NoWrap; }
signals:
    void wordWrapToggled(bool on);
protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
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
    void complete(const QString &text);
    void updateCompletion(bool request);
    bool jumpPlaceholder(int step);
    void scan();
    QWidget *m_gutter;
    int gutterWidth() const;
    void paintGutter();
    void updateHighlights();
};

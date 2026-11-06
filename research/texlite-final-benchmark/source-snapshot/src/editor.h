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

class Editor : public QTextEdit
{
    Q_OBJECT
public:
    explicit Editor(QWidget *parent = nullptr);
    void goToLine(int line, int column = 0);
    int lineNumber() const;
    void toggleComment();
protected:
    void keyPressEvent(QKeyEvent *event) override;
private:
    QCompleter *m_completion;
    void complete(const QString &text);
};

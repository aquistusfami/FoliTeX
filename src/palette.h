#pragma once
#include <QFrame>
#include <functional>

class QLabel;
class QLineEdit;
class QListWidget;

struct PaletteItem
{
    QString title;
    QString group; // shown dimmed at the right, with hint
    QString hint;  // shortcut or line number
    std::function<void()> run;
};

// Fuzzy-matching list over the window: commands (Ctrl+Shift+P) or files and sections (Ctrl+P).
class Palette : public QFrame
{
    Q_OBJECT
public:
    explicit Palette(QWidget *parent);
    void open(const QString &placeholder, const QString &scope, QList<PaletteItem> items);
    // Subsequence match; 0 = no match. Consecutive and word-start characters score higher.
    static int score(const QString &text, const QString &query);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
private:
    void refresh();
    void run(int row);
    QFrame *m_box;
    QLabel *m_scope;
    QLineEdit *m_input;
    QListWidget *m_list;
    QList<PaletteItem> m_items;
    QString m_query;
    QList<int> m_shown; // indexes into m_items, best first
};

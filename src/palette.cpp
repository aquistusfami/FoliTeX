#include "palette.h"
#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <algorithm>

namespace
{
constexpr int rightRole = Qt::UserRole + 1;

class Delegate : public QStyledItemDelegate
{
public:
    Delegate(const QString *query, QObject *parent) : QStyledItemDelegate(parent), m_query(query) {}
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem plain = option;
        plain.text.clear();
        plain.state &= ~QStyle::State_HasFocus;
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &plain, painter, option.widget);
        QRect area = option.rect.adjusted(10, 0, -10, 0);
        const QString right = index.data(rightRole).toString();
        painter->setPen(option.palette.color(QPalette::PlaceholderText));
        painter->drawText(area, Qt::AlignRight | Qt::AlignVCenter, right);
        int reserved = painter->fontMetrics().horizontalAdvance(right) + 16;
        const QString title = index.data().toString();
        const QRect room = area.adjusted(0, 0, -reserved, 0);
        const QFontMetrics metrics = painter->fontMetrics();
        const QColor normal = option.palette.color(option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text);
        if (metrics.horizontalAdvance(title) > room.width() || m_query->isEmpty())
        {
            painter->setPen(normal);
            painter->drawText(room, Qt::AlignLeft | Qt::AlignVCenter, metrics.elidedText(title, Qt::ElideMiddle, room.width()));
            return;
        }
        // The characters the query matched are drawn bold in the heading colour.
        QFont bold = painter->font();
        bold.setBold(true);
        int x = room.left(), next = 0;
        for (const QChar ch : title)
        {
            const bool hit = next < m_query->size() && ch.toLower() == (*m_query)[next].toLower();
            if (hit) ++next;
            painter->setFont(hit ? bold : option.font);
            painter->setPen(hit ? QColor("#DCDCAA") : normal);
            painter->drawText(QRect(x, room.top(), 40, room.height()), Qt::AlignLeft | Qt::AlignVCenter, QString(ch));
            x += QFontMetrics(hit ? bold : option.font).horizontalAdvance(ch);
        }
        painter->setFont(option.font);
    }
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return {0, 26}; }
private:
    const QString *m_query;
};
}

Palette::Palette(QWidget *parent)
    : QFrame(parent), m_box(new QFrame(this)), m_scope(new QLabel), m_input(new QLineEdit), m_list(new QListWidget)
{
    // Covers the window with a dimmed scrim; the box sits near the top like Inkdrop's Telescope.
    m_box->setObjectName("paletteBox");
    m_scope->setObjectName("paletteScope");
    auto *layout = new QVBoxLayout(m_box);
    layout->setContentsMargins(0, 0, 0, 4);
    layout->setSpacing(0);
    layout->addWidget(m_input);
    layout->addWidget(m_scope);
    layout->addWidget(m_list);
    m_list->setItemDelegate(new Delegate(&m_query, m_list));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setUniformItemSizes(true);
    m_list->setFocusPolicy(Qt::NoFocus);
    m_input->installEventFilter(this);
    parent->installEventFilter(this);
    connect(m_input, &QLineEdit::textChanged, this, &Palette::refresh);
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) { run(m_list->row(item)); });
    hide();
}

void Palette::paintEvent(QPaintEvent *)
{
    QPainter(this).fillRect(rect(), QColor(8, 10, 12, 115));
}

void Palette::mousePressEvent(QMouseEvent *event)
{
    if (!m_box->geometry().contains(event->position().toPoint())) hide();
}

int Palette::score(const QString &text, const QString &query)
{
    if (query.isEmpty()) return 1;
    int points = 0, next = 0, last = -2;
    for (int i = 0; i < text.size() && next < query.size(); ++i)
        if (text[i].toLower() == query[next].toLower())
        {
            points += (i == last + 1 ? 3 : 1) + (i == 0 || text[i - 1] == ' ' || text[i - 1] == '/' ? 2 : 0);
            last = i;
            ++next;
        }
    return next == query.size() ? points : 0;
}

void Palette::open(const QString &placeholder, const QString &scope, QList<PaletteItem> items)
{
    m_items = std::move(items);
    m_input->setPlaceholderText(placeholder);
    m_scope->setText(scope);
    m_input->clear();
    setGeometry(parentWidget()->rect());
    refresh();
    show();
    raise();
    m_input->setFocus();
}

void Palette::refresh()
{
    m_query = m_input->text().trimmed();
    const QString &query = m_query;
    QList<std::pair<int, int>> scored; // score, index
    for (int i = 0; i < m_items.size(); ++i)
        if (int s = score(m_items[i].title, query)) scored.append({s, i});
    std::stable_sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    m_list->clear();
    m_shown.clear();
    for (const auto &[points, index] : scored)
    {
        (void)points;
        auto *item = new QListWidgetItem(m_items[index].title, m_list);
        item->setData(rightRole, (m_items[index].hint.isEmpty() ? QString() : m_items[index].hint + "   ") + m_items[index].group);
        m_shown << index;
    }
    if (m_list->count()) m_list->setCurrentRow(0);
    m_list->setFixedHeight(std::clamp(m_list->count(), 1, 11) * 26 + 6);
    const int width = std::min(560, this->width() - 24);
    m_box->setFixedWidth(width);
    m_box->adjustSize();
    m_box->move((this->width() - width) / 2, 48);
}

void Palette::run(int row)
{
    if (row < 0 || row >= m_shown.size()) return;
    auto action = m_items[m_shown[row]].run;
    hide();
    if (action) action();
}

bool Palette::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget())
    {
        if (event->type() == QEvent::Resize && isVisible()) { setGeometry(parentWidget()->rect()); m_box->move((width() - m_box->width()) / 2, 48); }
        return false;
    }
    if (watched != m_input) return false;
    if (event->type() == QEvent::FocusOut) { hide(); return false; }
    if (event->type() != QEvent::KeyPress) return false;
    switch (static_cast<QKeyEvent *>(event)->key())
    {
    case Qt::Key_Escape: hide(); return true;
    case Qt::Key_Return: case Qt::Key_Enter: run(m_list->currentRow()); return true;
    case Qt::Key_Down: m_list->setCurrentRow(std::min(m_list->currentRow() + 1, m_list->count() - 1)); return true;
    case Qt::Key_Up: m_list->setCurrentRow(std::max(m_list->currentRow() - 1, 0)); return true;
    case Qt::Key_PageDown: m_list->setCurrentRow(std::min(m_list->currentRow() + 8, m_list->count() - 1)); return true;
    case Qt::Key_PageUp: m_list->setCurrentRow(std::max(m_list->currentRow() - 8, 0)); return true;
    default: return false;
    }
}

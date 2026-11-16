#pragma once
#include <QWidget>
#include <QPointer>
#include <functional>

class QComboBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

// Runs a coding-agent CLI the user already has (Claude Code, Codex, ...) in the project folder.
// Its edits reach the editor through the source watcher, which reloads open files undoably.
class AgentPanel : public QWidget
{
    Q_OBJECT
    friend class Tests;
public:
    struct Agent { QString name, program, promptFlag; }; // promptFlag: how the CLI takes a first prompt (empty = positional)
    explicit AgentPanel(std::function<QString()> directory, QWidget *parent = nullptr);
    static QList<Agent> installedAgents(); // known CLIs found on PATH
    static bool embedded(); // built with qtermwidget
    void start(const QString &prompt = {});
    void openExternal(const QString &prompt = {});
    void send(const QString &prompt); // submits it to the running agent, or starts one with it
    QString directory() const { return m_directory(); }
    bool running() const { return !m_terminal.isNull(); }
    void refresh(); // re-scan PATH
private:
    std::function<QString()> m_directory;
    QComboBox *m_agents;
    QPushButton *m_start;
    QLabel *m_hint;
    QVBoxLayout *m_layout;
    QPointer<QWidget> m_terminal;
    QString program() const;
    QStringList command(const QString &prompt) const; // the agent's arguments for a first prompt
    void stop();
};

#include "agent.h"
#include "editor.h"
#include <QComboBox>
#include <QDir>
#include <QFontDatabase>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#ifdef FOLITEX_TERMINAL
#include <qtermwidget.h>
#endif

QList<AgentPanel::Agent> AgentPanel::installedAgents()
{
    static const QList<Agent> known = {{"Claude Code", "claude", ""}, {"Codex", "codex", ""}, {"Gemini CLI", "gemini", "-i"}, {"Antigravity", "agy", "-i"}, {"OpenCode", "opencode", "--prompt"}};
    // A desktop launcher may not have the user's shell PATH, where these CLIs usually install.
    const QString home = QDir::homePath();
    QStringList extra = {home + "/.local/bin", home + "/.npm-global/bin", home + "/.bun/bin", home + "/.opencode/bin"};
    if (!qEnvironmentVariableIsEmpty("APPDATA")) extra << qEnvironmentVariable("APPDATA") + "/npm"; // npm's global folder on Windows
    QList<Agent> found;
    for (const Agent &agent : known)
    {
        QString path = QStandardPaths::findExecutable(agent.program);
        if (path.isEmpty()) path = QStandardPaths::findExecutable(agent.program, extra);
        if (!path.isEmpty()) found.append({agent.name, path, agent.promptFlag});
    }
    return found;
}

bool AgentPanel::embedded()
{
#ifdef FOLITEX_TERMINAL
    return true;
#else
    return false;
#endif
}

AgentPanel::AgentPanel(std::function<QString()> directory, QWidget *parent)
    : QWidget(parent), m_directory(std::move(directory))
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 6, 8, 6);
    m_layout->setSpacing(6);
    auto *row = new QHBoxLayout;
    m_agents = new QComboBox;
    m_start = new QPushButton("Start");
    auto *external = new QPushButton("Open in terminal");
    external->setToolTip("Run the agent in your own terminal emulator ($TERMINAL)");
    row->addWidget(m_agents);
    row->addWidget(m_start);
    row->addWidget(external);
    row->addStretch();
    m_layout->addLayout(row);
    m_hint = new QLabel;
    m_hint->setObjectName("agentHint");
    m_hint->setWordWrap(true);
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_layout->addWidget(m_hint, 1);
    connect(m_start, &QPushButton::clicked, this, [this] { start(); });
    connect(external, &QPushButton::clicked, this, [this] { openExternal(); });
    connect(m_agents, &QComboBox::activated, this, [this] { QSettings().setValue("agent", m_agents->currentText()); });
    refresh();
}

void AgentPanel::refresh()
{
    const QString chosen = m_agents->count() ? m_agents->currentText() : QSettings().value("agent").toString();
    m_agents->clear();
    for (const Agent &agent : installedAgents()) m_agents->addItem(agent.name, QStringList{agent.program, agent.promptFlag});
    if (int i = m_agents->findText(chosen); i >= 0) m_agents->setCurrentIndex(i);
    const bool any = m_agents->count();
    m_agents->setEnabled(any);
    for (auto *button : findChildren<QPushButton *>()) button->setEnabled(any);
    if (m_terminal) return;
    m_hint->setText(any ? QString("Runs the agent in %1. Files it edits reload here; Ctrl+Z undoes a reload.%2")
                              .arg(QDir::toNativeSeparators(m_directory()),
                                   embedded() ? "" : "\nThis build has no embedded terminal (qtermwidget); Start opens your terminal.")
                        : "No coding agent found. Install Claude Code (claude), Codex (codex), Gemini CLI (gemini), Antigravity (agy) or OpenCode (opencode) and sign in once in a terminal.");
}

QString AgentPanel::program() const
{
    return m_agents->currentData().toStringList().value(0);
}

QStringList AgentPanel::command(const QString &prompt) const
{
    if (prompt.isEmpty()) return {};
    const QString flag = m_agents->currentData().toStringList().value(1);
    return flag.isEmpty() ? QStringList{prompt} : QStringList{flag, prompt};
}

void AgentPanel::send(const QString &prompt)
{
    if (program().isEmpty()) { refresh(); return; } // the hint says what to install
#ifdef FOLITEX_TERMINAL
    if (auto *terminal = qobject_cast<QTermWidget *>(m_terminal.data()))
    {
        terminal->sendText(prompt);
        // Enter on its own, so a TUI that takes fast input for a paste still submits it.
        QTimer::singleShot(150, terminal, [terminal] { terminal->sendText("\r"); });
        terminal->setFocus();
        return;
    }
#endif
    start(prompt);
}

void AgentPanel::start(const QString &prompt)
{
    const QString exe = program();
    if (exe.isEmpty()) return;
#ifdef FOLITEX_TERMINAL
    stop();
    auto *terminal = new QTermWidget(0, this);
    terminal->setShellProgram(exe);
    terminal->setArgs(command(prompt));
    terminal->setWorkingDirectory(m_directory());
    terminal->setEnvironment({"TERM=xterm-256color", "COLORTERM=truecolor"});
    terminal->setColorScheme(lightTheme() ? "BlackOnWhite" : ":/folitex-dark.colorscheme"); // dark: DarkPastels on the panel colour
    terminal->setTerminalFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    terminal->setScrollBarPosition(QTermWidget::ScrollBarRight);
    connect(terminal, &QTermWidget::finished, this, [this]
    {
        stop();
        m_hint->setText("The agent exited. Start runs it again.");
    });
    m_hint->hide();
    m_layout->addWidget(terminal, 1);
    m_terminal = terminal;
    m_start->setText("Restart");
    terminal->startShellProgram();
    terminal->setFocus();
#else
    openExternal(prompt);
#endif
}

void AgentPanel::stop()
{
    QWidget *terminal = m_terminal;
    m_terminal = nullptr;
    if (!terminal) return;
    terminal->disconnect(this);
    terminal->deleteLater(); // closes the pty, which hangs up the agent
    m_hint->show();
    m_start->setText("Start");
}

void AgentPanel::openExternal(const QString &prompt)
{
    const QString exe = program();
    if (exe.isEmpty()) return;
#ifdef Q_OS_WIN
    // A console program started detached from a GUI app gets its own console window.
    // npm installs .cmd shims, which only cmd.exe runs.
    QStringList arguments = this->command(prompt);
    QString launcher = exe;
    if (exe.endsWith(".cmd", Qt::CaseInsensitive) || exe.endsWith(".bat", Qt::CaseInsensitive))
    {
        arguments = QStringList{"/c", exe} + arguments;
        launcher = "cmd.exe";
    }
    if (!QProcess::startDetached(launcher, arguments, m_directory())) m_hint->setText("Could not start " + exe + ".");
    return;
#endif
    QStringList command = QProcess::splitCommand(qEnvironmentVariable("TERMINAL"));
    if (!command.isEmpty()) command << "-e";
    else
    {
        static const QList<QStringList> terminals = {{"foot"}, {"kitty"}, {"alacritty", "-e"}, {"wezterm", "start", "--"}, {"konsole", "-e"}, {"gnome-terminal", "--"}, {"xterm", "-e"}};
        for (const QStringList &candidate : terminals)
            if (!QStandardPaths::findExecutable(candidate.first()).isEmpty()) { command = candidate; break; }
    }
    if (command.isEmpty()) { m_hint->setText("No terminal emulator found; set $TERMINAL."); return; }
    const QString program = command.takeFirst();
    if (!QProcess::startDetached(program, command << exe << this->command(prompt), m_directory()))
        m_hint->setText("Could not start " + program + ".");
}

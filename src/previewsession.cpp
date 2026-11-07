#include "previewsession.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSet>

namespace {
// The floating window's title, so Oma Easel can tell when it has been closed.
const char *windowTitle = "Oma Easel preview";
}

PreviewSession::PreviewSession(const Commands &commands, QObject *parent)
    : QObject(parent), m_commands(commands) {
    if (m_commands.currentBackgroundLink.isEmpty())
        m_commands.currentBackgroundLink = QDir::homePath() + "/.local/state/omarchy/current/background";
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &PreviewSession::checkResult);
    // Closing the terminal without choosing counts as exiting the preview.
    m_poll.setInterval(1000);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        checkResult();
        if (!m_active)
            return;
        const auto clients = QJsonDocument::fromJson(hyprctl({"clients", "-j"})).array();
        for (const QJsonValue &client : clients)
            if (client.toObject().value("title").toString() == windowTitle)
                return;
        emit decided(false);
    });
}

PreviewSession::~PreviewSession() {
    if (m_active)
        finish(false);
}

QByteArray PreviewSession::hyprctl(const QStringList &arguments) const {
    QProcess process;
    process.start(m_commands.hyprctl, arguments);
    if (!process.waitForFinished(3000))
        return {};
    return process.readAllStandardOutput();
}

void PreviewSession::dispatch(const QString &lua) const {
    hyprctl({"dispatch", lua});
}

// The lowest workspace number with no windows on it.
int PreviewSession::emptyWorkspace(const QByteArray &workspacesJson) {
    QSet<int> busy;
    for (const QJsonValue &value : QJsonDocument::fromJson(workspacesJson).array()) {
        const QJsonObject workspace = value.toObject();
        if (workspace.value("windows").toInt() > 0)
            busy.insert(workspace.value("id").toInt());
    }
    int id = 1;
    while (busy.contains(id))
        ++id;
    return id;
}

// gum is what Omarchy's own terminal prompts use; plain read is the fallback.
QString PreviewSession::promptScript(const QString &label, const QString &resultFile) {
    QString quotedLabel = label;
    quotedLabel.replace('\'', "'\\''");
    QString quotedResult = resultFile;
    quotedResult.replace('\'', "'\\''");
    return QString(
               "source omarchy-restart-gum >/dev/null 2>&1\n"
               "label='%1'\n"
               "if command -v gum >/dev/null; then\n"
               "  choice=$(gum choose --header \"Previewing $label\" 'Keep this background' 'Exit preview')\n"
               "else\n"
               "  printf 'Previewing %%s\\n\\n  k  keep this background\\n  any other key  exit preview\\n' \"$label\"\n"
               "  read -rsn1 key; [ \"$key\" = k ] && choice='Keep this background'\n"
               "fi\n"
               "case \"$choice\" in 'Keep this background') echo keep ;; *) echo exit ;; esac > '%2'\n")
        .arg(quotedLabel, quotedResult);
}

bool PreviewSession::perScreen() const {
    QProcess process;
    process.start(m_commands.shell, {"background", "shown"});
    if (!process.waitForFinished(3000))
        return false;
    return process.readAllStandardOutput().contains('\t');
}

void PreviewSession::rescanVersions() const {
    QProcess::startDetached(m_commands.shell, {"-q", "background", "rescanVersions"});
}

bool PreviewSession::start(const QString &image, const QString &label, QString *error, const QString &monitor,
                           const QString &paired) {
    if (m_active)
        finish(false);
    m_work = std::make_unique<QTemporaryDir>();
    if (!m_work->isValid()) {
        *error = tr("Could not create a folder for the preview.");
        return false;
    }

    // Where to come back to: Oma Easel's own window and workspace.
    const qint64 pid = QCoreApplication::applicationPid();
    for (const QJsonValue &value : QJsonDocument::fromJson(hyprctl({"clients", "-j"})).array()) {
        const QJsonObject client = value.toObject();
        if (client.value("pid").toInteger() == pid) {
            m_returnWindow = client.value("address").toString();
            m_returnWorkspace = QString::number(client.value("workspace").toObject().value("id").toInt());
        }
    }
    const int workspace = emptyWorkspace(hyprctl({"workspaces", "-j"}));
    m_previousBackground = QFileInfo(m_commands.currentBackgroundLink).symLinkTarget();

    m_screenOnly = !monitor.isEmpty() && perScreen();
    if (m_screenOnly) {
        if (!paired.isEmpty())
            setBackground(paired);
        QProcess::startDetached(m_commands.shell, {"-q", "background", "setScreen", monitor, image});
    } else {
        setBackground(image);
    }
    if (!monitor.isEmpty())
        dispatch(QString("hl.dsp.focus({ monitor = \"%1\" })").arg(monitor));
    dispatch(QString("hl.dsp.focus({ workspace = \"%1\" })").arg(workspace));
    // An empty workspace can still belong to another monitor.
    if (!monitor.isEmpty())
        dispatch(QString("hl.dsp.workspace.move({ workspace = \"%1\", monitor = \"%2\" })").arg(workspace).arg(monitor));

    const QString script = m_work->filePath("prompt.sh");
    QFile file(script);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = tr("Could not write the preview prompt.");
        return false;
    }
    file.write(promptScript(label, m_work->filePath("result")).toUtf8());
    file.close();
    // TUI.float is the app id Omarchy floats and centers.
    if (!QProcess::startDetached(m_commands.terminal, {"--app-id=TUI.float", QString("--title=%1").arg(windowTitle),
                                                      "-e", "bash", script})) {
        *error = tr("Could not open a terminal for the preview (%1).").arg(m_commands.terminal);
        finish(false);
        return false;
    }
    m_watcher.addPath(m_work->path());
    m_active = true;
    // Give the terminal time to appear before treating its absence as closed.
    QTimer::singleShot(4000, this, [this] { if (m_active) m_poll.start(); });
    return true;
}

void PreviewSession::setBackground(const QString &image) {
    QProcess::startDetached(m_commands.setBackground, {image});
}

void PreviewSession::checkResult() {
    if (!m_active || !m_work)
        return;
    QFile result(m_work->filePath("result"));
    if (!result.open(QIODevice::ReadOnly))
        return;
    const bool keep = result.readAll().trimmed() == "keep";
    emit decided(keep);
}

void PreviewSession::finish(bool keep) {
    if (!m_active && !m_work)
        return;
    m_active = false;
    m_poll.stop();
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    if (m_screenOnly) {
        QProcess::startDetached(m_commands.shell, {"-q", "background", "clearScreens"});
        m_screenOnly = false;
    }
    if (!keep && !m_previousBackground.isEmpty() && QFileInfo(m_previousBackground).isFile())
        setBackground(m_previousBackground);
    // Close the prompt if it is still open, then go back to Oma Easel.
    dispatch(QString("hl.dsp.window.close({ window = \"title:^%1$\" })").arg(windowTitle));
    if (!m_returnWorkspace.isEmpty())
        dispatch(QString("hl.dsp.focus({ workspace = \"%1\" })").arg(m_returnWorkspace));
    if (!m_returnWindow.isEmpty())
        dispatch(QString("hl.dsp.focus({ window = \"address:%1\" })").arg(m_returnWindow));
    m_work.reset();
}

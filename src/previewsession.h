#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>

// Shows a candidate as the desktop background on an empty workspace, with a
// small floating terminal asking whether to keep it. Keeping leaves the new
// background in place; exiting puts the old one back. Either way the user
// lands back on the workspace easel is on.
class PreviewSession : public QObject {
    Q_OBJECT

public:
    struct Commands {
        QString hyprctl = "hyprctl";
        QString setBackground = "omarchy-theme-bg-set";
        QString terminal = "xdg-terminal-exec";
        QString currentBackgroundLink;   // ~/.local/state/omarchy/current/background
    };

    explicit PreviewSession(const Commands &commands, QObject *parent = nullptr);
    ~PreviewSession() override;

    bool active() const { return m_active; }
    bool start(const QString &image, const QString &label, QString *error);
    void setBackground(const QString &image);
    void finish(bool keep);

    static int emptyWorkspace(const QByteArray &workspacesJson);
    static QString promptScript(const QString &label, const QString &resultFile);

signals:
    // The user chose; on keep the caller decides what to store before the
    // background is final.
    void decided(bool keep);

private:
    void checkResult();
    QByteArray hyprctl(const QStringList &arguments) const;
    void dispatch(const QString &lua) const;

    Commands m_commands;
    bool m_active = false;
    QString m_previousBackground;
    QString m_returnWorkspace;
    QString m_returnWindow;
    std::unique_ptr<QTemporaryDir> m_work;
    QFileSystemWatcher m_watcher;
    QTimer m_poll;
};

#include "paths.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace {
QString xdg(const char *variable, const QString &fallback) {
    const QString value = qEnvironmentVariable(variable);
    return value.isEmpty() ? QDir::homePath() + fallback : value;
}
}

Paths Paths::defaults() {
    const QString config = xdg("XDG_CONFIG_HOME", "/.config");
    const QString state = xdg("XDG_STATE_HOME", "/.local/state");
    const QString omarchy = qEnvironmentVariable("OMARCHY_PATH", "/usr/share/omarchy");
    const QString codexHome = xdg("CODEX_HOME", "/.codex");

    Paths paths;
    paths.userThemes = config + "/omarchy/themes";
    paths.stockThemes = omarchy + "/themes";
    paths.userBackgrounds = config + "/omarchy/backgrounds";
    paths.currentThemeName = state + "/omarchy/current/theme.name";
    paths.cache = xdg("XDG_CACHE_HOME", "/.cache") + "/oma-easel";
    paths.data = xdg("XDG_DATA_HOME", "/.local/share") + "/oma-easel";
    paths.codexImages = codexHome + "/generated_images";
    paths.settings = config + "/oma-easel/settings.json";
    return paths;
}

// Batches and masters record absolute paths (a master's candidate, a
// batch's references), so moved files have their recorded paths rewritten
// too; otherwise kept candidates would stop showing as kept.
void Paths::migrateLegacy() {
    const Paths now = defaults();
    const QString config = QFileInfo(now.settings).absolutePath();
    const QList<QPair<QString, QString>> moves{
        {QFileInfo(now.cache).absolutePath() + "/easel", now.cache},
        {QFileInfo(now.data).absolutePath() + "/easel", now.data},
        {QFileInfo(config).absolutePath() + "/easel", config}};
    QList<QPair<QString, QString>> moved;
    for (const auto &[from, to] : moves)
        if (QFileInfo(from).isDir() && !QFileInfo::exists(to) && QDir().rename(from, to))
            moved.append({from + "/", to + "/"});
    if (moved.isEmpty())
        return;
    for (const QString &root : {now.cache, now.data}) {
        QDirIterator it(root, {"*.json"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile file(it.next());
            if (!file.open(QIODevice::ReadOnly))
                continue;
            QString text = QString::fromUtf8(file.readAll());
            file.close();
            const QString before = text;
            for (const auto &[from, to] : moved)
                text.replace(from, to);
            if (text != before && file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                file.write(text.toUtf8());
        }
    }
}

// The same layout under one directory, for tests.
Paths Paths::underRoot(const QString &root) {
    Paths paths;
    paths.userThemes = root + "/config/omarchy/themes";
    paths.stockThemes = root + "/omarchy/themes";
    paths.userBackgrounds = root + "/config/omarchy/backgrounds";
    paths.currentThemeName = root + "/state/omarchy/current/theme.name";
    paths.cache = root + "/cache/oma-easel";
    paths.data = root + "/data/oma-easel";
    paths.codexImages = root + "/codex/generated_images";
    paths.settings = root + "/config/oma-easel/settings.json";
    return paths;
}

#include "paths.h"

#include <QDir>

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
    paths.cache = xdg("XDG_CACHE_HOME", "/.cache") + "/easel";
    paths.data = xdg("XDG_DATA_HOME", "/.local/share") + "/easel";
    paths.codexImages = codexHome + "/generated_images";
    return paths;
}

// The same layout under one directory, for tests.
Paths Paths::underRoot(const QString &root) {
    Paths paths;
    paths.userThemes = root + "/config/omarchy/themes";
    paths.stockThemes = root + "/omarchy/themes";
    paths.userBackgrounds = root + "/config/omarchy/backgrounds";
    paths.currentThemeName = root + "/state/omarchy/current/theme.name";
    paths.cache = root + "/cache/easel";
    paths.data = root + "/data/easel";
    paths.codexImages = root + "/codex/generated_images";
    return paths;
}

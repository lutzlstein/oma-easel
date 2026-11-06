#pragma once

#include <QString>

// Every location easel reads or writes, in one place so tests can point the
// whole app at a temporary directory instead of the user's home.
struct Paths {
    QString userThemes;        // ~/.config/omarchy/themes
    QString stockThemes;       // $OMARCHY_PATH/themes
    QString userBackgrounds;   // ~/.config/omarchy/backgrounds
    QString currentThemeName;  // ~/.local/state/omarchy/current/theme.name
    QString cache;             // ~/.cache/easel, for candidates that may be thrown away
    QString data;              // ~/.local/share/easel, for kept lossless masters
    QString codexImages;       // ~/.codex/generated_images
    QString settings;          // ~/.config/easel/settings.json

    static Paths defaults();
    static Paths underRoot(const QString &root);
};

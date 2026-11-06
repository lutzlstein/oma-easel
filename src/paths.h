#pragma once

#include <QString>

// Every location Oma Easel reads or writes, in one place so tests can point the
// whole app at a temporary directory instead of the user's home.
struct Paths {
    QString userThemes;        // ~/.config/omarchy/themes
    QString stockThemes;       // $OMARCHY_PATH/themes
    QString userBackgrounds;   // ~/.config/omarchy/backgrounds
    QString currentThemeName;  // ~/.local/state/omarchy/current/theme.name
    QString cache;             // ~/.cache/oma-easel, for candidates that may be thrown away
    QString data;              // ~/.local/share/oma-easel, for kept lossless masters
    QString codexImages;       // ~/.codex/generated_images
    QString settings;          // ~/.config/oma-easel/settings.json

    static Paths defaults();
    // Moves data from before the rename (~/.cache/easel, ~/.local/share/easel,
    // ~/.config/easel) to the oma-easel folders, once.
    static void migrateLegacy();
    static Paths underRoot(const QString &root);
};

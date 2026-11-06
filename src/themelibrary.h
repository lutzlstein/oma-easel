#pragma once

#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <optional>

#include "paths.h"

struct ThemeInfo {
    QString slug;
    QString name;
    QMap<QString, QString> colors;     // colors.toml keys to "#rrggbb"
    QStringList backgrounds;           // absolute paths, in Omarchy's cycle order
    QString userBackgroundDirectory;   // where easel adds new backgrounds

    QJsonObject toJson() const;
};

// Reads installed Omarchy themes the way omarchy-theme-set does: a theme in
// ~/.config/omarchy/themes overlays the stock theme of the same name, and
// ~/.config/omarchy/backgrounds/<slug>/ adds backgrounds to any theme.
class ThemeLibrary {
public:
    explicit ThemeLibrary(const Paths &paths);

    QStringList slugs() const;
    QString currentSlug() const;
    std::optional<ThemeInfo> load(const QString &slugOrName) const;

    static QString displayName(const QString &slug);
    static QString slugFor(const QString &slugOrName);
    static QMap<QString, QString> readColors(const QString &path);
    static bool isImage(const QString &fileName);

private:
    Paths m_paths;
};

#include "themelibrary.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

ThemeLibrary::ThemeLibrary(const Paths &paths) : m_paths(paths) {}

QStringList ThemeLibrary::slugs() const {
    QSet<QString> found;
    for (const QString &root : {m_paths.userThemes, m_paths.stockThemes})
        for (const QString &entry : QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            found.insert(entry);
    QStringList sorted(found.begin(), found.end());
    sorted.sort();
    return sorted;
}

QString ThemeLibrary::currentSlug() const {
    QFile file(m_paths.currentThemeName);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readLine()).trimmed();
}

// Same transformation as omarchy-theme-list: "tokyo-night" becomes "Tokyo Night".
QString ThemeLibrary::displayName(const QString &slug) {
    QStringList words = slug.split('-', Qt::SkipEmptyParts);
    for (QString &word : words)
        word[0] = word[0].toUpper();
    return words.join(' ');
}

// Accepts either form, so "Tokyo Night" and "tokyo-night" name the same theme.
QString ThemeLibrary::slugFor(const QString &slugOrName) {
    return slugOrName.trimmed().toLower().replace(QRegularExpression("\\s+"), "-");
}

QMap<QString, QString> ThemeLibrary::readColors(const QString &path) {
    QMap<QString, QString> colors;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return colors;
    static const QRegularExpression line(R"re(^\s*([A-Za-z0-9_]+)\s*=\s*["'](#[0-9a-fA-F]{6})["'])re",
                                         QRegularExpression::MultilineOption);
    auto it = line.globalMatch(QString::fromUtf8(file.readAll()));
    while (it.hasNext()) {
        const auto match = it.next();
        colors.insert(match.captured(1), match.captured(2).toLower());
    }
    return colors;
}

bool ThemeLibrary::isImage(const QString &fileName) {
    static const QSet<QString> extensions{"png", "jpg", "jpeg", "webp", "bmp"};
    return extensions.contains(QFileInfo(fileName).suffix().toLower());
}

std::optional<ThemeInfo> ThemeLibrary::load(const QString &slugOrName) const {
    const QString slug = slugFor(slugOrName);
    const QString user = m_paths.userThemes + "/" + slug;
    const QString stock = m_paths.stockThemes + "/" + slug;
    if (slug.isEmpty() || (!QFileInfo(user).isDir() && !QFileInfo(stock).isDir()))
        return std::nullopt;

    ThemeInfo theme;
    theme.slug = slug;
    theme.name = displayName(slug);
    theme.colors = readColors(QFileInfo::exists(user + "/colors.toml") ? user + "/colors.toml"
                                                                         : stock + "/colors.toml");
    theme.userBackgroundDirectory = m_paths.userBackgrounds + "/" + slug;

    // A user overlay's file replaces the stock file of the same name, then the
    // user's extra backgrounds follow, each set sorted like Omarchy sorts them.
    QMap<QString, QString> themeBackgrounds;
    for (const QString &root : {stock, user}) {
        const QDir directory(root + "/backgrounds");
        for (const QString &file : directory.entryList(QDir::Files))
            if (isImage(file))
                themeBackgrounds.insert(file, directory.absoluteFilePath(file));
    }
    QMap<QString, QString> extras;
    const QDir extraDirectory(theme.userBackgroundDirectory);
    for (const QString &file : extraDirectory.entryList(QDir::Files))
        if (isImage(file))
            extras.insert(file, extraDirectory.absoluteFilePath(file));
    theme.backgrounds = extras.values() + themeBackgrounds.values();
    return theme;
}

QJsonObject ThemeInfo::toJson() const {
    QJsonObject colorObject;
    for (auto it = colors.begin(); it != colors.end(); ++it)
        colorObject.insert(it.key(), it.value());
    return {{"slug", slug},
            {"name", name},
            {"colors", colorObject},
            {"backgrounds", QJsonArray::fromStringList(backgrounds)},
            {"userBackgroundDirectory", userBackgroundDirectory}};
}

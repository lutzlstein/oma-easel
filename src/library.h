#pragma once

#include <QHash>
#include <QJsonObject>
#include <QStringList>
#include <optional>

#include "paths.h"
#include "screens.h"
#include "themelibrary.h"

// Where candidates, masters, and exported backgrounds live. A batch is a
// cache directory of candidate PNGs plus batch.json describing how they were
// made; keeping a candidate copies it to a lossless master with a sidecar
// and exports a WebP into the theme's user backgrounds, which joins
// Omarchy's background cycle.
class Library {
public:
    struct Kept {
        QString master;
        QString sidecar;
        QString exported;
        QJsonObject toJson() const;
    };

    explicit Library(const Paths &paths, const QString &magick = "magick");

    QString createBatch(const QString &themeSlug, const QJsonObject &info, QString *error) const;
    static QString candidatePath(const QString &batch, int index);
    static QJsonObject readBatch(const QString &batch);
    static QStringList importInto(const QString &batch, const QStringList &files, QString *error);

    std::optional<Kept> keep(const QString &candidate, const ThemeInfo &theme, const Target &target,
                             const QString &name, QString *error) const;

    // Backgrounds easel added to a theme: export path -> master path.
    QHash<QString, QString> addedBackgrounds(const QString &themeSlug) const;
    // Moves an added background, its master, and its sidecar to the trash.
    bool remove(const QString &exported, const QString &themeSlug, QString *error) const;

    static QString slugify(const QString &text, int maxWords = 5);
    static int nextNumber(const QStringList &fileNames);

private:
    bool exportWebp(const QString &source, const QString &destination, const Target &target,
                    QString *error) const;

    Paths m_paths;
    QString m_magick;
};

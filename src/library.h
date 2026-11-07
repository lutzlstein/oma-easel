#pragma once

#include <QHash>
#include <QJsonObject>
#include <QStringList>
#include <optional>

#include "paths.h"
#include "screens.h"
#include "themelibrary.h"

// How kept backgrounds are written: format and quality, and an optional
// second folder (a theme repo, say) that gets a copy.
struct ExportOptions {
    QString format = "webp";
    int quality = 85;
    QString extraFolder;
};

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
        QString extraCopy;
        QJsonObject toJson() const;
    };

    using ExportOptions = ::ExportOptions;

    explicit Library(const Paths &paths, const QString &magick = "magick");

    QString createBatch(const QString &themeSlug, const QJsonObject &info, QString *error) const;
    static QString candidatePath(const QString &batch, int index);
    static QJsonObject readBatch(const QString &batch);
    // Records which shape an imported candidate is for, by its own aspect.
    static void recordShape(const QString &candidate, const Target &target);
    static QStringList importInto(const QString &batch, const QStringList &files, QString *error);

    // With `variantOf`, the candidate is saved as that background's version
    // for another screen shape: same name, in a folder named for the shape
    // ("9x16/7-add-boat.webp"), replacing an earlier version. Omarchy's
    // background cycle only reads the top folder, so versions never show up
    // in it as backgrounds of their own.
    std::optional<Kept> keep(const QString &candidate, const ThemeInfo &theme, const Target &target,
                             const QString &name, QString *error, const ExportOptions &options = ExportOptions(),
                             const QString &variantOf = {}) const;

    // The shape a candidate was made for, from its batch; batches from before
    // shapes existed were all made for `fallback`.
    static Target targetFor(const QString &candidate, const Target &fallback);
    // Shape keys ("9x16") a background has versions for, among the theme's
    // user backgrounds.
    static QStringList variantsOf(const ThemeInfo &theme, const QString &background);

    // Backgrounds Oma Easel added to a theme: export path -> master path.
    QHash<QString, QString> addedBackgrounds(const QString &themeSlug) const;
    // Moves an added background, its master, and its sidecar to the trash.
    bool remove(const QString &exported, const QString &themeSlug, QString *error) const;

    static QString slugify(const QString &text, int maxWords = 5);
    static int nextNumber(const QStringList &fileNames);

private:
    bool exportImage(const QString &source, const QString &destination, const Target &target,
                     const ExportOptions &options, QString *error) const;

    Paths m_paths;
    QString m_magick;
};

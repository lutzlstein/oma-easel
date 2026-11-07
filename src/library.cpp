#include "library.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

namespace {
bool writeJson(const QString &path, const QJsonObject &object) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return file.commit();
}
}

Library::Library(const Paths &paths, const QString &magick) : m_paths(paths), m_magick(magick) {}

QString Library::createBatch(const QString &themeSlug, const QJsonObject &info, QString *error) const {
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz");
    const QString batch = m_paths.cache + "/" + themeSlug + "/" + stamp;
    if (!QDir().mkpath(batch) || !writeJson(batch + "/batch.json", info)) {
        if (error)
            *error = "Could not create the candidate folder " + batch;
        return {};
    }
    return batch;
}

QString Library::candidatePath(const QString &batch, int index) {
    return QString("%1/candidate-%2.png").arg(batch).arg(index + 1);
}

QJsonObject Library::readBatch(const QString &batch) {
    QFile file(batch + "/batch.json");
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

void Library::recordShape(const QString &candidate, const Target &target) {
    const QString batch = QFileInfo(candidate).absolutePath();
    QJsonObject info = readBatch(batch);
    QJsonArray shapes = info.value("shapes").toArray();
    bool known = false;
    for (const QJsonValue &value : shapes)
        known = known || value.toObject().value("key").toString() == target.key();
    if (!known)
        shapes.append(target.toJson());
    QJsonObject byCandidate = info.value("candidateShapes").toObject();
    byCandidate.insert(QFileInfo(candidate).fileName(), target.key());
    info.insert("shapes", shapes);
    info.insert("candidateShapes", byCandidate);
    writeJson(batch + "/batch.json", info);
}

// Brings images made elsewhere into a batch as candidates, after any it
// already has. Like generated candidates they are stored as PNG, so keeping
// one works the same whichever way it was made. The originals are untouched.
QStringList Library::importInto(const QString &batch, const QStringList &files, QString *error) {
    int index = QDir(batch).entryList({"candidate-*.png"}, QDir::Files).size();
    QStringList imported;
    for (const QString &file : files) {
        const QImage image(file);
        if (image.isNull()) {
            if (error)
                *error = "Not an image Oma Easel can read: " + file;
            return {};
        }
        const QString candidate = candidatePath(batch, index++);
        if (!image.save(candidate, "PNG")) {
            if (error)
                *error = "Could not write " + candidate;
            return {};
        }
        imported << candidate;
    }
    return imported;
}

// "Misty harbor at dawn, fishing boats" becomes "misty-harbor-dawn-fishing-boats".
QString Library::slugify(const QString &text, int maxWords) {
    QString lower = text.toLower();
    lower.replace(QRegularExpression("[^a-z0-9]+"), " ");
    // Little words make names longer without saying more, so they are dropped
    // unless nothing else is left.
    static const QSet<QString> filler{"a", "an", "the", "at", "of", "with", "and", "in", "on", "to", "by", "for"};
    const QStringList all = lower.split(' ', Qt::SkipEmptyParts);
    QStringList words;
    for (const QString &word : all)
        if (!filler.contains(word))
            words << word;
    if (words.isEmpty())
        words = all;
    if (words.size() > maxWords)
        words = words.mid(0, maxWords);
    return words.isEmpty() ? QString("background") : words.join('-');
}

// Backgrounds are numbered like Omarchy's stock themes ("1-tree-tops.webp"),
// so a new one sorts after everything the theme already has.
// A master and its export share a name, so they count once.
int Library::nextNumber(const QStringList &fileNames) {
    static const QRegularExpression numbered("^(\\d+)-");
    QSet<QString> names;
    int highest = 0;
    for (const QString &path : fileNames) {
        const QString name = QFileInfo(path).completeBaseName();
        names.insert(name);
        const auto match = numbered.match(name);
        if (match.hasMatch())
            highest = std::max(highest, match.captured(1).toInt());
    }
    return std::max(highest, int(names.size())) + 1;
}

Target Library::targetFor(const QString &candidate, const Target &fallback) {
    const QJsonObject batch = readBatch(QFileInfo(candidate).absolutePath());
    const QString key = batch.value("candidateShapes").toObject().value(QFileInfo(candidate).fileName()).toString();
    for (const QJsonValue &value : batch.value("shapes").toArray())
        if (value.toObject().value("key").toString() == key)
            return Target::fromJson(value.toObject());
    if (batch.contains("target"))
        return Target::fromJson(batch.value("target").toObject());
    return fallback;
}

namespace {
// A background's version for one shape, whatever format it was exported in.
QString variantFile(const QString &directory, const QString &base) {
    const QStringList found = QDir(directory).entryList({base + ".*"}, QDir::Files);
    for (const QString &name : found)
        if (QFileInfo(name).completeBaseName() == base && !name.endsWith(".partial"))
            return QDir(directory).filePath(name);
    return {};
}

bool trash(const QString &path, QString *error) {
    if (path.isEmpty() || !QFileInfo::exists(path))
        return true;
    if (QFile::moveToTrash(path) || QFile::remove(path))
        return true;
    if (error)
        *error = "Could not move " + path + " to the trash.";
    return false;
}
}

QStringList Library::variantsOf(const ThemeInfo &theme, const QString &background) {
    const QString base = QFileInfo(background).completeBaseName();
    QStringList keys;
    static const QRegularExpression shapeFolder("^\\d+x\\d+$");
    for (const QString &folder : QDir(theme.userBackgroundDirectory).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        if (shapeFolder.match(folder).hasMatch() && !variantFile(theme.userBackgroundDirectory + "/" + folder, base).isEmpty())
            keys << folder;
    return keys;
}

std::optional<Library::Kept> Library::keep(const QString &candidate, const ThemeInfo &theme,
                                           const Target &target, const QString &name, QString *error,
                                           const ExportOptions &options, const QString &variantOf) const {
    auto failWith = [error](const QString &message) {
        if (error)
            *error = message;
        return std::nullopt;
    };
    if (!QFileInfo(candidate).isFile())
        return failWith("No candidate at " + candidate);

    const QJsonObject batch = readBatch(QFileInfo(candidate).absolutePath());
    const bool variant = !variantOf.isEmpty();
    const QString shapeFolder = variant ? "/" + target.key() : QString();
    const QString masterDirectory = m_paths.data + "/" + theme.slug + shapeFolder;
    const QString exportDirectory = theme.userBackgroundDirectory + shapeFolder;
    if (!QDir().mkpath(masterDirectory) || !QDir().mkpath(exportDirectory))
        return failWith("Could not create " + masterDirectory + " or " + exportDirectory);

    QString base;
    if (variant) {
        // A version takes its background's name; an earlier version for the
        // same shape goes to the trash.
        base = QFileInfo(variantOf).completeBaseName();
        for (const QString &old : {variantFile(exportDirectory, base), masterDirectory + "/" + base + ".png",
                                   masterDirectory + "/" + base + ".json"})
            if (!trash(old, error))
                return std::nullopt;
    } else {
        // Numbers continue after both the theme's backgrounds and earlier
        // masters, so a master and its export always share a name.
        QStringList existing = theme.backgrounds;
        for (const QString &file : QDir(masterDirectory).entryList({"*.png"}, QDir::Files))
            existing << file;
        base = QString("%1-%2")
                   .arg(nextNumber(existing))
                   .arg(slugify(name.isEmpty() ? batch.value("scene").toString() : name));
    }

    Kept kept;
    kept.master = masterDirectory + "/" + base + ".png";
    kept.sidecar = masterDirectory + "/" + base + ".json";
    const QString extension = options.format == "jpeg" ? "jpg" : options.format;
    kept.exported = exportDirectory + "/" + base + "." + extension;
    if (QFileInfo::exists(kept.master) || QFileInfo::exists(kept.exported))
        return failWith("A background named " + base + " already exists.");

    if (!QFile::copy(candidate, kept.master))
        return failWith("Could not copy the candidate to " + kept.master);

    QJsonObject sidecar = batch;
    sidecar.insert("theme", theme.slug);
    sidecar.insert("candidate", candidate);
    sidecar.insert("keptAt", QDateTime::currentDateTime().toString(Qt::ISODate));
    sidecar.insert("exported", kept.exported);
    sidecar.insert("target", target.toJson());
    if (variant)
        sidecar.insert("variantOf", variantOf);
    if (!writeJson(kept.sidecar, sidecar)) {
        QFile::remove(kept.master);
        return failWith("Could not write " + kept.sidecar);
    }

    if (!exportImage(kept.master, kept.exported, target, options, error)) {
        QFile::remove(kept.master);
        QFile::remove(kept.sidecar);
        return std::nullopt;
    }

    // The optional second copy, under the same name. A failure here doesn't
    // undo the keep; it's reported and the background stays in the theme.
    if (!options.extraFolder.isEmpty()) {
        const QString folder = options.extraFolder + shapeFolder;
        const QString copy = folder + "/" + QFileInfo(kept.exported).fileName();
        if (variant)
            QFile::remove(variantFile(folder, base));
        if (QDir().mkpath(folder) && !QFileInfo::exists(copy) && QFile::copy(kept.exported, copy))
            kept.extraCopy = copy;
        else if (error)
            *error = "Kept, but could not copy it to " + folder;
    }
    return kept;
}

// Lanczos keeps painterly brushwork better than AI upscalers, and WebP at
// quality 85 (the default) is visually lossless for wallpapers at a fraction
// of PNG's size. The file is written beside the target and renamed, so
// Omarchy's background cycle never sees half an image.
bool Library::exportImage(const QString &source, const QString &destination, const Target &target,
                          const ExportOptions &options, QString *error) const {
    const QString size = QString("%1x%2").arg(target.width).arg(target.height);
    const QString partial = destination + ".partial";
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    QStringList arguments{source, "-filter", "Lanczos", "-resize", size + "^", "-gravity", "center", "-extent", size};
    if (options.format == "png")
        arguments << "-define" << "png:compression-level=9";
    else
        arguments << "-quality" << QString::number(options.quality);
    if (options.format == "webp")
        arguments << "-define" << "webp:method=6";
    if (options.format == "jpeg")
        arguments << "-sampling-factor" << "4:4:4" << "-strip";
    arguments << options.format + ":" + partial;
    process.start(m_magick, arguments);
    if (!process.waitForStarted(5000)) {
        if (error)
            *error = "Could not run ImageMagick (" + m_magick + "). Install the imagemagick package.";
        return false;
    }
    if (!process.waitForFinished(120000) || process.exitCode() != 0) {
        QFile::remove(partial);
        if (error)
            *error = "ImageMagick failed: " + QString::fromUtf8(process.readAll()).trimmed();
        return false;
    }
    QFile::remove(destination);
    if (!QFile::rename(partial, destination)) {
        QFile::remove(partial);
        if (error)
            *error = "Could not move the export to " + destination;
        return false;
    }
    return true;
}

QHash<QString, QString> Library::addedBackgrounds(const QString &themeSlug) const {
    QHash<QString, QString> added;
    const QDir masters(m_paths.data + "/" + themeSlug);
    for (const QString &name : masters.entryList({"*.json"}, QDir::Files)) {
        QFile file(masters.filePath(name));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QString exported = QJsonDocument::fromJson(file.readAll()).object().value("exported").toString();
        if (!exported.isEmpty())
            added.insert(exported, masters.filePath(QFileInfo(name).completeBaseName() + ".png"));
    }
    return added;
}

// Only backgrounds Oma Easel added can be removed, and they go to the trash
// rather than being deleted, so a mistake can be undone from the file manager.
bool Library::remove(const QString &exported, const QString &themeSlug, QString *error) const {
    const QHash<QString, QString> added = addedBackgrounds(themeSlug);
    if (!added.contains(exported)) {
        if (error)
            *error = QFileInfo(exported).fileName() + " wasn't added by Oma Easel, so it won't be removed.";
        return false;
    }
    const QString master = added.value(exported);
    const QString base = QFileInfo(master).completeBaseName();
    const QString masters = QFileInfo(master).absolutePath();
    QStringList paths{exported, master, masters + "/" + base + ".json"};
    // Its versions for other screen shapes go with it.
    const QString exports = QFileInfo(exported).absolutePath();
    for (const QString &folder : QDir(exports).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        paths << variantFile(exports + "/" + folder, base) << masters + "/" + folder + "/" + base + ".png"
              << masters + "/" + folder + "/" + base + ".json";
    for (const QString &path : paths)
        if (!trash(path, error))
            return false;
    return true;
}

QJsonObject Library::Kept::toJson() const {
    QJsonObject json{{"master", master}, {"sidecar", sidecar}, {"exported", exported}};
    if (!extraCopy.isEmpty())
        json.insert("extraCopy", extraCopy);
    return json;
}

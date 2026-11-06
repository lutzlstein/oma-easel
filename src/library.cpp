#include "library.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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

std::optional<Library::Kept> Library::keep(const QString &candidate, const ThemeInfo &theme,
                                           const Target &target, const QString &name, QString *error) const {
    auto failWith = [error](const QString &message) {
        if (error)
            *error = message;
        return std::nullopt;
    };
    if (!QFileInfo(candidate).isFile())
        return failWith("No candidate at " + candidate);

    const QJsonObject batch = readBatch(QFileInfo(candidate).absolutePath());
    const QString masterDirectory = m_paths.data + "/" + theme.slug;
    if (!QDir().mkpath(masterDirectory) || !QDir().mkpath(theme.userBackgroundDirectory))
        return failWith("Could not create " + masterDirectory + " or " + theme.userBackgroundDirectory);

    // Numbers continue after both the theme's backgrounds and earlier
    // masters, so a master and its export always share a name.
    QStringList existing = theme.backgrounds;
    for (const QString &file : QDir(masterDirectory).entryList({"*.png"}, QDir::Files))
        existing << file;
    const QString base = QString("%1-%2")
                             .arg(nextNumber(existing))
                             .arg(slugify(name.isEmpty() ? batch.value("scene").toString() : name));

    Kept kept;
    kept.master = masterDirectory + "/" + base + ".png";
    kept.sidecar = masterDirectory + "/" + base + ".json";
    kept.exported = theme.userBackgroundDirectory + "/" + base + ".webp";
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
    if (!writeJson(kept.sidecar, sidecar)) {
        QFile::remove(kept.master);
        return failWith("Could not write " + kept.sidecar);
    }

    if (!exportWebp(kept.master, kept.exported, target, error)) {
        QFile::remove(kept.master);
        QFile::remove(kept.sidecar);
        return std::nullopt;
    }
    return kept;
}

// Lanczos keeps painterly brushwork better than AI upscalers, and WebP at
// quality 85 is visually lossless for wallpapers at a fraction of PNG's size.
// The file is written beside the target and renamed, so Omarchy's background
// cycle never sees half an image.
bool Library::exportWebp(const QString &source, const QString &destination, const Target &target,
                         QString *error) const {
    const QString size = QString("%1x%2").arg(target.width).arg(target.height);
    const QString partial = destination + ".partial";
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(m_magick, {source, "-filter", "Lanczos", "-resize", size + "^", "-gravity", "center",
                             "-extent", size, "-quality", "85", "-define", "webp:method=6", "webp:" + partial});
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

QJsonObject Library::Kept::toJson() const {
    return {{"master", master}, {"sidecar", sidecar}, {"exported", exported}};
}

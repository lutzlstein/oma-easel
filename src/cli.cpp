#include "cli.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <memory>
#include <vector>

#include "codexgenerator.h"
#include "library.h"
#include "promptbuilder.h"
#include "screens.h"
#include "themelibrary.h"

namespace {
const QStringList commands{"themes", "theme", "screens", "generate", "keep", "help"};

QTextStream &out() {
    static QTextStream stream(stdout);
    return stream;
}

int failure(const QString &message) {
    QTextStream(stderr) << "easel: " << message << Qt::endl;
    return 1;
}

void printJson(const QJsonValue &value) {
    const QJsonDocument document = value.isArray() ? QJsonDocument(value.toArray()) : QJsonDocument(value.toObject());
    out() << document.toJson(QJsonDocument::Indented) << Qt::flush;
}

// Parses a subcommand's own options; "easel generate --help" shows them.
bool parse(QCommandLineParser &parser, const QStringList &arguments, QString *error) {
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.addOption({"json", "Print JSON."});
    parser.addHelpOption();
    if (!parser.parse(arguments)) {
        *error = parser.errorText();
        return false;
    }
    return true;
}

std::optional<ThemeInfo> loadTheme(const ThemeLibrary &library, const QString &requested, QString *error) {
    const QString slug = requested.isEmpty() ? library.currentSlug() : requested;
    if (slug.isEmpty()) {
        *error = "No theme given and no current Omarchy theme found.";
        return std::nullopt;
    }
    auto theme = library.load(slug);
    if (!theme)
        *error = QString("No installed theme called \"%1\". Run `easel themes` to list them.").arg(slug);
    return theme;
}

// A reference is a file path, a background file name from the theme, or the
// background's 1-based position in `easel theme`.
QString resolveReference(const QString &reference, const ThemeInfo &theme) {
    if (QFileInfo(reference).isFile())
        return QFileInfo(reference).absoluteFilePath();
    bool isNumber = false;
    const int index = reference.toInt(&isNumber);
    if (isNumber && index >= 1 && index <= theme.backgrounds.size())
        return theme.backgrounds.at(index - 1);
    for (const QString &background : theme.backgrounds)
        if (QFileInfo(background).fileName() == reference)
            return background;
    return {};
}

int themes(const QStringList &arguments, const Paths &paths) {
    QCommandLineParser parser;
    QString error;
    if (!parse(parser, arguments, &error))
        return failure(error);
    const ThemeLibrary library(paths);
    const QString current = library.currentSlug();
    QJsonArray list;
    for (const QString &slug : library.slugs()) {
        list.append(QJsonObject{{"slug", slug}, {"name", ThemeLibrary::displayName(slug)}, {"current", slug == current}});
        if (!parser.isSet("json"))
            out() << (slug == current ? "* " : "  ") << ThemeLibrary::displayName(slug) << "  (" << slug << ")\n";
    }
    if (parser.isSet("json"))
        printJson(list);
    out() << Qt::flush;
    return 0;
}

int theme(const QStringList &arguments, const Paths &paths) {
    QCommandLineParser parser;
    parser.addPositionalArgument("theme", "Theme slug or name; the current theme when left out.");
    QString error;
    if (!parse(parser, arguments, &error))
        return failure(error);
    const auto info = loadTheme(ThemeLibrary(paths), parser.positionalArguments().value(0), &error);
    if (!info)
        return failure(error);
    if (parser.isSet("json")) {
        printJson(info->toJson());
        return 0;
    }
    out() << info->name << " (" << info->slug << ")\n\nColors:\n";
    for (auto it = info->colors.begin(); it != info->colors.end(); ++it)
        out() << "  " << it.key() << " " << it.value() << "\n";
    out() << "\nBackgrounds:\n";
    for (int i = 0; i < info->backgrounds.size(); ++i)
        out() << "  " << i + 1 << ". " << info->backgrounds.at(i) << "\n";
    out() << "\nNew backgrounds go in " << info->userBackgroundDirectory << "\n" << Qt::flush;
    return 0;
}

int screens(const QStringList &arguments) {
    QCommandLineParser parser;
    QString error;
    if (!parse(parser, arguments, &error))
        return failure(error);
    const QList<Monitor> monitors = Screens::query();
    const Target target = Screens::target(monitors);
    QJsonArray list;
    for (const Monitor &monitor : monitors)
        list.append(QJsonObject{{"name", monitor.name}, {"width", monitor.width}, {"height", monitor.height}});
    if (parser.isSet("json")) {
        QJsonObject object = target.toJson();
        object.insert("monitors", list);
        printJson(object);
        return 0;
    }
    for (const Monitor &monitor : monitors)
        out() << monitor.name << "  " << monitor.width << "x" << monitor.height << "\n";
    out() << "Backgrounds export at " << target.width << "x" << target.height << " ("
          << PromptBuilder::aspectLabel(target) << ")";
    if (!target.cropped.isEmpty())
        out() << "; cropped on " << target.cropped.join(", ");
    out() << "\n" << Qt::flush;
    return 0;
}

int generate(const QStringList &arguments, const Paths &paths) {
    QCommandLineParser parser;
    parser.addPositionalArgument("theme", "Theme slug or name; the current theme when left out.");
    parser.addOption({"prompt", "The scene to paint.", "text"});
    parser.addOption({"ref", "Style reference: a file, a background name, or its number in `easel theme`. "
                             "Repeatable.", "reference"});
    parser.addOption({{"n", "count"}, "How many candidates, 1 to 6 (default 3).", "count", "3"});
    parser.addOption({"codex", "The codex binary to run.", "path", "codex"});
    QString error;
    if (!parse(parser, arguments, &error))
        return failure(error);
    const QString scene = parser.value("prompt").trimmed();
    if (scene.isEmpty())
        return failure("Describe the scene with --prompt \"...\".");
    bool validCount = false;
    const int count = parser.value("count").toInt(&validCount);
    if (!validCount || count < 1 || count > 6)
        return failure("--count must be between 1 and 6.");

    const auto info = loadTheme(ThemeLibrary(paths), parser.positionalArguments().value(0), &error);
    if (!info)
        return failure(error);
    QStringList references;
    for (const QString &reference : parser.values("ref")) {
        const QString resolved = resolveReference(reference, *info);
        if (resolved.isEmpty())
            return failure(QString("No file or %1 background matches \"%2\".").arg(info->name, reference));
        references << resolved;
    }

    const Target target = Screens::target(Screens::query());
    const QString prompt = PromptBuilder::build(scene, info->colors, references.size(), target);
    const QJsonObject batchInfo{{"scene", scene},
                                {"prompt", prompt},
                                {"references", QJsonArray::fromStringList(references)},
                                {"backend", "codex"},
                                {"theme", info->slug},
                                {"target", target.toJson()},
                                {"createdAt", QDateTime::currentDateTime().toString(Qt::ISODate)}};
    const Library library(paths);
    const QString batch = library.createBatch(info->slug, batchInfo, &error);
    if (batch.isEmpty())
        return failure(error);

    // All candidates run at once; each reports back on its own, and the
    // command returns once every one has finished or failed.
    QTextStream progress(stderr);
    progress << "Generating " << count << " candidate(s) for " << info->name << "..." << Qt::endl;
    std::vector<std::unique_ptr<CodexGenerator>> jobs;
    QJsonArray candidates, failures;
    int remaining = count;
    QEventLoop loop;
    for (int i = 0; i < count; ++i) {
        auto job = std::make_unique<CodexGenerator>(parser.value("codex"), paths.codexImages);
        const QString output = Library::candidatePath(batch, i);
        QObject::connect(job.get(), &Generator::finished, &loop, [&, i](const QString &path) {
            candidates.append(QJsonObject{{"index", i + 1}, {"path", path}});
            progress << "  candidate " << i + 1 << ": " << path << Qt::endl;
            if (--remaining == 0)
                loop.quit();
        });
        QObject::connect(job.get(), &Generator::failed, &loop, [&, i](const QString &message) {
            failures.append(QJsonObject{{"index", i + 1}, {"error", message}});
            progress << "  candidate " << i + 1 << " failed: " << message << Qt::endl;
            if (--remaining == 0)
                loop.quit();
        });
        job->start({prompt, references, output});
        jobs.push_back(std::move(job));
    }
    loop.exec();

    if (parser.isSet("json"))
        printJson(QJsonObject{{"theme", info->slug}, {"batch", batch}, {"prompt", prompt},
                              {"candidates", candidates}, {"failures", failures}});
    else
        for (const QJsonValue &candidate : candidates)
            out() << candidate.toObject().value("path").toString() << "\n";
    out() << Qt::flush;
    return candidates.isEmpty() ? failure("No candidates were generated.") : 0;
}

int keep(const QStringList &arguments, const Paths &paths) {
    QCommandLineParser parser;
    parser.addPositionalArgument("candidate", "A candidate PNG printed by `easel generate`.");
    parser.addOption({"name", "Words for the file name; the scene when left out.", "name"});
    QString error;
    if (!parse(parser, arguments, &error))
        return failure(error);
    const QString candidate = parser.positionalArguments().value(0);
    if (candidate.isEmpty())
        return failure("Give the candidate to keep.");

    const QString themeSlug = Library::readBatch(QFileInfo(candidate).absolutePath()).value("theme").toString();
    if (themeSlug.isEmpty())
        return failure(candidate + " is not part of an easel batch.");
    const auto info = loadTheme(ThemeLibrary(paths), themeSlug, &error);
    if (!info)
        return failure(error);

    const Target target = Screens::target(Screens::query());
    const auto kept = Library(paths).keep(candidate, *info, target, parser.value("name"), &error);
    if (!kept)
        return failure(error);
    if (parser.isSet("json"))
        printJson(kept->toJson());
    else
        out() << "Kept " << kept->exported << "\nMaster " << kept->master << "\n" << Qt::flush;
    return 0;
}
}

bool Cli::handles(const QStringList &arguments) {
    return arguments.size() < 2 || commands.contains(arguments.at(1)) || arguments.at(1).startsWith('-');
}

QString Cli::usage() {
    return "Usage: easel <command> [options]\n\n"
           "  themes                 List installed Omarchy themes\n"
           "  theme [theme]          Show a theme's colors and backgrounds\n"
           "  screens                Show monitors and the export size\n"
           "  generate [theme] --prompt \"...\" [--ref ...] [-n 3]\n"
           "                         Generate candidate backgrounds\n"
           "  keep <candidate> [--name \"...\"]\n"
           "                         Keep a candidate as a background for its theme\n\n"
           "Every command takes --json. Run `easel <command> --help` for its options.\n";
}

int Cli::run(const QStringList &arguments, const Paths &paths) {
    const QString command = arguments.value(1);
    // QCommandLineParser expects the program name first, so each subcommand
    // sees "easel <its options>".
    QStringList rest = arguments.mid(2);
    rest.prepend(arguments.value(0));
    if (command == "themes")
        return themes(rest, paths);
    if (command == "theme")
        return theme(rest, paths);
    if (command == "screens")
        return screens(rest);
    if (command == "generate")
        return generate(rest, paths);
    if (command == "keep")
        return keep(rest, paths);
    if (command == "--version" || command == "-v") {
        out() << QCoreApplication::applicationName() << " " << QCoreApplication::applicationVersion() << "\n"
              << Qt::flush;
        return 0;
    }
    out() << usage() << Qt::flush;
    return command.isEmpty() || command == "help" || command == "--help" || command == "-h" ? 0 : 1;
}

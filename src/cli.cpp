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

#include "backends.h"
#include "codexgenerator.h"
#include "library.h"
#include "promptbuilder.h"
#include "screens.h"
#include "themelibrary.h"

namespace {
const QStringList commands{"themes", "theme", "screens", "backends", "generate", "prompt", "import", "keep", "remove", "help"};

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

// The scene, references, and finished prompt shared by `generate` and
// `prompt`, so the manual prompt is exactly what a backend would send.
struct Brief {
    ThemeInfo theme;
    QString scene;
    QStringList references;
    Target target;
    QString prompt;
};

std::optional<Brief> readBrief(const QCommandLineParser &parser, const Paths &paths, QString *error) {
    const QString scene = parser.value("prompt").trimmed();
    if (scene.isEmpty()) {
        *error = "Describe the scene with --prompt \"...\".";
        return std::nullopt;
    }
    const auto info = loadTheme(ThemeLibrary(paths), parser.positionalArguments().value(0), error);
    if (!info)
        return std::nullopt;
    Brief brief{*info, scene, {}, Screens::target(Screens::query()), {}};
    for (const QString &reference : parser.values("ref")) {
        const QString resolved = resolveReference(reference, *info);
        if (resolved.isEmpty()) {
            *error = QString("No file or %1 background matches \"%2\".").arg(info->name, reference);
            return std::nullopt;
        }
        brief.references << resolved;
    }
    brief.prompt = PromptBuilder::build(scene, info->colors, brief.references.size(), brief.target);
    return brief;
}

void addBriefOptions(QCommandLineParser &parser) {
    parser.addPositionalArgument("theme", "Theme slug or name; the current theme when left out.");
    parser.addOption({"prompt", "The scene to paint.", "text"});
    parser.addOption({"ref", "Style reference: a file, a background name, or its number in `easel theme`. "
                             "Repeatable.", "reference"});
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
    const auto added = Library(paths).addedBackgrounds(info->slug);
    out() << "\nBackgrounds (* added by easel):\n";
    for (int i = 0; i < info->backgrounds.size(); ++i)
        out() << (added.contains(info->backgrounds.at(i)) ? "* " : "  ") << i + 1 << ". " << info->backgrounds.at(i) << "\n";
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
    addBriefOptions(parser);
    parser.addOption({{"n", "count"}, "How many candidates, 1 to 6 (default 3).", "count", "3"});
    parser.addOption({"codex", "The codex binary to run.", "path", "codex"});
    QString error;
    if (!parse(parser, arguments, &error))
        return failure(error);
    bool validCount = false;
    const int count = parser.value("count").toInt(&validCount);
    if (!validCount || count < 1 || count > 6)
        return failure("--count must be between 1 and 6.");
    const auto brief = readBrief(parser, paths, &error);
    if (!brief)
        return failure(error);
    const auto *info = &brief->theme;
    const QStringList &references = brief->references;
    const Target &target = brief->target;
    const QString &prompt = brief->prompt;
    const QString &scene = brief->scene;

    // Without a generator, point at the manual path instead of failing blind.
    const BackendStatus codex = Backends::codex(parser.value("codex"));
    if (!codex.available)
        return failure(codex.detail + " Use `easel prompt` to get a prompt for another image platform, "
                                      "then `easel import` the result.");

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

int backends(const QStringList &arguments) {
    QCommandLineParser parser;
    QString error;
    if (!parse(parser, arguments, &error))
        return failure(error);
    const QList<BackendStatus> statuses = Backends::all();
    const QString preferred = Backends::preferred(statuses);
    if (parser.isSet("json")) {
        QJsonArray list;
        for (const BackendStatus &status : statuses)
            list.append(status.toJson());
        printJson(QJsonObject{{"preferred", preferred}, {"backends", list}});
        return 0;
    }
    for (const BackendStatus &status : statuses)
        out() << (status.name == preferred ? "* " : "  ") << status.name << (status.available ? "  ready: " : "  unavailable: ")
              << status.detail << "\n";
    out() << Qt::flush;
    return 0;
}

// The manual backend: everything needed to make the image somewhere else.
int prompt(const QStringList &arguments, const Paths &paths) {
    QCommandLineParser parser;
    addBriefOptions(parser);
    QString error;
    if (!parse(parser, arguments, &error))
        return failure(error);
    const auto brief = readBrief(parser, paths, &error);
    if (!brief)
        return failure(error);
    const QString steps = Backends::manualSteps(brief->theme.slug, brief->references.size());
    if (parser.isSet("json")) {
        printJson(QJsonObject{{"theme", brief->theme.slug},
                              {"scene", brief->scene},
                              {"prompt", brief->prompt},
                              {"references", QJsonArray::fromStringList(brief->references)},
                              {"target", brief->target.toJson()},
                              {"steps", steps}});
        return 0;
    }
    out() << brief->prompt << "\n\n";
    if (!brief->references.isEmpty()) {
        out() << "References to attach:\n";
        for (const QString &reference : brief->references)
            out() << "  " << reference << "\n";
        out() << "\n";
    }
    out() << steps << Qt::flush;
    return 0;
}

int import(const QStringList &arguments, const Paths &paths) {
    QCommandLineParser parser;
    parser.addPositionalArgument("files", "Images made elsewhere.", "<file>...");
    parser.addOption({"theme", "Theme the images are for; the current theme when left out.", "theme"});
    parser.addOption({"prompt", "The scene, used to name the background when kept.", "text"});
    // Not --platform: Qt claims that option for itself before easel sees it.
    parser.addOption({"source", "Where the images were made (ChatGPT, Gemini, ...), recorded with them.", "name"});
    QString error;
    if (!parse(parser, arguments, &error))
        return failure(error);
    const QStringList files = parser.positionalArguments();
    if (files.isEmpty())
        return failure("Give the image file(s) to import.");
    const auto info = loadTheme(ThemeLibrary(paths), parser.value("theme"), &error);
    if (!info)
        return failure(error);

    QJsonArray sources;
    for (const QString &file : files)
        sources.append(QFileInfo(file).absoluteFilePath());
    const QJsonObject batchInfo{{"scene", parser.value("prompt")},
                                {"backend", "manual"},
                                {"source", parser.value("source")},
                                {"importedFrom", sources},
                                {"theme", info->slug},
                                {"createdAt", QDateTime::currentDateTime().toString(Qt::ISODate)}};
    const QString batch = Library(paths).createBatch(info->slug, batchInfo, &error);
    if (batch.isEmpty())
        return failure(error);
    const QStringList candidates = Library::importInto(batch, files, &error);
    if (candidates.isEmpty())
        return failure(error);
    if (parser.isSet("json"))
        printJson(QJsonObject{{"theme", info->slug}, {"batch", batch}, {"candidates", QJsonArray::fromStringList(candidates)}});
    else
        for (const QString &candidate : candidates)
            out() << candidate << "\n";
    out() << Qt::flush;
    return 0;
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

// No arguments, or "open", means the window; everything else is a command.
namespace {
int remove(const QStringList &arguments, const Paths &paths) {
    QCommandLineParser parser;
    parser.addPositionalArgument("background", "A background easel added: its path, file name, or number in `easel theme`.");
    parser.addOption({"theme", "The theme it belongs to; the current theme when left out.", "theme"});
    QString error;
    if (!parse(parser, arguments, &error))
        return failure(error);
    const QString background = parser.positionalArguments().value(0);
    if (background.isEmpty())
        return failure("Give the background to remove.");
    const auto info = loadTheme(ThemeLibrary(paths), parser.value("theme"), &error);
    if (!info)
        return failure(error);
    const QString path = resolveReference(background, *info);
    if (path.isEmpty())
        return failure(QString("No %1 background matches \"%2\".").arg(info->name, background));
    if (!Library(paths).remove(path, info->slug, &error))
        return failure(error);
    if (parser.isSet("json"))
        printJson(QJsonObject{{"removed", path}, {"theme", info->slug}});
    else
        out() << "Moved " << path << " and its master to the trash.\n" << Qt::flush;
    return 0;
}
}

bool Cli::handles(const QStringList &arguments) {
    return arguments.size() >= 2 && arguments.at(1) != "open";
}

QString Cli::usage() {
    return "Usage: easel [open [theme]] | easel <command> [options]\n\n"
           "  open [theme]           Open the window (the default)\n"
           "  themes                 List installed Omarchy themes\n"
           "  theme [theme]          Show a theme's colors and backgrounds\n"
           "  screens                Show monitors and the export size\n"
           "  backends               Show which ways of making images are available\n"
           "  generate [theme] --prompt \"...\" [--ref ...] [-n 3]\n"
           "                         Generate candidate backgrounds\n"
           "  prompt [theme] --prompt \"...\" [--ref ...]\n"
           "                         Print a prompt to paste into another image platform\n"
           "  import <file>... [--theme ...] [--prompt \"...\"]\n"
           "                         Add images made elsewhere as candidates\n"
           "  keep <candidate> [--name \"...\"]\n"
           "                         Keep a candidate as a background for its theme\n"
           "  remove <background> [--theme ...]\n"
           "                         Move a background easel added to the trash\n\n"
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
    if (command == "backends")
        return backends(rest);
    if (command == "generate")
        return generate(rest, paths);
    if (command == "prompt")
        return prompt(rest, paths);
    if (command == "import")
        return import(rest, paths);
    if (command == "keep")
        return keep(rest, paths);
    if (command == "remove")
        return remove(rest, paths);
    if (command == "--version" || command == "-v") {
        out() << QCoreApplication::applicationName() << " " << QCoreApplication::applicationVersion() << "\n"
              << Qt::flush;
        return 0;
    }
    out() << usage() << Qt::flush;
    return command.isEmpty() || command == "help" || command == "--help" || command == "-h" ? 0 : 1;
}

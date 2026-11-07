#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

#include "apigenerators.h"
#include "backends.h"
#include "keystore.h"
#include "settings.h"
#include "codexgenerator.h"
#include "library.h"
#include "promptbuilder.h"
#include "screens.h"
#include "studio.h"
#include "themelibrary.h"

class OmaEaselTests : public QObject {
    Q_OBJECT

private slots:
    void namesFollowOmarchy()
    {
        QCOMPARE(ThemeLibrary::displayName("tokyo-night"), QString("Tokyo Night"));
        QCOMPARE(ThemeLibrary::slugFor("Tokyo Night"), QString("tokyo-night"));
        QCOMPARE(ThemeLibrary::slugFor("tokyo-night"), QString("tokyo-night"));
    }

    void readsColors()
    {
        QTemporaryDir dir;
        write(dir.filePath("colors.toml"), "# comment\naccent = \"#7AA2F7\"\nbackground='#1a1b26'\nmode = \"dark\"\n");
        const auto colors = ThemeLibrary::readColors(dir.filePath("colors.toml"));
        QCOMPARE(colors.value("accent"), QString("#7aa2f7"));
        QCOMPARE(colors.value("background"), QString("#1a1b26"));
        QVERIFY(!colors.contains("mode"));
    }

    void userThemeOverlaysStock()
    {
        QTemporaryDir dir;
        const Paths paths = Paths::underRoot(dir.path());
        write(paths.stockThemes + "/nord/colors.toml", "accent = \"#111111\"\n");
        write(paths.stockThemes + "/nord/backgrounds/1-fjord.png", "x");
        write(paths.stockThemes + "/nord/backgrounds/2-snow.png", "x");
        write(paths.userThemes + "/nord/colors.toml", "accent = \"#222222\"\n");
        write(paths.userThemes + "/nord/backgrounds/2-snow.png", "x");
        write(paths.userBackgrounds + "/nord/3-aurora.webp", "x");
        write(paths.userBackgrounds + "/nord/notes.txt", "x");
        write(paths.userThemes + "/mine/colors.toml", "accent = \"#333333\"\n");
        write(paths.currentThemeName, "nord\n");

        const ThemeLibrary library(paths);
        QCOMPARE(library.slugs(), QStringList({"mine", "nord"}));
        QCOMPARE(library.currentSlug(), QString("nord"));

        const auto theme = library.load("Nord");
        QVERIFY(theme);
        QCOMPARE(theme->colors.value("accent"), QString("#222222"));
        QCOMPARE(theme->backgrounds.size(), 3);
        QVERIFY(theme->backgrounds.at(0).endsWith("/backgrounds/nord/3-aurora.webp"));
        QVERIFY(theme->backgrounds.contains(paths.userThemes + "/nord/backgrounds/2-snow.png"));
        QVERIFY(!library.load("missing"));
    }

    void rotatedMonitorsSwap()
    {
        const auto monitors = Screens::parse(R"([
            {"name":"DP-2","width":2560,"height":1440,"transform":1},
            {"name":"DP-3","width":3440,"height":1440,"transform":0}])");
        QCOMPARE(monitors.size(), 2);
        QCOMPARE(monitors.at(0).width, 1440);
        QCOMPARE(monitors.at(0).height, 2560);
    }

    void targetCoversLandscapeMonitors()
    {
        const Target ultrawide = Screens::target({{"DP-3", 3440, 1440}, {"DP-2", 2560, 1440}});
        QCOMPARE(ultrawide.width, 3440);
        QCOMPARE(ultrawide.height, 1440);
        QVERIFY(ultrawide.cropped.isEmpty());

        const Target mixed = Screens::target({{"DP-3", 3440, 1440}, {"DP-2", 1440, 2560}});
        QCOMPARE(mixed.width, 3440);
        QCOMPARE(mixed.cropped, QStringList({"DP-2"}));

        const Target taller = Screens::target({{"A", 1920, 1080}, {"B", 2560, 1600}});
        QCOMPARE(taller.width, 2846);
        QCOMPARE(taller.height, 1600);

        const Target none = Screens::target({});
        QCOMPARE(none.width, 3840);
        QCOMPARE(none.height, 2160);
    }

    void shapesGroupMonitorsByAspect()
    {
        const QList<Target> mixed = Screens::shapes({{"DP-2", 1440, 2560}, {"DP-3", 3440, 1440}});
        QCOMPARE(mixed.size(), 2);
        QCOMPARE(mixed.at(0).key(), QString("21x9"));
        QCOMPARE(mixed.at(0).monitors, QStringList({"DP-3"}));
        QCOMPARE(mixed.at(0).cropped, QStringList({"DP-2"}));
        QCOMPARE(mixed.at(1).key(), QString("9x16"));
        QCOMPARE(mixed.at(1).width, 1440);
        QCOMPARE(mixed.at(1).height, 2560);
        QVERIFY(mixed.at(1).portrait());

        // Close shapes share one: 16:9 and 16:10 are both made at 16:10.
        const QList<Target> close = Screens::shapes({{"A", 1920, 1080}, {"B", 2560, 1600}});
        QCOMPARE(close.size(), 1);
        QCOMPARE(close.first().monitors.size(), 2);

        QCOMPARE(Screens::shapes({}).size(), 1);
        QCOMPARE(Target::fromJson(mixed.at(1).toJson()).key(), QString("9x16"));
    }

    void portraitPromptsAndCompanions()
    {
        const Target portrait{1440, 2560, {}};
        const QString prompt = PromptBuilder::build("harbor", {}, 0, portrait);
        QVERIFY(prompt.contains("Tall portrait composition at 9:16 (1440x2560)"));
        const QString companion = PromptBuilder::build("harbor", {}, 1, portrait, "ignored", true);
        QVERIFY(companion.contains("Paint the same scene again for a tall portrait screen"));
        QVERIFY(!companion.contains("ignored"));
    }

    void generatesForEachShapeAndKeepsVersions()
    {
        if (QStandardPaths::findExecutable("magick").isEmpty())
            QSKIP("ImageMagick is not installed.");
        QTemporaryDir dir;
        qputenv("XDG_DATA_HOME", dir.filePath("data-home").toUtf8());
        const Paths paths = Paths::underRoot(dir.path());
        write(paths.stockThemes + "/nord/colors.toml", "accent = \"#81a1c1\"\n");
        write(paths.currentThemeName, "nord\n");
        QImage image(60, 60, QImage::Format_RGB32);
        image.fill(Qt::green);
        QVERIFY(image.save(dir.filePath("fixture.png")));
        const QString codex = dir.filePath("fake-codex");
        write(codex, "#!/bin/sh\n"
                     "while [ $# -gt 0 ]; do [ \"$1\" = --output-last-message ] && out=\"$2\"; shift; done\n"
                     "dest='" + paths.codexImages.toUtf8() + "'/run-$$/image.png\n"
                     "mkdir -p \"$(dirname \"$dest\")\" && cp '" + dir.filePath("fixture.png").toUtf8() + "' \"$dest\"\n"
                     "echo \"$dest\" > \"$out\"\n");
        QVERIFY(QFile::setPermissions(codex, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

        Target wide{120, 50, {}, {"DP-3"}};
        Target tall{50, 90, {}, {"DP-2"}};
        Studio::Options options;
        options.detectBackends = false;
        options.backend = "codex";
        options.codex = codex;
        options.shapes = {wide, tall};
        Studio studio(paths, options);
        QCOMPARE(studio.displays().size(), 2);
        QCOMPARE(studio.enabledDisplays(), 2);

        // One grid per shape, each with its own prompt.
        studio.setScene("green hills");
        studio.setCount(2);
        studio.generate();
        QCOMPARE(studio.candidates().size(), 4);
        QTRY_VERIFY_WITH_TIMEOUT(!studio.busy(), 10000);
        const QVariantList sections = studio.sections();
        QCOMPARE(sections.size(), 2);
        QCOMPARE(sections.at(0).toMap().value("key").toString(), QString("21x9"));
        QCOMPARE(sections.at(1).toMap().value("indices").toList(), QVariantList({2, 3}));
        QVERIFY(studio.candidates().at(2).toMap().value("variant").toBool());
        const QString batch = QFileInfo(studio.candidates().at(0).toMap().value("path").toString()).absolutePath();
        const QJsonObject info = Library::readBatch(batch);
        QVERIFY(info.value("shapes").toArray().at(1).toObject().value("prompt").toString().contains("Tall portrait"));

        // Keeping the portrait candidate pairs it with the one main image kept.
        studio.keep(0);
        const QString main = studio.candidates().at(0).toMap().value("keptAs").toString();
        QVERIFY(main.endsWith("/nord/1-green-hills.webp"));
        studio.keep(2);
        const QString version = studio.candidates().at(2).toMap().value("keptAs").toString();
        QCOMPARE(version, paths.userBackgrounds + "/nord/9x16/1-green-hills.webp");
        QCOMPARE(QImage(version).size(), QSize(50, 90));
        QCOMPARE(studio.backgrounds().first().toMap().value("variants").toStringList(), QStringList({"9x16"}));
        // Versions stay out of the theme's own list, and so out of Omarchy's cycle.
        QCOMPARE(studio.backgrounds().size(), 1);

        // With two main images kept, the window has to ask.
        studio.keep(1);
        QSignalSpy asked(&studio, &Studio::pairNeeded);
        studio.keep(3);
        QCOMPARE(asked.size(), 1);
        studio.keepVariant(3, main);
        QCOMPARE(studio.candidates().at(3).toMap().value("state").toString(), QString("kept"));

        // A companion recomposes one candidate for the other shapes, and its
        // results pair with that candidate's background.
        studio.companion(0);
        QCOMPARE(studio.candidates().size(), 2);
        QVERIFY(studio.candidates().at(0).toMap().value("variant").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!studio.busy(), 10000);
        const QString companionBatch = QFileInfo(studio.candidates().at(0).toMap().value("path").toString()).absolutePath();
        QVERIFY(Library::readBatch(companionBatch).value("prompt").toString().contains("Paint the same scene again"));
        studio.keep(0);
        QCOMPARE(studio.candidates().at(0).toMap().value("keptAs").toString(), version);

        // Only one shape on: generating makes just that grid. The last shape
        // can't be turned off.
        studio.toggleDisplay("9x16");
        QCOMPARE(studio.enabledDisplays(), 1);
        studio.toggleDisplay("21x9");
        QCOMPARE(studio.enabledDisplays(), 1);

        // Removing a background takes its versions with it.
        studio.removeBackground(main);
        QVERIFY(!QFileInfo::exists(version));
        qunsetenv("XDG_DATA_HOME");
    }

    void promptCarriesSceneColorsAndShape()
    {
        const Target target = Screens::target({{"DP-3", 3440, 1440}});
        const QString prompt = PromptBuilder::build("  misty harbor at dawn ",
                                                    {{"background", "#0b0701"}, {"accent", "#eac018"}}, 2, target);
        QVERIFY(prompt.contains("Scene: misty harbor at dawn"));
        QVERIFY(prompt.contains("background #0b0701, accent #eac018"));
        QVERIFY(prompt.contains("The 2 attached images are style references"));
        QVERIFY(prompt.contains("21:9 (3440x1440)"));
        QVERIFY(!PromptBuilder::build("x", {}, 0, target).contains("reference"));
    }

    void codexArgumentsKeepThePromptLast()
    {
        const QStringList arguments = CodexGenerator::arguments({"paint", {"/a.png", "/b.png"}, "/out.png"},
                                                                "/work", "/work/last.txt");
        QCOMPARE(arguments.first(), QString("exec"));
        QCOMPARE(arguments.count("--image"), 2);
        QCOMPARE(arguments.at(arguments.size() - 2), QString("--"));
        QVERIFY(arguments.last().endsWith("paint"));
    }

    void findsCodexImagePath()
    {
        const QString root = "/home/u/.codex/generated_images";
        QCOMPARE(CodexGenerator::findImagePath(root + "/abc/exec-1.png\n", root), root + "/abc/exec-1.png");
        QCOMPARE(CodexGenerator::findImagePath("Saved to `" + root + "/abc/exec-2.png`.", root),
                 root + "/abc/exec-2.png");
        QCOMPARE(CodexGenerator::findImagePath("/tmp/generated_images/x.png", root), QString());
        QCOMPARE(CodexGenerator::findImagePath("no image", root), QString());
    }

    void namesAndNumbers()
    {
        QCOMPARE(Library::slugify("Misty harbor at dawn, fishing boats!"), QString("misty-harbor-dawn-fishing-boats"));
        QCOMPARE(Library::slugify("!!!"), QString("background"));
        QCOMPARE(Library::slugify("a quiet mountain lake at dusk with a boat"), QString("quiet-mountain-lake-dusk-boat"));
        QCOMPARE(Library::slugify("the end"), QString("end"));
        QCOMPARE(Library::slugify("of the"), QString("of-the"));
        QCOMPARE(Library::nextNumber({"1-a.webp", "/x/3-b.png", "c.jpg"}), 4);
        QCOMPARE(Library::nextNumber({"a.jpg", "b.jpg"}), 3);
        QCOMPARE(Library::nextNumber({}), 1);
    }

    void detectsCodexLogin()
    {
        QTemporaryDir dir;
        const QString loggedIn = dir.filePath("codex-in"), loggedOut = dir.filePath("codex-out");
        write(loggedIn, "#!/bin/sh\necho 'Logged in using ChatGPT'\n");
        write(loggedOut, "#!/bin/sh\necho 'Not logged in'\nexit 1\n");
        for (const QString &script : {loggedIn, loggedOut})
            QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

        QVERIFY(Backends::codex(loggedIn).available);
        QCOMPARE(Backends::codex(loggedIn).detail, QString("Logged in using ChatGPT"));
        QVERIFY(!Backends::codex(loggedOut).available);
        QVERIFY(!Backends::codex(dir.filePath("missing")).available);
    }

    void fallsBackToManual()
    {
        QCOMPARE(Backends::preferred({{"codex", false, ""}, {"openai", false, ""}, Backends::manual()}), QString("manual"));
        QCOMPARE(Backends::preferred({{"codex", true, ""}, Backends::manual()}), QString("codex"));
        const QString steps = Backends::manualSteps("nord", 2);
        QVERIFY(steps.contains("Attach the 2 reference image(s)"));
        QVERIFY(steps.contains("oma-easel import <file> --theme nord"));
        QVERIFY(!Backends::manualSteps("nord", 0).contains("Attach"));
    }

    void importsImagesAsCandidates()
    {
        QTemporaryDir dir;
        const Paths paths = Paths::underRoot(dir.path());
        QImage image(64, 32, QImage::Format_RGB32);
        image.fill(Qt::darkGreen);
        QVERIFY(image.save(dir.filePath("download.jpg")));
        write(dir.filePath("notes.txt"), "not an image");

        QString error;
        const QString batch = Library(paths).createBatch("nord", {{"backend", "manual"}}, &error);
        const QStringList imported = Library::importInto(batch, {dir.filePath("download.jpg")}, &error);
        QCOMPARE(imported, QStringList({Library::candidatePath(batch, 0)}));
        QCOMPARE(QImage(imported.first()).size(), QSize(64, 32));
        QVERIFY(QFileInfo::exists(dir.filePath("download.jpg")));

        // A second import continues the numbering; a non-image is refused.
        QCOMPARE(Library::importInto(batch, {dir.filePath("download.jpg")}, &error).first(), Library::candidatePath(batch, 1));
        QVERIFY(Library::importInto(batch, {dir.filePath("notes.txt")}, &error).isEmpty());
        QVERIFY(error.contains("notes.txt"));
    }

    void studioManualFlow()
    {
        if (QStandardPaths::findExecutable("magick").isEmpty())
            QSKIP("ImageMagick is not installed.");
        QTemporaryDir dir;
        const Paths paths = Paths::underRoot(dir.path());
        write(paths.stockThemes + "/nord/colors.toml", "background = \"#2e3440\"\naccent = \"#81a1c1\"\n");
        write(paths.stockThemes + "/nord/backgrounds/1-fjord.png", "x");
        write(paths.currentThemeName, "nord\n");
        Studio::Options options;
        options.detectBackends = false;  // manual, without asking Codex
        options.target = Target{120, 50, {}};
        Studio studio(paths, options);
        QCOMPARE(studio.themeSlug(), QString("nord"));
        QVERIFY(studio.manual());

        // An empty scene is refused; a real one yields the prompt and steps.
        studio.generate();
        QVERIFY(studio.manualPrompt().isEmpty());
        studio.setScene("Frozen fjord");
        studio.toggleReference(paths.stockThemes + "/nord/backgrounds/1-fjord.png");
        studio.generate();
        QVERIFY(studio.manualPrompt().contains("Scene: Frozen fjord"));
        QVERIFY(studio.manualPrompt().contains("The attached image is a style reference"));
        QVERIFY(studio.manualSteps().contains("Attach the 1 reference"));

        // Import, keep, then the kept image shows among the theme's backgrounds.
        QImage image(240, 100, QImage::Format_RGB32);
        image.fill(Qt::darkCyan);
        QVERIFY(image.save(dir.filePath("made-elsewhere.png")));
        QVERIFY(image.save(dir.filePath("second.png")));
        studio.importFiles({dir.filePath("made-elsewhere.png"), dir.filePath("second.png")});
        QCOMPARE(studio.candidates().size(), 2);
        QCOMPARE(studio.unkept(), 2);
        studio.keep(0);
        QCOMPARE(studio.candidates().at(0).toMap().value("state").toString(), QString("kept"));
        QCOMPARE(studio.unkept(), 1);
        QCOMPARE(studio.backgrounds().size(), 2);
        QVERIFY(QFileInfo::exists(paths.userBackgrounds + "/nord/2-frozen-fjord.webp"));

        // Discard drops the candidate; vary makes the kept one the only reference.
        studio.discard(1);
        QCOMPARE(studio.candidates().size(), 1);
        studio.vary(0);
        QCOMPARE(studio.references().size(), 1);
        QVERIFY(studio.references().first().endsWith("candidate-1.png"));
    }

    void emptyWorkspaceSkipsBusyOnes()
    {
        QCOMPARE(PreviewSession::emptyWorkspace(R"([{"id":1,"windows":2},{"id":2,"windows":0},{"id":3,"windows":1}])"), 2);
        QCOMPARE(PreviewSession::emptyWorkspace(R"([{"id":1,"windows":1},{"id":2,"windows":1},{"id":-98,"windows":1}])"), 3);
        QCOMPARE(PreviewSession::emptyWorkspace("[]"), 1);
    }

    void previewKeepsOrRestores_data()
    {
        QTest::addColumn<QByteArray>("answer");
        QTest::addColumn<bool>("keep");
        QTest::newRow("keep") << QByteArray("k") << true;
        QTest::newRow("exit") << QByteArray("x") << false;
    }

    void previewKeepsOrRestores()
    {
        if (QStandardPaths::findExecutable("magick").isEmpty())
            QSKIP("ImageMagick is not installed.");
        QFETCH(QByteArray, answer);
        QFETCH(bool, keep);
        QTemporaryDir dir;
        const Paths paths = Paths::underRoot(dir.path());
        write(paths.stockThemes + "/nord/colors.toml", "accent = \"#81a1c1\"\n");
        write(paths.currentThemeName, "nord\n");
        write(dir.filePath("previous.png"), "x");
        QVERIFY(QFile::link(dir.filePath("previous.png"), dir.filePath("background")));

        // Fakes for everything that would touch the desktop: the background
        // setter and hyprctl log their calls, and the "terminal" runs the
        // prompt script without gum and answers it.
        const QString setLog = dir.filePath("set.log"), hyprLog = dir.filePath("hypr.log");
        auto script = [&](const QString &name, const QByteArray &body) {
            write(dir.filePath(name), "#!/bin/sh\n" + body);
            QVERIFY(QFile::setPermissions(dir.filePath(name), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        };
        script("bg-set", "echo \"$1\" >> '" + setLog.toUtf8() + "'\n");
        script("hyprctl", "echo \"$*\" >> '" + hyprLog.toUtf8() + "'\necho '[]'\n");
        script("terminal", "echo " + answer + " | PATH=/nonexistent /bin/bash \"$5\"\n");

        Studio::Options options;
        options.detectBackends = false;
        options.target = Target{120, 50, {}};
        options.preview = {dir.filePath("hyprctl"), dir.filePath("bg-set"), dir.filePath("terminal"), dir.filePath("background")};
        Studio studio(paths, options);
        QImage image(60, 25, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(dir.filePath("candidate.png")));
        studio.setScene("red sky");
        studio.importFiles({dir.filePath("candidate.png")});
        studio.preview(0);
        QCOMPARE(studio.previewing(), 0);
        QTRY_COMPARE(studio.previewing(), -1);

        auto lines = [](const QString &path) {
            QFile file(path);
            return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed().split('\n') : QStringList();
        };
        QTRY_COMPARE(lines(setLog).size(), 2);
        const QStringList sets = lines(setLog);
        QVERIFY(sets.at(0).endsWith("candidate-1.png"));
        if (keep) {
            QCOMPARE(studio.candidates().at(0).toMap().value("state").toString(), QString("kept"));
            QVERIFY(sets.at(1).endsWith("1-red-sky.webp"));
        } else {
            QCOMPARE(studio.candidates().at(0).toMap().value("state").toString(), QString("ready"));
            QCOMPARE(sets.at(1), dir.filePath("previous.png"));
        }
        QVERIFY(lines(hyprLog).join('\n').contains("hl.dsp.focus({ workspace = \"1\" })"));
    }

    void historyRemembersBatchesAndPrompts()
    {
        QTemporaryDir dir;
        const Paths paths = Paths::underRoot(dir.path());
        write(paths.stockThemes + "/nord/colors.toml", "accent = \"#81a1c1\"\n");
        write(paths.currentThemeName, "nord\n");
        Studio::Options options;
        options.detectBackends = false;
        options.target = Target{120, 50, {}};
        Studio studio(paths, options);
        QImage image(60, 25, QImage::Format_RGB32);
        image.fill(Qt::blue);
        QVERIFY(image.save(dir.filePath("a.png")));

        studio.setScene("blue lake");
        studio.importFiles({dir.filePath("a.png")});
        QCOMPARE(studio.history().size(), 1);
        const QVariantMap batch = studio.history().first().toMap();
        QCOMPARE(batch.value("scene").toString(), QString("blue lake"));
        const QString candidate = batch.value("candidates").toList().first().toMap().value("path").toString();

        // A candidate can be a reference, and reopening the batch restores its scene.
        studio.toggleReference(candidate);
        QCOMPARE(studio.extraReferences().size(), 1);
        studio.setScene("something else");
        studio.openBatch(batch.value("batch").toString());
        QCOMPARE(studio.scene(), QString("blue lake"));
        QCOMPARE(studio.batchScene(), QString("blue lake"));
        QVERIFY(studio.candidates().first().toMap().value("reference").toBool());
    }

    void variationPromptKeepsTheCandidate()
    {
        const Target target{3440, 1440, {}};
        const QString varied = PromptBuilder::build("lake", {}, 1, target, "warmer light");
        QVERIFY(varied.contains("starting point"));
        QVERIFY(varied.contains("change only this: warmer light."));
        QVERIFY(!varied.contains("style reference only"));
        QVERIFY(PromptBuilder::build("lake", {}, 1, target).contains("style reference only"));
    }

    void generationOutlivesThemeSwitch()
    {
        QTemporaryDir dir;
        const Paths paths = Paths::underRoot(dir.path());
        write(paths.stockThemes + "/nord/colors.toml", "accent = \"#81a1c1\"\n");
        write(paths.stockThemes + "/gruvbox/colors.toml", "accent = \"#d79921\"\n");
        write(paths.currentThemeName, "nord\n");
        QImage image(60, 25, QImage::Format_RGB32);
        image.fill(Qt::green);
        QVERIFY(image.save(dir.filePath("fixture.png")));

        // A fake codex: waits a moment, "generates" by copying a fixture into
        // the image folder, and replies with the path like the real one.
        const QString codex = dir.filePath("fake-codex");
        write(codex, "#!/bin/sh\n"
                     "while [ $# -gt 0 ]; do [ \"$1\" = --output-last-message ] && out=\"$2\"; shift; done\n"
                     "sleep 1\n"
                     "dest='" + paths.codexImages.toUtf8() + "'/run-$$/image.png\n"
                     "mkdir -p \"$(dirname \"$dest\")\" && cp '" + dir.filePath("fixture.png").toUtf8() + "' \"$dest\"\n"
                     "echo \"$dest\" > \"$out\"\n");
        QVERIFY(QFile::setPermissions(codex, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

        Studio::Options options;
        options.detectBackends = false;
        options.backend = "codex";
        options.codex = codex;
        options.target = Target{120, 50, {}};
        Studio studio(paths, options);
        studio.setScene("green hills");
        studio.setCount(2);
        studio.generate();
        QVERIFY(studio.busy());
        QCOMPARE(studio.activity().size(), 1);
        // The status file the companion plugin reads says what's running.
        {
            QFile status(paths.status);
            QVERIFY(status.open(QIODevice::ReadOnly));
            const QJsonObject json = QJsonDocument::fromJson(status.readAll()).object();
            QCOMPARE(json.value("generating").toInt(), 2);
            QCOMPARE(json.value("batches").toArray().first().toObject().value("theme").toString(), QString("nord"));
        }
        // The field clears once the run starts; the batch keeps the scene.
        QVERIFY(studio.scene().isEmpty());
        QCOMPARE(studio.batchScene(), QString("green hills"));

        // Switching away leaves the jobs running; they still land in nord's history.
        studio.loadTheme("gruvbox");
        QVERIFY(studio.anyRunning());
        QVERIFY(studio.candidates().isEmpty());
        QCOMPARE(studio.activity().first().toMap().value("theme").toString(), QString("nord"));
        QTRY_VERIFY_WITH_TIMEOUT(!studio.anyRunning(), 10000);

        studio.loadTheme("nord");
        QCOMPARE(studio.history().size(), 1);
        QCOMPARE(studio.history().first().toMap().value("candidates").toList().size(), 2);
        QCOMPARE(studio.history().first().toMap().value("scene").toString(), QString("green hills"));
    }

    void removesOnlyWhatEaselAdded()
    {
        if (QStandardPaths::findExecutable("magick").isEmpty())
            QSKIP("ImageMagick is not installed.");
        QTemporaryDir dir;
        const Paths paths = Paths::underRoot(dir.path());
        write(paths.stockThemes + "/nord/colors.toml", "accent = \"#81a1c1\"\n");
        write(paths.stockThemes + "/nord/backgrounds/1-fjord.png", "x");
        write(paths.currentThemeName, "nord\n");
        write(dir.filePath("previous.png"), "x");
        const QString setLog = dir.filePath("set.log");
        write(dir.filePath("bg-set"), "#!/bin/sh\necho \"$1\" >> '" + setLog.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(dir.filePath("bg-set"), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

        Studio::Options options;
        options.detectBackends = false;
        options.target = Target{120, 50, {}};
        options.preview.setBackground = dir.filePath("bg-set");
        options.preview.currentBackgroundLink = dir.filePath("background");
        Studio studio(paths, options);
        QImage image(60, 25, QImage::Format_RGB32);
        image.fill(Qt::yellow);
        QVERIFY(image.save(dir.filePath("c.png")));
        studio.setScene("sunny field");
        studio.importFiles({dir.filePath("c.png")});
        studio.keep(0);
        const QString exported = studio.candidates().first().toMap().value("keptAs").toString();
        QVERIFY(QFileInfo::exists(exported));
        QVERIFY(QFile::link(exported, dir.filePath("background")));

        // The stock background can't be removed; the added one can.
        studio.removeBackground(paths.stockThemes + "/nord/backgrounds/1-fjord.png");
        QVERIFY(QFileInfo::exists(paths.stockThemes + "/nord/backgrounds/1-fjord.png"));
        QVERIFY(studio.message().contains("wasn't added by Oma Easel"));
        QVERIFY(studio.backgrounds().first().toMap().value("added").toBool());

        studio.removeBackground(exported);
        QVERIFY(!QFileInfo::exists(exported));
        QVERIFY(QDir(paths.data + "/nord").entryList(QDir::Files).isEmpty());
        QCOMPARE(studio.backgrounds().size(), 1);
        QCOMPARE(studio.candidates().first().toMap().value("state").toString(), QString("ready"));
        // It was on the desktop, so the desktop moved to the remaining background.
        QTRY_VERIFY(QFileInfo(setLog).size() > 0);
        QFile log(setLog);
        QVERIFY(log.open(QIODevice::ReadOnly));
        QVERIFY(QString::fromUtf8(log.readAll()).trimmed().endsWith("1-fjord.png"));
    }

    void openaiSizesFollowTheRules()
    {
        QCOMPARE(OpenAIGenerator::sizeFor(Target{3440, 1440, {}}), QSize(3440, 1440));
        // Too wide, too big, too small: each is brought inside the limits.
        const QSize wide = OpenAIGenerator::sizeFor(Target{5120, 1440, {}});
        QVERIFY(double(wide.width()) / wide.height() <= 3.0);
        QVERIFY(wide.width() % 16 == 0 && wide.height() % 16 == 0);
        const QSize big = OpenAIGenerator::sizeFor(Target{7680, 4320, {}});
        QVERIFY(big.width() <= 3840 && big.width() * big.height() <= 8294400);
        const QSize small = OpenAIGenerator::sizeFor(Target{640, 360, {}});
        QVERIFY(small.width() * small.height() >= 655360 - 2 * 16 * 1200);
    }

    void apiResponsesDecode()
    {
        QImage image(8, 4, QImage::Format_RGB32);
        image.fill(Qt::magenta);
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        const QString b64 = QString::fromLatin1(png.toBase64());

        QString error;
        const QJsonObject openai{{"data", QJsonArray{QJsonObject{{"b64_json", b64}}}}};
        QCOMPARE(QImage::fromData(OpenAIGenerator::decode(openai, &error)).size(), QSize(8, 4));
        const QJsonObject gemini{{"candidates", QJsonArray{QJsonObject{{"content", QJsonObject{{"parts", QJsonArray{
            QJsonObject{{"text", "here you go"}}, QJsonObject{{"inlineData", QJsonObject{{"mimeType", "image/png"}, {"data", b64}}}}}}}}}}}};
        QCOMPARE(QImage::fromData(GeminiGenerator::decode(gemini, &error)).size(), QSize(8, 4));
        QVERIFY(GeminiGenerator::decode(QJsonObject{{"promptFeedback", QJsonObject{{"blockReason", "SAFETY"}}}}, &error).isEmpty());
        QVERIFY(error.contains("SAFETY"));
    }

    void geminiRequestAsksForImageOnly()
    {
        QTemporaryDir dir;
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::black);
        QVERIFY(image.save(dir.filePath("ref.png")));
        const QJsonObject body = GeminiGenerator::body("a lake", {dir.filePath("ref.png")}, Target{3440, 1440, {}});
        const QJsonObject config = body.value("generationConfig").toObject();
        QCOMPARE(config.value("responseModalities").toArray(), QJsonArray{"IMAGE"});
        QCOMPARE(config.value("imageConfig").toObject().value("aspectRatio").toString(), QString("21:9"));
        const QJsonArray parts = body.value("contents").toArray().first().toObject().value("parts").toArray();
        QCOMPARE(parts.size(), 2);
        QCOMPARE(parts.at(1).toObject().value("inline_data").toObject().value("mime_type").toString(), QString("image/png"));
    }

    void settingsPersist()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("oma-easel/settings.json");
        {
            Settings settings(file);
            QCOMPARE(settings.openaiModel(), QString(Settings::defaultOpenaiModel));
            settings.setBackend("gemini");
            settings.setGeminiModel("  some-image-model ");
            settings.setExportFormat("jpeg");
            settings.setExportQuality(130);
            settings.setExtraExportFolder("nord", dir.filePath("repo/backgrounds"));
            settings.setExportFormat("gif");  // not a choice; ignored
        }
        Settings again(file);
        QCOMPARE(again.backend(), QString("gemini"));
        QCOMPARE(again.geminiModel(), QString("some-image-model"));
        QCOMPARE(again.exportFormat(), QString("jpeg"));
        QCOMPARE(again.exportQuality(), 100);
        QCOMPARE(again.extraExportFolder("nord"), dir.filePath("repo/backgrounds"));
        again.setGeminiModel("");
        QCOMPARE(again.geminiModel(), QString(Settings::defaultGeminiModel));
    }

    void keysComeFromEnvironmentOrKeyring()
    {
        QTemporaryDir dir;
        // A fake secret-tool that keeps one secret in a file.
        const QString store = dir.filePath("secret");
        write(dir.filePath("secret-tool"),
              "#!/bin/sh\ncase \"$1\" in\n  store) cat > '" + store.toUtf8() + "' ;;\n"
              "  lookup) [ -f '" + store.toUtf8() + "' ] && cat '" + store.toUtf8() + "' || exit 1 ;;\n"
              "  clear) rm -f '" + store.toUtf8() + "' ;;\nesac\n");
        QVERIFY(QFile::setPermissions(dir.filePath("secret-tool"), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        qputenv("OMA_EASEL_SECRET_TOOL", dir.filePath("secret-tool").toUtf8());
        const QByteArray previous = qgetenv("GEMINI_API_KEY");
        qunsetenv("GEMINI_API_KEY");

        QVERIFY(!Backends::gemini().available);
        QString error;
        QVERIFY(KeyStore::set("gemini", "  test-key \n", &error));
        QCOMPARE(KeyStore::get("gemini"), QString("test-key"));
        QVERIFY(Backends::gemini().available);
        qputenv("GEMINI_API_KEY", "from-env");
        QCOMPARE(KeyStore::get("gemini"), QString("from-env"));
        QVERIFY(Backends::gemini().detail.contains("GEMINI_API_KEY"));
        qunsetenv("GEMINI_API_KEY");
        QVERIFY(KeyStore::clear("gemini", &error));
        QVERIFY(KeyStore::get("gemini").isEmpty());

        if (!previous.isEmpty())
            qputenv("GEMINI_API_KEY", previous);
        qunsetenv("OMA_EASEL_SECRET_TOOL");
    }

    void exportsJpegAndCopiesToExtraFolder()
    {
        if (QStandardPaths::findExecutable("magick").isEmpty())
            QSKIP("ImageMagick is not installed.");
        QTemporaryDir dir;
        const Paths paths = Paths::underRoot(dir.path());
        write(paths.stockThemes + "/nord/colors.toml", "accent = \"#111111\"\n");
        const auto theme = ThemeLibrary(paths).load("nord");
        const Library library(paths);
        QString error;
        const QString batch = library.createBatch("nord", {{"scene", "snow"}, {"theme", "nord"}}, &error);
        QImage image(240, 100, QImage::Format_RGB32);
        image.fill(Qt::white);
        QVERIFY(image.save(Library::candidatePath(batch, 0)));

        const ExportOptions options{"jpeg", 90, dir.filePath("repo/backgrounds")};
        const auto kept = library.keep(Library::candidatePath(batch, 0), *theme, Target{120, 50, {}}, {}, &error, options);
        QVERIFY2(kept, qPrintable(error));
        QVERIFY(kept->exported.endsWith("1-snow.jpg"));
        QCOMPARE(QImageReader(kept->exported).format(), QByteArray("jpeg"));
        QCOMPARE(kept->extraCopy, dir.filePath("repo/backgrounds/1-snow.jpg"));
        QVERIFY(QFileInfo::exists(kept->extraCopy));
    }

    void keepStoresMasterAndExport()
    {
        if (QStandardPaths::findExecutable("magick").isEmpty())
            QSKIP("ImageMagick is not installed.");
        QTemporaryDir dir;
        const Paths paths = Paths::underRoot(dir.path());
        write(paths.stockThemes + "/nord/colors.toml", "accent = \"#111111\"\n");
        write(paths.stockThemes + "/nord/backgrounds/1-fjord.png", "x");
        const auto theme = ThemeLibrary(paths).load("nord");
        QVERIFY(theme);

        const Library library(paths);
        QString error;
        const QString batch = library.createBatch("nord", {{"scene", "Quiet fjord at night"}, {"theme", "nord"}}, &error);
        QVERIFY2(!batch.isEmpty(), qPrintable(error));
        QImage image(240, 100, QImage::Format_RGB32);
        image.fill(QColor("#334455"));
        QVERIFY(image.save(Library::candidatePath(batch, 0)));

        const Target target{120, 50, {}};
        const auto kept = library.keep(Library::candidatePath(batch, 0), *theme, target, {}, &error);
        QVERIFY2(kept, qPrintable(error));
        QCOMPARE(QFileInfo(kept->master).fileName(), QString("2-quiet-fjord-night.png"));
        QCOMPARE(QFileInfo(kept->exported).fileName(), QString("2-quiet-fjord-night.webp"));
        QCOMPARE(QFileInfo(kept->exported).absolutePath(), paths.userBackgrounds + "/nord");
        QCOMPARE(QImage(kept->exported).size(), QSize(120, 50));
        QVERIFY(QFileInfo(kept->sidecar).isFile());
        QVERIFY(!QFileInfo::exists(kept->exported + ".partial"));

        // A second keep of the same candidate takes the next number.
        const auto again = library.keep(Library::candidatePath(batch, 0), *ThemeLibrary(paths).load("nord"),
                                        target, "second", &error);
        QVERIFY2(again, qPrintable(error));
        QCOMPARE(QFileInfo(again->master).fileName(), QString("3-second.png"));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(OmaEaselTests)
#include "oma_easel_tests.moc"

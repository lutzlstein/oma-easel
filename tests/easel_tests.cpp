#include <QDir>
#include <QFile>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "backends.h"
#include "codexgenerator.h"
#include "library.h"
#include "promptbuilder.h"
#include "screens.h"
#include "themelibrary.h"

class EaselTests : public QObject {
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
        QVERIFY(steps.contains("easel import <file> --theme nord"));
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

QTEST_MAIN(EaselTests)
#include "easel_tests.moc"

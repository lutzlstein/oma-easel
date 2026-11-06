// Oma Easel: generates new background images for an installed Omarchy theme.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

#include "cli.h"
#include "paths.h"
#include "studio.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);

    // Commands draw nothing, so they run without a display; QGuiApplication
    // is still needed because images are decoded with QImage.
    const bool command = Cli::handles(arguments);
    if (command)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    Paths::migrateLegacy();
    // The working name lives here (and in the binary, package, and desktop
    // file names); the window reads it from Qt.application.displayName.
    app.setApplicationName("oma-easel");
    app.setApplicationDisplayName("Oma Easel");
    app.setApplicationVersion("0.1.0");

    if (command)
        return Cli::run(arguments, Paths::defaults());

    // Matches the window to oma-easel.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("oma-easel");
    app.setWindowIcon(QIcon::fromTheme("oma-easel"));
    // Basic is the plainest base; every control Oma Easel shows is restyled from
    // the shell's tokens in src/ui/.
    QQuickStyle::setStyle("Basic");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Studio studio(Paths::defaults());
    // "oma-easel open nord" starts on that theme instead of the current one.
    if (arguments.size() >= 3)
        studio.loadTheme(arguments.at(2));

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("studio", &studio);
    // The footer's attribution links here.
    // OMA_EASEL_SCREENSHOT_OPEN names an overlay to open for the screenshot.
    engine.rootContext()->setContextProperty("screenshotOpen", qEnvironmentVariable("OMA_EASEL_SCREENSHOT_OPEN"));
    engine.rootContext()->setContextProperty("appAuthor", QStringLiteral("lutzlstein"));
    engine.rootContext()->setContextProperty("appRepository", QStringLiteral("https://github.com/lutzlstein/oma-easel"));
    engine.rootContext()->setContextProperty("appSupport", QStringLiteral("https://ko-fi.com/lutzlstein"));
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    // For READMEs and checking the look without touching the desktop: with
    // OMA_EASEL_SCREENSHOT=<file> (and QT_QPA_PLATFORM=offscreen), save the
    // window once it has drawn and quit.
    const QString screenshot = qEnvironmentVariable("OMA_EASEL_SCREENSHOT");
    if (!screenshot.isEmpty())
        QTimer::singleShot(2500, &app, [&] {
            if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()))
                window->grabWindow().save(screenshot);
            app.quit();
        });

    return app.exec();
}

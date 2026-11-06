// easel: generates new background images for an installed Omarchy theme.

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
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
    app.setApplicationName("easel");
    app.setApplicationVersion("0.1.0");

    if (command)
        return Cli::run(arguments, Paths::defaults());

    // Matches the window to easel.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("easel");
    app.setWindowIcon(QIcon::fromTheme("easel"));
    // Basic is the plainest base; every control easel shows is restyled from
    // the shell's tokens in src/ui/.
    QQuickStyle::setStyle("Basic");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Studio studio(Paths::defaults());
    // "easel open nord" starts on that theme instead of the current one.
    if (arguments.size() >= 3)
        studio.loadTheme(arguments.at(2));

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("studio", &studio);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}

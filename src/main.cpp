// easel: generates new background images for an installed Omarchy theme.

#include <QGuiApplication>
#include <QTextStream>

#include "cli.h"
#include "paths.h"

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);

    // Commands draw nothing, so they run without a display; QGuiApplication
    // is still needed because candidates are decoded with QImage.
    const bool command = Cli::handles(arguments) || arguments.value(1) != "open";
    if (command)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("easel");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("easel");

    if (command)
        return Cli::run(arguments, Paths::defaults());

    // The window arrives in phase 2.
    QTextStream(stderr) << "easel: the window isn't built yet. Use the commands; see `easel help`." << Qt::endl;
    return 1;
}

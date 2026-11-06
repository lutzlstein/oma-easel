#include "screens.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <cmath>

QList<Monitor> Screens::parse(const QByteArray &hyprctlJson) {
    QList<Monitor> monitors;
    for (const QJsonValue &value : QJsonDocument::fromJson(hyprctlJson).array()) {
        const QJsonObject object = value.toObject();
        Monitor monitor{object.value("name").toString(), object.value("width").toInt(),
                        object.value("height").toInt()};
        // Hyprland reports the mode's size; transforms 1, 3, 5 and 7 rotate it
        // a quarter turn, so the wallpaper the monitor shows is the other way up.
        if (object.value("transform").toInt() % 2 == 1)
            std::swap(monitor.width, monitor.height);
        if (monitor.width > 0 && monitor.height > 0)
            monitors.append(monitor);
    }
    return monitors;
}

QList<Monitor> Screens::query(const QString &hyprctl) {
    QProcess process;
    process.start(hyprctl, {"monitors", "-j"});
    if (!process.waitForFinished(3000) || process.exitCode() != 0)
        return {};
    return parse(process.readAllStandardOutput());
}

Target Screens::target(const QList<Monitor> &monitors) {
    QList<Monitor> landscape;
    Target target;
    for (const Monitor &monitor : monitors) {
        if (monitor.width >= monitor.height)
            landscape.append(monitor);
        else
            target.cropped.append(monitor.name);
    }
    if (landscape.isEmpty())
        return target;

    double aspect = 0;
    for (const Monitor &monitor : landscape)
        aspect = std::max(aspect, double(monitor.width) / monitor.height);

    // Wide enough for the widest screen and tall enough for the tallest once
    // scaled to fill, rounded to even pixels so encoders never pad.
    int width = 0;
    for (const Monitor &monitor : landscape)
        width = std::max({width, monitor.width, int(std::ceil(monitor.height * aspect))});
    auto even = [](double value) { return int(std::lround(value / 2.0)) * 2; };
    target.width = even(width);
    target.height = even(width / aspect);
    return target;
}

QJsonObject Target::toJson() const {
    return {{"width", width},
            {"height", height},
            {"aspect", std::round(aspect() * 100) / 100},
            {"cropped", QJsonArray::fromStringList(cropped)}};
}

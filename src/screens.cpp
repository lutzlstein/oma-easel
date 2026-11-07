#include "screens.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <algorithm>
#include <cmath>

QList<Monitor> Screens::parse(const QByteArray &hyprctlJson) {
    QList<Monitor> monitors;
    for (const QJsonValue &value : QJsonDocument::fromJson(hyprctlJson).array()) {
        const QJsonObject object = value.toObject();
        Monitor monitor{object.value("name").toString(), object.value("width").toInt(),
                        object.value("height").toInt(), object.value("x").toInt(), object.value("y").toInt(),
                        object.value("scale").toDouble(1)};
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

namespace {
// The fill-without-upscaling size for a group of monitors: wide enough for
// the widest and tall enough for the tallest once scaled, rounded to even
// pixels so encoders never pad.
Target fit(const QList<Monitor> &group) {
    Target target;
    double aspect = 0;
    for (const Monitor &monitor : group)
        aspect = std::max(aspect, double(monitor.width) / monitor.height);
    int width = 0;
    for (const Monitor &monitor : group) {
        width = std::max({width, monitor.width, int(std::ceil(monitor.height * aspect))});
        target.monitors << monitor.name;
    }
    auto even = [](double value) { return int(std::lround(value / 2.0)) * 2; };
    target.width = even(width);
    target.height = even(width / aspect);
    return target;
}
}

QList<Target> Screens::shapes(const QList<Monitor> &monitors) {
    QList<Monitor> sorted = monitors;
    std::stable_sort(sorted.begin(), sorted.end(), [](const Monitor &a, const Monitor &b) {
        return qint64(a.width) * a.height > qint64(b.width) * b.height;
    });
    QList<QList<Monitor>> groups;
    for (const Monitor &monitor : sorted) {
        const double aspect = double(monitor.width) / monitor.height;
        bool placed = false;
        for (QList<Monitor> &group : groups) {
            const double first = double(group.first().width) / group.first().height;
            if (std::abs(aspect - first) / first < 0.15) {
                group.append(monitor);
                placed = true;
                break;
            }
        }
        if (!placed)
            groups.append({monitor});
    }
    QList<Target> shapes;
    QStringList used;
    for (const QList<Monitor> &group : groups) {
        Target shape = fit(group);
        // Two groups that round to the same familiar ratio would share a
        // folder, so the smaller one joins the larger instead.
        if (used.contains(shape.key())) {
            for (Target &existing : shapes)
                if (existing.key() == shape.key())
                    existing.monitors << shape.monitors;
            continue;
        }
        used << shape.key();
        shapes << shape;
    }
    if (shapes.isEmpty())
        shapes << Target();
    for (int i = 1; i < shapes.size(); ++i)
        shapes.first().cropped << shapes.at(i).monitors;
    return shapes;
}

QString Screens::ratioLabel(double aspect) {
    struct Ratio { const char *label; double value; };
    static const Ratio ratios[] = {{"32:9", 32.0 / 9}, {"21:9", 21.0 / 9}, {"16:9", 16.0 / 9}, {"16:10", 16.0 / 10},
                                   {"3:2", 3.0 / 2},   {"4:3", 4.0 / 3},   {"5:4", 5.0 / 4},   {"1:1", 1.0},
                                   {"4:5", 4.0 / 5},   {"3:4", 3.0 / 4},   {"2:3", 2.0 / 3},   {"10:16", 10.0 / 16},
                                   {"9:16", 9.0 / 16}, {"9:21", 9.0 / 21}, {"9:32", 9.0 / 32}};
    const Ratio *best = &ratios[0];
    for (const Ratio &ratio : ratios)
        if (std::abs(ratio.value - aspect) < std::abs(best->value - aspect))
            best = &ratio;
    return QString::fromLatin1(best->label);
}

QString Target::ratio() const { return Screens::ratioLabel(aspect()); }

QString Target::key() const { return ratio().replace(':', 'x'); }

QJsonObject Target::toJson() const {
    return {{"width", width},
            {"height", height},
            {"aspect", std::round(aspect() * 100) / 100},
            {"ratio", ratio()},
            {"key", key()},
            {"monitors", QJsonArray::fromStringList(monitors)},
            {"cropped", QJsonArray::fromStringList(cropped)}};
}

Target Target::fromJson(const QJsonObject &json) {
    Target target;
    if (json.value("width").toInt() > 0 && json.value("height").toInt() > 0) {
        target.width = json.value("width").toInt();
        target.height = json.value("height").toInt();
    }
    for (const QJsonValue &value : json.value("monitors").toArray())
        target.monitors << value.toString();
    for (const QJsonValue &value : json.value("cropped").toArray())
        target.cropped << value.toString();
    return target;
}

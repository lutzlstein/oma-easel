#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

struct Monitor {
    QString name;
    int width = 0;   // physical pixels, already swapped for rotated monitors
    int height = 0;
};

// The size a new background is exported at: the widest landscape monitor's
// shape, large enough to fill every landscape monitor without upscaling at
// display time. Portrait monitors would need a far taller image, so they are
// listed as cropped instead of driving the size.
struct Target {
    int width = 3840;
    int height = 2160;
    QStringList cropped;
    QStringList monitors;   // the monitors this shape is for, when known

    double aspect() const { return double(width) / height; }
    // The nearest familiar ratio, "21:9" or "9:16".
    QString ratio() const;
    // The ratio as a folder name, "21x9" or "9x16". It names a shape in a
    // batch and the folder a background's version for that shape lives in.
    QString key() const;
    bool portrait() const { return height > width; }
    QJsonObject toJson() const;
    static Target fromJson(const QJsonObject &json);
};

namespace Screens {
QList<Monitor> parse(const QByteArray &hyprctlJson);
QList<Monitor> query(const QString &hyprctl = "hyprctl");
Target target(const QList<Monitor> &monitors);
// One target per monitor shape, so each kind of screen gets images composed
// for it. Monitors within 15% of each other's aspect share a shape. The
// shape of the largest monitor comes first; it is the one a background's
// main image is made for, and the others are versions of it.
QList<Target> shapes(const QList<Monitor> &monitors);
QString ratioLabel(double aspect);
}

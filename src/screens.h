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

    double aspect() const { return double(width) / height; }
    QJsonObject toJson() const;
};

namespace Screens {
QList<Monitor> parse(const QByteArray &hyprctlJson);
QList<Monitor> query(const QString &hyprctl = "hyprctl");
Target target(const QList<Monitor> &monitors);
}

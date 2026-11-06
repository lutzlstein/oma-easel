#include "promptbuilder.h"

#include <QStringList>
#include <cmath>

// The nearest familiar ratio reads better in a prompt than 2.39:1.
QString PromptBuilder::aspectLabel(const Target &target) {
    struct Ratio { const char *label; double value; };
    static const Ratio ratios[] = {{"32:9", 32.0 / 9}, {"21:9", 21.0 / 9}, {"16:9", 16.0 / 9},
                                   {"16:10", 16.0 / 10}, {"3:2", 3.0 / 2}, {"4:3", 4.0 / 3}};
    const Ratio *best = &ratios[0];
    for (const Ratio &ratio : ratios)
        if (std::abs(ratio.value - target.aspect()) < std::abs(best->value - target.aspect()))
            best = &ratio;
    return QString::fromLatin1(best->label);
}

QString PromptBuilder::build(const QString &scene, const QMap<QString, QString> &colors, int referenceCount,
                             const Target &target, const QString &change) {
    QStringList parts;
    parts << "Create one new desktop wallpaper.";

    if (!change.trimmed().isEmpty() && referenceCount >= 1)
        parts << "The first attached image is the starting point. Keep its composition, medium, and palette, and "
                 "change only this: " + change.trimmed() + "." +
                     (referenceCount > 1 ? QString(" Any other attached images are style references only.") : QString());
    else if (referenceCount == 1)
        parts << "The attached image is a style reference only: do not edit or copy it. Match its medium, "
                 "brushwork, lighting, and mood in a new composition.";
    else if (referenceCount > 1)
        parts << QString("The %1 attached images are style references only: do not edit or copy them. Match "
                         "their shared medium, brushwork, lighting, and mood in a new composition.")
                     .arg(referenceCount);

    // The colors that decide whether a wallpaper sits well under the theme:
    // the base, the text, the accent, then the hues the theme leans on.
    QStringList palette;
    for (const char *key : {"background", "foreground", "accent", "red", "orange", "yellow", "green", "cyan",
                            "blue", "magenta"})
        if (colors.contains(key))
            palette << QString("%1 %2").arg(QString::fromLatin1(key), colors.value(key));
    if (!palette.isEmpty())
        parts << "Build the color palette from these theme colors, letting the darkest and the accent "
                 "dominate: " + palette.join(", ") + ".";

    parts << "Scene: " + scene.trimmed();
    parts << QString("Wide landscape composition at %1 (%2x%3), filling the whole frame. No text, no "
                     "lettering, no watermark, no signature, no border.")
                 .arg(aspectLabel(target))
                 .arg(target.width)
                 .arg(target.height);
    return parts.join("\n\n");
}

#include "promptbuilder.h"

#include <QStringList>
#include <cmath>

// The nearest familiar ratio reads better in a prompt than 2.39:1.
QString PromptBuilder::aspectLabel(const Target &target) {
    return target.ratio();
}

QString PromptBuilder::build(const QString &scene, const QMap<QString, QString> &colors, int referenceCount,
                             const Target &target, const QString &change, bool recompose) {
    QStringList parts;
    parts << "Create one new desktop wallpaper.";

    if (recompose && referenceCount >= 1)
        parts << QString("The first attached image is a finished wallpaper made for a screen of another shape. "
                         "Paint the same scene again for a %1 screen: keep its subject, setting, medium, "
                         "brushwork, palette, and lighting, and arrange them to fill the new frame naturally. "
                         "Do not crop, stretch, or pad the original.")
                     .arg(target.portrait() ? "tall portrait" : "wide landscape") +
                 (referenceCount > 1 ? QString(" Any other attached images are style references only.") : QString());
    else if (!change.trimmed().isEmpty() && referenceCount >= 1)
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
    const double aspect = target.aspect();
    const QString shape = aspect > 1.1 ? "Wide landscape" : aspect < 0.9 ? "Tall portrait" : "Square";
    parts << QString("%1 composition at %2 (%3x%4), filling the whole frame. No text, no "
                     "lettering, no watermark, no signature, no border.")
                 .arg(shape, aspectLabel(target))
                 .arg(target.width)
                 .arg(target.height);
    return parts.join("\n\n");
}

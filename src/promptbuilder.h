#pragma once

#include <QMap>
#include <QString>

#include "screens.h"

// Turns the scene the user typed into a complete image prompt: style
// references, the theme's palette, and the shape of the user's screens.
namespace PromptBuilder {
// With a `change`, the single reference is a candidate being varied: keep it
// and change only what was asked, instead of using it as a style guide.
//
// With `recompose`, the single reference is a finished wallpaper for another
// screen: keep the scene and compose it again for this target's shape.
QString build(const QString &scene, const QMap<QString, QString> &colors, int referenceCount,
              const Target &target, const QString &change = {}, bool recompose = false);
QString aspectLabel(const Target &target);
}

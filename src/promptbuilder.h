#pragma once

#include <QMap>
#include <QString>

#include "screens.h"

// Turns the scene the user typed into a complete image prompt: style
// references, the theme's palette, and the shape of the user's screens.
namespace PromptBuilder {
QString build(const QString &scene, const QMap<QString, QString> &colors, int referenceCount,
              const Target &target);
QString aspectLabel(const Target &target);
}

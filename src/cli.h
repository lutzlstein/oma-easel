#pragma once

#include <QStringList>

#include "paths.h"

// The display-free commands. Each prints plain text, or JSON with --json,
// and returns 0 on success or 1 with the reason on stderr.
namespace Cli {
bool handles(const QStringList &arguments);
int run(const QStringList &arguments, const Paths &paths);
QString usage();
}

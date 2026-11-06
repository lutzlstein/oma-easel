#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

// Which ways of making images this machine has. When no generator is
// available, easel falls back to manual mode: it hands the user a ready
// prompt to paste into any image platform they can use, and imports the
// image they bring back.
struct BackendStatus {
    QString name;       // "codex", "openai", "manual"
    bool available = false;
    QString detail;     // why it is or isn't available, shown to the user

    QJsonObject toJson() const;
};

namespace Backends {
BackendStatus codex(const QString &codex = "codex");
BackendStatus openai();
BackendStatus manual();
QList<BackendStatus> all(const QString &codex = "codex");
QString preferred(const QList<BackendStatus> &statuses);
QString manualSteps(const QString &themeSlug, int referenceCount);
}

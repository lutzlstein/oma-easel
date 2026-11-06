#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

// Which ways of making images this machine has. When no generator is
// available, Oma Easel falls back to manual mode: it hands the user a ready
// prompt to paste into any image platform they can use, and imports the
// image they bring back.
struct BackendStatus {
    QString name;       // "codex", "openai", "gemini", "manual"
    bool available = false;
    QString detail;     // why it is or isn't available, shown to the user

    QJsonObject toJson() const;
};

class Generator;
class Settings;
struct Target;
class QObject;

namespace Backends {
BackendStatus codex(const QString &codex = "codex");
BackendStatus openai();
BackendStatus gemini();
BackendStatus manual();
QString label(const QString &name);
// A generator for the named backend, or nullptr with the reason.
Generator *create(const QString &name, const Settings &settings, const Target &target, const QString &codex,
                  const QString &codexImages, QObject *parent, QString *error);
QList<BackendStatus> all(const QString &codex = "codex");
QString preferred(const QList<BackendStatus> &statuses);
QString manualSteps(const QString &themeSlug, int referenceCount);
}

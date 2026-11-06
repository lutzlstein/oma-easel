#include "backends.h"

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

#include "apigenerators.h"
#include "codexgenerator.h"
#include "keystore.h"
#include "settings.h"

BackendStatus Backends::codex(const QString &codex) {
    const QString executable = QFileInfo(codex).isAbsolute() ? codex : QStandardPaths::findExecutable(codex);
    if (executable.isEmpty() || !QFileInfo(executable).isExecutable())
        return {"codex", false, "Codex is not installed."};

    // `codex login status` exits 0 only when a ChatGPT login or API key is set,
    // which is what the built-in image tool needs.
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(executable, {"login", "status"});
    if (!process.waitForFinished(15000)) {
        process.kill();
        return {"codex", false, "Codex did not answer `codex login status`."};
    }
    const QString answer = QString::fromUtf8(process.readAll()).trimmed().section('\n', -1);
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return {"codex", false, "Codex is not logged in. Run `codex login`."};
    return {"codex", true, answer.isEmpty() ? QString("Logged in.") : answer};
}

namespace {
BackendStatus keyed(const QString &name) {
    if (KeyStore::fromEnvironment(name))
        return {name, true, "Using " + KeyStore::environmentVariable(name) + "."};
    if (!KeyStore::get(name).isEmpty())
        return {name, true, "API key saved in the keyring."};
    return {name, false, "No API key yet. Paste one below, or set " + KeyStore::environmentVariable(name) + "."};
}
}

BackendStatus Backends::openai() { return keyed("openai"); }
BackendStatus Backends::gemini() { return keyed("gemini"); }

QString Backends::label(const QString &name) {
    if (name == "codex") return "Codex";
    if (name == "openai") return "OpenAI API";
    if (name == "gemini") return "Gemini API";
    return "Manual";
}

Generator *Backends::create(const QString &name, const Settings &settings, const Target &target, const QString &codex,
                            const QString &codexImages, QObject *parent, QString *error) {
    if (name == "codex")
        return new CodexGenerator(codex, codexImages, parent);
    if (name == "openai" || name == "gemini") {
        const QString key = KeyStore::get(name);
        if (key.isEmpty()) {
            *error = label(name) + " has no API key.";
            return nullptr;
        }
        if (name == "openai")
            return new OpenAIGenerator(key, settings.openaiModel(), target, parent);
        return new GeminiGenerator(key, settings.geminiModel(), target, parent);
    }
    *error = "Manual mode doesn't generate; copy the prompt instead.";
    return nullptr;
}

BackendStatus Backends::manual() {
    return {"manual", true, "Copy the prompt into any image platform you can use, then import the result."};
}

QList<BackendStatus> Backends::all(const QString &codex) {
    return {Backends::codex(codex), openai(), gemini(), manual()};
}

QString Backends::preferred(const QList<BackendStatus> &statuses) {
    for (const BackendStatus &status : statuses)
        if (status.available)
            return status.name;
    return "manual";
}

// Kept generic on purpose: naming each platform's buttons would go stale as
// soon as one of them changes.
QString Backends::manualSteps(const QString &themeSlug, int referenceCount) {
    QStringList steps;
    steps << "Open an image generator you have access to (ChatGPT, Gemini, Copilot, or similar).";
    if (referenceCount > 0)
        steps << QString("Attach the %1 reference image(s) before pasting the prompt.").arg(referenceCount);
    steps << "Paste the prompt. If the platform lets you pick a shape, pick the widest landscape option.";
    steps << QString("Download the image, then run: oma-easel import <file> --theme %1").arg(themeSlug);
    QString text;
    for (int i = 0; i < steps.size(); ++i)
        text += QString("%1. %2\n").arg(i + 1).arg(steps.at(i));
    return text;
}

QJsonObject BackendStatus::toJson() const {
    return {{"name", name}, {"available", available}, {"detail", detail}};
}

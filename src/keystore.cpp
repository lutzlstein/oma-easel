#include "keystore.h"

#include <QProcess>
#include <QStandardPaths>

namespace {
// Tests point this at a fake; the default is the real secret-tool.
QString secretTool() {
    const QString overridden = qEnvironmentVariable("EASEL_SECRET_TOOL");
    return overridden.isEmpty() ? QStandardPaths::findExecutable("secret-tool") : overridden;
}

QStringList attributes(const QString &provider) {
    return {"service", "easel", "provider", provider};
}
}

QString KeyStore::environmentVariable(const QString &provider) {
    if (provider == "openai")
        return "OPENAI_API_KEY";
    if (provider == "gemini")
        return "GEMINI_API_KEY";
    return {};
}

bool KeyStore::fromEnvironment(const QString &provider) {
    const QString variable = environmentVariable(provider);
    return !variable.isEmpty() && !qEnvironmentVariableIsEmpty(variable.toUtf8());
}

QString KeyStore::get(const QString &provider) {
    if (fromEnvironment(provider))
        return qEnvironmentVariable(environmentVariable(provider).toUtf8()).trimmed();
    const QString tool = secretTool();
    if (tool.isEmpty())
        return {};
    QProcess process;
    process.start(tool, QStringList{"lookup"} + attributes(provider));
    if (!process.waitForFinished(5000) || process.exitCode() != 0)
        return {};
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

bool KeyStore::set(const QString &provider, const QString &key, QString *error) {
    const QString tool = secretTool();
    if (tool.isEmpty()) {
        *error = QStringLiteral("secret-tool isn't installed, so the key can't be saved to the keyring. Set %1 instead.")
                     .arg(environmentVariable(provider));
        return false;
    }
    // secret-tool reads the secret from stdin, so the key never appears in a
    // process list.
    QProcess process;
    process.start(tool, QStringList{"store", "--label", "easel " + provider + " API key"} + attributes(provider));
    if (!process.waitForStarted(5000)) {
        *error = QStringLiteral("Could not run secret-tool.");
        return false;
    }
    process.write(key.trimmed().toUtf8());
    process.closeWriteChannel();
    if (!process.waitForFinished(15000) || process.exitCode() != 0) {
        *error = QStringLiteral("The keyring refused the key: ") + QString::fromUtf8(process.readAllStandardError()).trimmed();
        return false;
    }
    return true;
}

bool KeyStore::clear(const QString &provider, QString *error) {
    const QString tool = secretTool();
    if (tool.isEmpty()) {
        *error = QStringLiteral("secret-tool isn't installed.");
        return false;
    }
    QProcess process;
    process.start(tool, QStringList{"clear"} + attributes(provider));
    process.waitForFinished(5000);
    return true;
}

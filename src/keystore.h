#pragma once

#include <QString>

// API keys for image providers. An environment variable (OPENAI_API_KEY,
// GEMINI_API_KEY) wins; otherwise the key is kept in the desktop keyring
// through secret-tool, never in Oma Easel's own files.
namespace KeyStore {
QString environmentVariable(const QString &provider);
QString get(const QString &provider);
bool fromEnvironment(const QString &provider);
bool set(const QString &provider, const QString &key, QString *error);
bool clear(const QString &provider, QString *error);
}

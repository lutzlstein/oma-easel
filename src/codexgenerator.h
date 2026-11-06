#pragma once

#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>

#include "generator.h"

// Generates through `codex exec` and Codex's built-in image_gen tool, which
// runs on the user's ChatGPT login and needs no API key. Codex saves the image
// under ~/.codex/generated_images and replies with its path, which is copied
// to the request's output path.
class CodexGenerator : public Generator {
    Q_OBJECT

public:
    explicit CodexGenerator(const QString &codex = "codex", const QString &generatedImages = {},
                            QObject *parent = nullptr);
    ~CodexGenerator() override;

    QString name() const override { return "codex"; }
    void start(const Request &request) override;
    void cancel() override;

    static QStringList arguments(const Request &request, const QString &workDirectory,
                                 const QString &lastMessageFile);
    static QString instructions(const QString &prompt);
    static QString findImagePath(const QString &text, const QString &generatedImages);

private:
    void finish(int exitCode, QProcess::ExitStatus status);
    void fail(const QString &error);

    QString m_codex;
    QString m_generatedImages;
    Request m_request;
    QProcess m_process;
    QTimer m_timeout;
    std::unique_ptr<QTemporaryDir> m_work;
    bool m_done = false;
};

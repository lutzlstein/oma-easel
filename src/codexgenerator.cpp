#include "codexgenerator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QRegularExpression>
#include <csignal>
#include <unistd.h>

CodexGenerator::CodexGenerator(const QString &codex, const QString &generatedImages, QObject *parent)
    : Generator(parent), m_codex(codex), m_generatedImages(generatedImages) {
    // Image generation usually takes a minute or two; past ten minutes the job
    // is stuck, and a stuck job would block the whole batch.
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(10 * 60 * 1000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        stop();
        fail("Codex did not finish within ten minutes.");
    });
    // Codex runs in its own process group, so stopping a job stops everything
    // it started, not just the first process.
    m_process.setChildProcessModifier([] { ::setpgid(0, 0); });
    connect(&m_process, &QProcess::finished, this, &CodexGenerator::finish);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            fail(QString("Could not run %1. Is Codex installed and on PATH?").arg(m_codex));
    });
}

CodexGenerator::~CodexGenerator() {
    stop();
}

void CodexGenerator::stop() {
    if (m_process.state() == QProcess::NotRunning)
        return;
    const qint64 pid = m_process.processId();
    if (pid > 0)
        ::kill(-pid, SIGTERM);
    if (!m_process.waitForFinished(1500)) {
        if (pid > 0)
            ::kill(-pid, SIGKILL);
        m_process.waitForFinished(1500);
    }
}

// Codex is an agent, so the prompt tells it to do one thing: call the image
// tool once, run nothing else, and answer with the saved path.
QString CodexGenerator::instructions(const QString &prompt) {
    return "Use your built-in image_gen tool exactly once to create the image described below. Do not run "
           "shell commands, do not write or edit files, and do not ask questions. When the image is saved, "
           "reply with only its absolute file path.\n\n" + prompt;
}

QStringList CodexGenerator::arguments(const Request &request, const QString &workDirectory,
                                      const QString &lastMessageFile) {
    QStringList arguments{"exec", "--skip-git-repo-check", "--sandbox", "read-only", "--color", "never",
                          "--cd", workDirectory, "--output-last-message", lastMessageFile};
    for (const QString &reference : request.references)
        arguments << "--image" << reference;
    // --image takes several values, so the prompt must come after "--" or
    // Codex reads it as one more image.
    arguments << "--" << instructions(request.prompt);
    return arguments;
}

// The reply should be just the path, but Codex sometimes wraps it in a
// sentence or backticks, so take the last image path under its image folder.
QString CodexGenerator::findImagePath(const QString &text, const QString &generatedImages) {
    static const QRegularExpression path(R"((/[^\s`'"]*generated_images/[^\s`'"]+\.(?:png|jpe?g|webp)))",
                                         QRegularExpression::CaseInsensitiveOption);
    QString found;
    auto it = path.globalMatch(text);
    while (it.hasNext()) {
        const QString candidate = it.next().captured(1);
        if (generatedImages.isEmpty() || candidate.startsWith(generatedImages))
            found = candidate;
    }
    return found;
}

void CodexGenerator::start(const Request &request) {
    m_request = request;
    m_done = false;
    m_work = std::make_unique<QTemporaryDir>();
    if (!m_work->isValid()) {
        fail("Could not create a working directory for Codex.");
        return;
    }
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    m_process.setStandardInputFile(QProcess::nullDevice());
    m_process.start(m_codex, arguments(request, m_work->path(), m_work->filePath("last-message.txt")));
    m_timeout.start();
}

void CodexGenerator::cancel() {
    m_done = true;  // a cancelled job reports nothing more
    m_timeout.stop();
    stop();
}

void CodexGenerator::finish(int exitCode, QProcess::ExitStatus status) {
    if (m_done)
        return;
    m_timeout.stop();
    const QString output = QString::fromUtf8(m_process.readAll());
    if (status != QProcess::NormalExit || exitCode != 0) {
        const QString tail = output.trimmed().right(400);
        fail(QString("Codex exited with code %1.%2").arg(exitCode).arg(tail.isEmpty() ? "" : "\n" + tail));
        return;
    }

    QFile lastMessage(m_work->filePath("last-message.txt"));
    const QString reply = lastMessage.open(QIODevice::ReadOnly) ? QString::fromUtf8(lastMessage.readAll()) : QString();
    QString source = findImagePath(reply, m_generatedImages);
    if (source.isEmpty())
        source = findImagePath(output, m_generatedImages);
    if (source.isEmpty() || !QFileInfo(source).isFile()) {
        fail("Codex finished without reporting a generated image. Its reply was: " + reply.trimmed().left(300));
        return;
    }

    // Candidates are always PNG, whatever the backend produced, so later
    // steps only deal with one lossless format.
    QDir().mkpath(QFileInfo(m_request.outputPath).absolutePath());
    const QImage image(source);
    if (image.isNull() || !image.save(m_request.outputPath, "PNG")) {
        fail("Could not copy the generated image from " + source);
        return;
    }
    m_done = true;
    emit finished(m_request.outputPath);
}

void CodexGenerator::fail(const QString &error) {
    if (m_done)
        return;
    m_done = true;
    m_timeout.stop();
    emit failed(error);
}

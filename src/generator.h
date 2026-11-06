#pragma once

#include <QObject>
#include <QStringList>

// One image generation job. Each backend (Codex now, the OpenAI API later)
// implements start() and reports exactly one of finished() or failed().
class Generator : public QObject {
    Q_OBJECT

public:
    struct Request {
        QString prompt;
        QStringList references;  // image files attached as style references
        QString outputPath;      // where the finished PNG should land
    };

    using QObject::QObject;
    virtual QString name() const = 0;
    virtual void start(const Request &request) = 0;
    virtual void cancel() = 0;

signals:
    void finished(const QString &outputPath);
    void failed(const QString &error);
};

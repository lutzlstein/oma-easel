#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSize>

#include "generator.h"
#include "screens.h"

// Image generation over a provider's HTTP API with the user's own key.
// Both report through the same Generator signals as Codex.
class ApiGenerator : public Generator {
    Q_OBJECT

public:
    ApiGenerator(const QString &key, const QString &model, const Target &target, QObject *parent = nullptr);
    void cancel() override;

protected:
    void send(const QNetworkRequest &request, const QByteArray &body);
    void send(const QNetworkRequest &request, class QHttpMultiPart *multipart);
    void handle(class QNetworkReply *reply);
    virtual QByteArray imageFrom(const QJsonObject &response, QString *error) const = 0;

    QString m_key;
    QString m_model;
    Target m_target;
    QString m_output;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    bool m_done = false;
};

// OpenAI Images API: /v1/images/generations, or /v1/images/edits when there
// are references to attach.
class OpenAIGenerator : public ApiGenerator {
    Q_OBJECT

public:
    using ApiGenerator::ApiGenerator;
    QString name() const override { return "openai"; }
    void start(const Request &request) override;

    static QSize sizeFor(const Target &target);
    static QByteArray decode(const QJsonObject &response, QString *error);

protected:
    QByteArray imageFrom(const QJsonObject &response, QString *error) const override { return decode(response, error); }
};

// Gemini API: generateContent on an image model, references sent inline.
class GeminiGenerator : public ApiGenerator {
    Q_OBJECT

public:
    using ApiGenerator::ApiGenerator;
    QString name() const override { return "gemini"; }
    void start(const Request &request) override;

    static QJsonObject body(const QString &prompt, const QStringList &references, const Target &target);
    static QByteArray decode(const QJsonObject &response, QString *error);

protected:
    QByteArray imageFrom(const QJsonObject &response, QString *error) const override { return decode(response, error); }
};

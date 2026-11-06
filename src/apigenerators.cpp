#include "apigenerators.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <cmath>

#include "promptbuilder.h"

ApiGenerator::ApiGenerator(const QString &key, const QString &model, const Target &target, QObject *parent)
    : Generator(parent), m_key(key), m_model(model), m_target(target) {}

void ApiGenerator::cancel() {
    m_done = true;
    if (m_reply)
        m_reply->abort();
}

void ApiGenerator::send(const QNetworkRequest &request, const QByteArray &body) {
    m_reply = m_network.post(request, body);
    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply] { handle(reply); });
}

void ApiGenerator::send(const QNetworkRequest &request, QHttpMultiPart *multipart) {
    m_reply = m_network.post(request, multipart);
    multipart->setParent(m_reply);
    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply] { handle(reply); });
}

// Both APIs answer with JSON carrying the image as base64; anything else is
// an error worth showing as the provider wrote it.
void ApiGenerator::handle(QNetworkReply *reply) {
    reply->deleteLater();
    if (m_done)
        return;
    m_done = true;
    const QByteArray data = reply->readAll();
    const QJsonObject json = QJsonDocument::fromJson(data).object();
    if (reply->error() != QNetworkReply::NoError) {
        const QJsonValue message = json.value("error").toObject().value("message");
        emit failed(QString("%1: %2").arg(name(), message.isString() ? message.toString() : reply->errorString()));
        return;
    }
    QString error;
    const QByteArray bytes = imageFrom(json, &error);
    const QImage image = QImage::fromData(bytes);
    if (image.isNull()) {
        emit failed(QString("%1: %2").arg(name(), error.isEmpty() ? QString("no image in the response") : error));
        return;
    }
    QDir().mkpath(QFileInfo(m_output).absolutePath());
    if (!image.save(m_output, "PNG")) {
        emit failed("Could not save the image to " + m_output);
        return;
    }
    emit finished(m_output);
}

// gpt-image-2 takes any size whose edges are multiples of 16, at most 3840,
// with a ratio up to 3:1 and between 655,360 and 8,294,400 pixels. The
// export size is used when it qualifies, else the nearest size that does.
QSize OpenAIGenerator::sizeFor(const Target &target) {
    double width = target.width, height = target.height;
    const double ratio = std::min(3.0, std::max(1.0 / 3.0, width / height));
    height = width / ratio;
    const double longest = std::max(width, height);
    if (longest > 3840) {
        width *= 3840 / longest;
        height *= 3840 / longest;
    }
    const double pixels = width * height;
    if (pixels > 8294400) {
        width *= std::sqrt(8294400 / pixels);
        height *= std::sqrt(8294400 / pixels);
    } else if (pixels < 655360) {
        width *= std::sqrt(655360 / pixels);
        height *= std::sqrt(655360 / pixels);
    }
    auto toGrid = [](double value) { return std::max(16, int(std::floor(value / 16)) * 16); };
    return {toGrid(width), toGrid(height)};
}

QByteArray OpenAIGenerator::decode(const QJsonObject &response, QString *error) {
    const QString base64 = response.value("data").toArray().first().toObject().value("b64_json").toString();
    if (base64.isEmpty())
        *error = "the response had no b64_json image";
    return QByteArray::fromBase64(base64.toLatin1());
}

void OpenAIGenerator::start(const Request &request) {
    m_output = request.outputPath;
    const QSize size = sizeFor(m_target);
    const QString sizeText = QString("%1x%2").arg(size.width()).arg(size.height());

    if (request.references.isEmpty()) {
        QNetworkRequest http(QUrl("https://api.openai.com/v1/images/generations"));
        http.setRawHeader("Authorization", "Bearer " + m_key.toUtf8());
        http.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        http.setTransferTimeout(10 * 60 * 1000);
        const QJsonObject body{{"model", m_model}, {"prompt", request.prompt}, {"size", sizeText}, {"quality", "high"}, {"n", 1}};
        send(http, QJsonDocument(body).toJson(QJsonDocument::Compact));
        return;
    }

    // References go to the edits endpoint as image[] parts.
    auto *multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    auto field = [multipart](const QString &name, const QString &value) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader, QString("form-data; name=\"%1\"").arg(name));
        part.setBody(value.toUtf8());
        multipart->append(part);
    };
    field("model", m_model);
    field("prompt", request.prompt);
    field("size", sizeText);
    field("quality", "high");
    for (const QString &reference : request.references) {
        auto *file = new QFile(reference, multipart);
        if (!file->open(QIODevice::ReadOnly)) {
            delete multipart;
            m_done = true;
            emit failed("Could not read the reference " + reference);
            return;
        }
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentTypeHeader, QMimeDatabase().mimeTypeForFile(reference).name());
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QString("form-data; name=\"image[]\"; filename=\"%1\"").arg(QFileInfo(reference).fileName()));
        part.setBodyDevice(file);
        multipart->append(part);
    }
    QNetworkRequest http(QUrl("https://api.openai.com/v1/images/edits"));
    http.setRawHeader("Authorization", "Bearer " + m_key.toUtf8());
    http.setTransferTimeout(10 * 60 * 1000);
    send(http, multipart);
}

QJsonObject GeminiGenerator::body(const QString &prompt, const QStringList &references, const Target &target) {
    QJsonArray parts{QJsonObject{{"text", prompt}}};
    for (const QString &reference : references) {
        QFile file(reference);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        parts.append(QJsonObject{{"inline_data", QJsonObject{{"mime_type", QMimeDatabase().mimeTypeForFile(reference).name()},
                                                             {"data", QString::fromLatin1(file.readAll().toBase64())}}}});
    }
    // Image only: asking for text as well has been seen to cap the size.
    return {{"contents", QJsonArray{QJsonObject{{"parts", parts}}}},
            {"generationConfig", QJsonObject{{"responseModalities", QJsonArray{"IMAGE"}},
                                             {"imageConfig", QJsonObject{{"aspectRatio", PromptBuilder::aspectLabel(target)},
                                                                         {"imageSize", "4K"}}}}}};
}

QByteArray GeminiGenerator::decode(const QJsonObject &response, QString *error) {
    const QJsonArray parts = response.value("candidates").toArray().first().toObject()
                                 .value("content").toObject().value("parts").toArray();
    for (const QJsonValue &part : parts) {
        const QJsonObject object = part.toObject();
        const QJsonObject inlineData = object.contains("inlineData") ? object.value("inlineData").toObject()
                                                                     : object.value("inline_data").toObject();
        const QString data = inlineData.value("data").toString();
        if (!data.isEmpty())
            return QByteArray::fromBase64(data.toLatin1());
    }
    const QString reason = response.value("promptFeedback").toObject().value("blockReason").toString();
    *error = reason.isEmpty() ? QString("the response had no image") : "blocked: " + reason;
    return {};
}

void GeminiGenerator::start(const Request &request) {
    m_output = request.outputPath;
    QNetworkRequest http(QUrl(QString("https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent").arg(m_model)));
    http.setRawHeader("x-goog-api-key", m_key.toUtf8());
    http.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    http.setTransferTimeout(10 * 60 * 1000);
    send(http, QJsonDocument(body(request.prompt, request.references, m_target)).toJson(QJsonDocument::Compact));
}

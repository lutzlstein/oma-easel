#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>

Settings::Settings(const QString &file, QObject *parent) : QObject(parent), m_file(file) {
    QFile input(file);
    if (!input.open(QIODevice::ReadOnly))
        return;
    const QJsonObject json = QJsonDocument::fromJson(input.readAll()).object();
    m_backend = json.value("backend").toString();
    m_openaiModel = json.value("openaiModel").toString(defaultOpenaiModel);
    m_geminiModel = json.value("geminiModel").toString(defaultGeminiModel);
    m_exportFormat = json.value("exportFormat").toString("webp");
    m_exportQuality = std::clamp(json.value("exportQuality").toInt(85), 40, 100);
    m_extraFolders = json.value("extraExportFolders").toObject().toVariantMap();
}

QString Settings::extraExportFolder(const QString &themeSlug) const {
    return m_extraFolders.value(themeSlug).toString();
}

void Settings::setBackend(const QString &backend) {
    if (backend == m_backend)
        return;
    m_backend = backend;
    save();
}

void Settings::setOpenaiModel(const QString &model) {
    const QString value = model.trimmed().isEmpty() ? QString(defaultOpenaiModel) : model.trimmed();
    if (value == m_openaiModel)
        return;
    m_openaiModel = value;
    save();
}

void Settings::setGeminiModel(const QString &model) {
    const QString value = model.trimmed().isEmpty() ? QString(defaultGeminiModel) : model.trimmed();
    if (value == m_geminiModel)
        return;
    m_geminiModel = value;
    save();
}

void Settings::setExportFormat(const QString &format) {
    if (!QStringList{"webp", "jpeg", "png"}.contains(format) || format == m_exportFormat)
        return;
    m_exportFormat = format;
    save();
}

void Settings::setExportQuality(int quality) {
    quality = std::clamp(quality, 40, 100);
    if (quality == m_exportQuality)
        return;
    m_exportQuality = quality;
    save();
}

void Settings::setExtraExportFolder(const QString &themeSlug, const QString &folder) {
    const QString value = folder.trimmed();
    if (value.isEmpty())
        m_extraFolders.remove(themeSlug);
    else
        m_extraFolders.insert(themeSlug, QDir::cleanPath(value.startsWith("~/") ? QDir::homePath() + value.mid(1) : value));
    save();
}

void Settings::save() {
    QDir().mkpath(QFileInfo(m_file).absolutePath());
    QSaveFile output(m_file);
    if (output.open(QIODevice::WriteOnly)) {
        const QJsonObject json{{"backend", m_backend},
                               {"openaiModel", m_openaiModel},
                               {"geminiModel", m_geminiModel},
                               {"exportFormat", m_exportFormat},
                               {"exportQuality", m_exportQuality},
                               {"extraExportFolders", QJsonObject::fromVariantMap(m_extraFolders)}};
        output.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
        output.commit();
    }
    emit changed();
}

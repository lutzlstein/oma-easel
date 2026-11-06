#pragma once

#include <QJsonObject>
#include <QObject>
#include <QVariantMap>

// What the user chose in the models & export menu, kept in
// ~/.config/easel/settings.json. API keys are not here; they live in the
// desktop keyring (see KeyStore).
class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString backend READ backend WRITE setBackend NOTIFY changed)
    Q_PROPERTY(QString openaiModel READ openaiModel WRITE setOpenaiModel NOTIFY changed)
    Q_PROPERTY(QString geminiModel READ geminiModel WRITE setGeminiModel NOTIFY changed)
    Q_PROPERTY(QString exportFormat READ exportFormat WRITE setExportFormat NOTIFY changed)
    Q_PROPERTY(int exportQuality READ exportQuality WRITE setExportQuality NOTIFY changed)

public:
    explicit Settings(const QString &file, QObject *parent = nullptr);

    QString backend() const { return m_backend; }          // empty: pick the first available
    QString openaiModel() const { return m_openaiModel; }
    QString geminiModel() const { return m_geminiModel; }
    QString exportFormat() const { return m_exportFormat; } // "webp", "jpeg", or "png"
    int exportQuality() const { return m_exportQuality; }
    Q_INVOKABLE QString extraExportFolder(const QString &themeSlug) const;

    void setBackend(const QString &backend);
    void setOpenaiModel(const QString &model);
    void setGeminiModel(const QString &model);
    void setExportFormat(const QString &format);
    void setExportQuality(int quality);
    Q_INVOKABLE void setExtraExportFolder(const QString &themeSlug, const QString &folder);

    static constexpr const char *defaultOpenaiModel = "gpt-image-2";
    static constexpr const char *defaultGeminiModel = "gemini-3.1-flash-image-preview";

signals:
    void changed();

private:
    void save();

    QString m_file;
    QString m_backend;
    QString m_openaiModel = defaultOpenaiModel;
    QString m_geminiModel = defaultGeminiModel;
    QString m_exportFormat = "webp";
    int m_exportQuality = 85;
    QVariantMap m_extraFolders;
};

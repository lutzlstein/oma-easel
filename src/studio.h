#pragma once

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <memory>
#include <optional>
#include <vector>

#include "backends.h"
#include "generator.h"
#include "library.h"
#include "paths.h"
#include "portalfilepicker.h"
#include "previewsession.h"
#include "screens.h"
#include "settings.h"
#include "themelibrary.h"

// The window's state and actions: the loaded theme, the chosen references,
// the scene, the batch on screen, and every earlier batch for the theme.
class Studio : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList themes READ themes CONSTANT)
    Q_PROPERTY(QString themeSlug READ themeSlug NOTIFY themeChanged)
    Q_PROPERTY(QString themeName READ themeName NOTIFY themeChanged)
    Q_PROPERTY(QVariantList palette READ palette NOTIFY themeChanged)
    Q_PROPERTY(QVariantList backgrounds READ backgrounds NOTIFY referencesChanged)
    Q_PROPERTY(QVariantList extraReferences READ extraReferences NOTIFY referencesChanged)
    Q_PROPERTY(QStringList references READ references NOTIFY referencesChanged)
    Q_PROPERTY(QString scene READ scene WRITE setScene NOTIFY sceneChanged)
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
    Q_PROPERTY(QString backend READ backend NOTIFY backendChanged)
    Q_PROPERTY(QString backendDetail READ backendDetail NOTIFY backendChanged)
    Q_PROPERTY(bool manual READ manual NOTIFY backendChanged)
    Q_PROPERTY(QString backendLabel READ backendLabel NOTIFY backendChanged)
    // Every way of making images, with whether it's ready and why:
    // [{name, label, available, detail, active, keyed, keyFromEnvironment, model}].
    Q_PROPERTY(QVariantList backends READ backends NOTIFY backendChanged)
    Q_PROPERTY(QObject *settings READ settingsObject CONSTANT)
    Q_PROPERTY(QVariantList candidates READ candidates NOTIFY candidatesChanged)
    Q_PROPERTY(QString batchScene READ batchScene NOTIFY candidatesChanged)
    Q_PROPERTY(QString batchChange READ batchChange NOTIFY candidatesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY candidatesChanged)
    Q_PROPERTY(int unkept READ unkept NOTIFY candidatesChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QString manualPrompt READ manualPrompt NOTIFY manualPromptChanged)
    Q_PROPERTY(QString manualSteps READ manualSteps NOTIFY manualPromptChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QString exportSize READ exportSize CONSTANT)
    Q_PROPERTY(double aspect READ aspect CONSTANT)
    Q_PROPERTY(int previewing READ previewing NOTIFY previewChanged)
    // Generation still running, for every theme, so switching themes never
    // hides or stops it: [{theme, themeName, batch, scene, pending, total, seconds}].
    Q_PROPERTY(QVariantList activity READ activity NOTIFY activityChanged)
    Q_PROPERTY(bool anyRunning READ anyRunning NOTIFY activityChanged)

public:
    struct Options {
        QString codex = "codex";
        PreviewSession::Commands preview;
        bool detectBackends = true;
        QString backend;                 // tests force one instead of detecting
        std::optional<Target> target;    // tests pass one instead of asking hyprctl
    };

    explicit Studio(const Paths &paths, const Options &options, QObject *parent = nullptr);
    explicit Studio(const Paths &paths, QObject *parent = nullptr) : Studio(paths, Options{}, parent) {}
    ~Studio() override;

    QVariantList themes() const;
    QString themeSlug() const { return m_theme.slug; }
    QString themeName() const { return m_theme.name; }
    QVariantList palette() const;
    QVariantList backgrounds() const;
    QVariantList extraReferences() const;
    QStringList references() const { return m_references; }
    QString scene() const { return m_scene; }
    void setScene(const QString &scene);
    int count() const { return m_count; }
    void setCount(int count);
    QString backend() const { return m_backend; }
    QString backendDetail() const { return m_backendDetail; }
    bool manual() const { return m_backend == "manual"; }
    QString backendLabel() const { return Backends::label(m_backend); }
    QVariantList backends() const;
    QObject *settingsObject() { return &m_settings; }
    QVariantList candidates() const;
    QString batchScene() const { return m_batchScene; }
    QString batchChange() const { return m_batchChange; }
    bool busy() const;
    int unkept() const;
    QVariantList history() const { return m_history; }
    QString manualPrompt() const { return m_manualPrompt; }
    QString manualSteps() const { return m_manualSteps; }
    QString message() const { return m_message; }
    QString exportSize() const { return QString("%1x%2").arg(m_target.width).arg(m_target.height); }
    double aspect() const { return m_target.aspect(); }
    int previewing() const { return m_previewing; }
    QVariantList activity() const;
    bool anyRunning() const;

    Q_INVOKABLE void loadTheme(const QString &slug);
    Q_INVOKABLE void chooseBackend(const QString &name);
    Q_INVOKABLE void refreshBackends();
    Q_INVOKABLE void saveKey(const QString &provider, const QString &key);
    Q_INVOKABLE void forgetKey(const QString &provider);
    Q_INVOKABLE QString extraExportFolder() const { return m_settings.extraExportFolder(m_theme.slug); }
    Q_INVOKABLE void setExtraExportFolder(const QString &folder);
    Q_INVOKABLE void toggleReference(const QString &path);
    Q_INVOKABLE void generate();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void keep(int index, const QString &name = {});
    Q_INVOKABLE void discard(int index);
    Q_INVOKABLE void vary(int index, const QString &change = {});
    Q_INVOKABLE void preview(int index);
    Q_INVOKABLE void endPreview();
    Q_INVOKABLE void removeBackground(const QString &path);
    Q_INVOKABLE void openBatch(const QString &batch, const QString &select = {});
    Q_INVOKABLE void copyPrompt();
    Q_INVOKABLE void copyReferencePaths();
    Q_INVOKABLE void openReferenceFolder();
    Q_INVOKABLE void pickImports();
    Q_INVOKABLE void importUrls(const QList<QUrl> &urls);
    void importFiles(const QStringList &files);
    void reloadHistory();

signals:
    void themeChanged();
    void referencesChanged();
    void sceneChanged();
    void countChanged();
    void backendChanged();
    void candidatesChanged();
    void historyChanged();
    void manualPromptChanged();
    void messageChanged();
    void previewChanged();
    void activityChanged();
    void selectCandidate(int index);

private:
    // One image being generated. Jobs belong to their batch, not to the
    // screen, so they finish even after the user moves to another theme.
    struct Job {
        Generator *generator = nullptr;
        QString theme;
        QString batch;
        QString scene;
        QString path;
        int total = 0;
        QDateTime started;
    };

    struct Candidate {
        QString path;
        QString state;   // "pending", "ready", "failed", "kept"
        QString error;
        QString keptAs;
    };

    void setMessage(const QString &message);
    void reloadBackgrounds();
    QString buildPrompt() const;
    QJsonObject batchInfo(const QString &backend) const;
    bool ensureManualBatch();
    void stopJobs(const QString &batch = {});
    void jobEnded(Generator *generator, bool ok, const QString &error);
    QList<Candidate> candidatesFor(const QString &batch) const;
    void previewDecided(bool keep);
    QHash<QString, QString> keptCandidates() const;
    void writeStatus() const;

    void pickBackend();

    Paths m_paths;
    Options m_options;
    Settings m_settings;
    QList<BackendStatus> m_statuses;
    ThemeLibrary m_library;
    Library m_store;
    Target m_target;
    ThemeInfo m_theme;
    QStringList m_references;
    QString m_scene;
    QString m_change;         // what to change, while varying a candidate
    int m_count = 3;
    QString m_backend = "manual";
    QString m_backendDetail;
    QString m_batch;
    QString m_batchScene;
    QString m_batchChange;
    QList<Candidate> m_candidates;
    QVariantList m_history;
    QList<Job> m_jobs;
    QTimer m_clock;
    QString m_manualPrompt;
    QString m_manualSteps;
    QString m_message;
    int m_previewing = -1;
    PreviewSession m_preview;
    PortalFilePicker m_picker;
};

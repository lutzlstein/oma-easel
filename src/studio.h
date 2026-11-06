#pragma once

#include <QObject>
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
#include "screens.h"
#include "themelibrary.h"

// The window's state and actions: the loaded theme, the chosen references,
// the scene, and the batch of candidates being generated or imported.
class Studio : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList themes READ themes CONSTANT)
    Q_PROPERTY(QString themeSlug READ themeSlug NOTIFY themeChanged)
    Q_PROPERTY(QString themeName READ themeName NOTIFY themeChanged)
    Q_PROPERTY(QVariantList palette READ palette NOTIFY themeChanged)
    Q_PROPERTY(QVariantList backgrounds READ backgrounds NOTIFY referencesChanged)
    Q_PROPERTY(QStringList references READ references NOTIFY referencesChanged)
    Q_PROPERTY(QString scene READ scene WRITE setScene NOTIFY sceneChanged)
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
    Q_PROPERTY(QString backend READ backend NOTIFY backendChanged)
    Q_PROPERTY(QString backendDetail READ backendDetail NOTIFY backendChanged)
    Q_PROPERTY(bool manual READ manual NOTIFY backendChanged)
    Q_PROPERTY(QVariantList candidates READ candidates NOTIFY candidatesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY candidatesChanged)
    Q_PROPERTY(int unkept READ unkept NOTIFY candidatesChanged)
    Q_PROPERTY(QString manualPrompt READ manualPrompt NOTIFY manualPromptChanged)
    Q_PROPERTY(QString manualSteps READ manualSteps NOTIFY manualPromptChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QString exportSize READ exportSize CONSTANT)
    Q_PROPERTY(double aspect READ aspect CONSTANT)
    Q_PROPERTY(int previewing READ previewing NOTIFY previewChanged)

public:
    struct Options {
        QString codex = "codex";
        QString setBackground = "omarchy-theme-bg-set";
        QString currentBackgroundLink;   // ~/.local/state/omarchy/current/background
        bool detectBackends = true;
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
    QStringList references() const { return m_references; }
    QString scene() const { return m_scene; }
    void setScene(const QString &scene);
    int count() const { return m_count; }
    void setCount(int count);
    QString backend() const { return m_backend; }
    QString backendDetail() const { return m_backendDetail; }
    bool manual() const { return m_backend == "manual"; }
    QVariantList candidates() const;
    bool busy() const;
    int unkept() const;
    QString manualPrompt() const { return m_manualPrompt; }
    QString manualSteps() const { return m_manualSteps; }
    QString message() const { return m_message; }
    QString exportSize() const { return QString("%1x%2").arg(m_target.width).arg(m_target.height); }
    double aspect() const { return m_target.aspect(); }
    int previewing() const { return m_previewing; }

    Q_INVOKABLE void loadTheme(const QString &slug);
    Q_INVOKABLE void toggleReference(const QString &path);
    Q_INVOKABLE void generate();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void keep(int index, const QString &name = {});
    Q_INVOKABLE void discard(int index);
    Q_INVOKABLE void vary(int index);
    Q_INVOKABLE void preview(int index);
    Q_INVOKABLE void endPreview();
    Q_INVOKABLE void copyPrompt();
    Q_INVOKABLE void copyReferencePaths();
    Q_INVOKABLE void openReferenceFolder();
    Q_INVOKABLE void pickImports();
    Q_INVOKABLE void importUrls(const QList<QUrl> &urls);
    void importFiles(const QStringList &files);

signals:
    void themeChanged();
    void referencesChanged();
    void sceneChanged();
    void countChanged();
    void backendChanged();
    void candidatesChanged();
    void manualPromptChanged();
    void messageChanged();
    void previewChanged();

private:
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
    void stopJobs();

    Paths m_paths;
    Options m_options;
    ThemeLibrary m_library;
    Library m_store;
    Target m_target;
    ThemeInfo m_theme;
    QStringList m_references;
    QString m_scene;
    int m_count = 3;
    QString m_backend = "manual";
    QString m_backendDetail;
    QString m_batch;
    QList<Candidate> m_candidates;
    std::vector<std::unique_ptr<Generator>> m_jobs;
    QString m_manualPrompt;
    QString m_manualSteps;
    QString m_message;
    int m_previewing = -1;
    QString m_backgroundBeforePreview;
    PortalFilePicker m_picker;
};

#include "studio.h"

#include <QClipboard>
#include <algorithm>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QProcess>

#include "codexgenerator.h"
#include "promptbuilder.h"

Studio::Studio(const Paths &paths, const Options &options, QObject *parent)
    : QObject(parent), m_paths(paths), m_options(options), m_library(paths), m_store(paths) {
    if (m_options.currentBackgroundLink.isEmpty())
        m_options.currentBackgroundLink = QDir::homePath() + "/.local/state/omarchy/current/background";
    m_target = m_options.target ? *m_options.target : Screens::target(Screens::query());

    if (m_options.detectBackends) {
        const QList<BackendStatus> statuses = Backends::all(m_options.codex);
        m_backend = Backends::preferred(statuses);
        for (const BackendStatus &status : statuses)
            if (status.name == m_backend)
                m_backendDetail = status.detail;
        // In manual mode, say why the generators were passed over.
        if (m_backend == "manual")
            m_backendDetail = statuses.first().detail;
    }

    connect(&m_picker, &PortalFilePicker::picked, this, &Studio::importFiles);
    connect(&m_picker, &PortalFilePicker::failed, this, &Studio::setMessage);

    const QString current = m_library.currentSlug();
    const QStringList slugs = m_library.slugs();
    loadTheme(slugs.contains(current) ? current : slugs.value(0));
}

Studio::~Studio() {
    stopJobs();
    endPreview();
}

QVariantList Studio::themes() const {
    QVariantList list;
    for (const QString &slug : m_library.slugs())
        list.append(QVariantMap{{"slug", slug}, {"name", ThemeLibrary::displayName(slug)}});
    return list;
}

QVariantList Studio::palette() const {
    QVariantList list;
    for (const char *key : {"background", "foreground", "accent", "red", "orange", "yellow", "green", "cyan",
                            "blue", "magenta"})
        if (m_theme.colors.contains(key))
            list.append(QVariantMap{{"name", QString::fromLatin1(key)}, {"color", m_theme.colors.value(key)}});
    return list;
}

QVariantList Studio::backgrounds() const {
    QVariantList list;
    for (const QString &path : m_theme.backgrounds)
        list.append(QVariantMap{{"path", path},
                                {"name", QFileInfo(path).fileName()},
                                {"selected", m_references.contains(path)}});
    return list;
}

void Studio::setScene(const QString &scene) {
    if (scene == m_scene)
        return;
    m_scene = scene;
    emit sceneChanged();
}

void Studio::setCount(int count) {
    count = std::clamp(count, 3, 6);
    if (count == m_count)
        return;
    m_count = count;
    emit countChanged();
}

QVariantList Studio::candidates() const {
    QVariantList list;
    for (const Candidate &candidate : m_candidates)
        list.append(QVariantMap{{"path", candidate.path},
                                {"state", candidate.state},
                                {"error", candidate.error},
                                {"keptAs", candidate.keptAs}});
    return list;
}

bool Studio::busy() const {
    for (const Candidate &candidate : m_candidates)
        if (candidate.state == "pending")
            return true;
    return false;
}

// Candidates that would be lost on quitting: ready but neither kept nor discarded.
int Studio::unkept() const {
    int count = 0;
    for (const Candidate &candidate : m_candidates)
        if (candidate.state == "ready")
            ++count;
    return count;
}

void Studio::setMessage(const QString &message) {
    m_message = message;
    emit messageChanged();
}

void Studio::loadTheme(const QString &slug) {
    const auto theme = m_library.load(slug);
    if (!theme) {
        setMessage(tr("No installed theme called \"%1\".").arg(slug));
        return;
    }
    stopJobs();
    endPreview();
    m_theme = *theme;
    m_references.clear();
    m_candidates.clear();
    m_batch.clear();
    m_manualPrompt.clear();
    m_manualSteps.clear();
    setMessage({});
    emit themeChanged();
    emit referencesChanged();
    emit candidatesChanged();
    emit manualPromptChanged();
}

void Studio::reloadBackgrounds() {
    if (const auto theme = m_library.load(m_theme.slug))
        m_theme.backgrounds = theme->backgrounds;
    emit referencesChanged();
}

void Studio::toggleReference(const QString &path) {
    if (!m_references.removeOne(path))
        m_references.append(path);
    emit referencesChanged();
    if (!m_manualPrompt.isEmpty()) {
        m_manualPrompt = buildPrompt();
        m_manualSteps = Backends::manualSteps(m_theme.slug, m_references.size());
        emit manualPromptChanged();
    }
}

QString Studio::buildPrompt() const {
    return PromptBuilder::build(m_scene, m_theme.colors, m_references.size(), m_target);
}

QJsonObject Studio::batchInfo(const QString &backend) const {
    return {{"scene", m_scene.trimmed()},
            {"prompt", buildPrompt()},
            {"references", QJsonArray::fromStringList(m_references)},
            {"backend", backend},
            {"theme", m_theme.slug},
            {"target", m_target.toJson()},
            {"createdAt", QDateTime::currentDateTime().toString(Qt::ISODate)}};
}

void Studio::generate() {
    if (m_scene.trimmed().isEmpty()) {
        setMessage(tr("Describe a scene first."));
        return;
    }
    if (busy())
        return;

    // Without a generator, the prompt is the product: show it with the steps
    // and wait for images to be imported.
    if (manual()) {
        m_batch.clear();
        m_manualPrompt = buildPrompt();
        m_manualSteps = Backends::manualSteps(m_theme.slug, m_references.size());
        emit manualPromptChanged();
        setMessage(tr("Copy the prompt into an image platform, then import or drop the result here."));
        return;
    }

    QString error;
    m_batch = m_store.createBatch(m_theme.slug, batchInfo(m_backend), &error);
    if (m_batch.isEmpty()) {
        setMessage(error);
        return;
    }
    stopJobs();
    m_candidates.clear();
    const QString prompt = buildPrompt();
    for (int i = 0; i < m_count; ++i) {
        m_candidates.append({Library::candidatePath(m_batch, i), "pending", {}, {}});
        auto job = std::make_unique<CodexGenerator>(m_options.codex, m_paths.codexImages);
        connect(job.get(), &Generator::finished, this, [this, i](const QString &) {
            if (i < m_candidates.size())
                m_candidates[i].state = "ready";
            emit candidatesChanged();
            if (!busy())
                setMessage(tr("Done. Keep the ones you like."));
        });
        connect(job.get(), &Generator::failed, this, [this, i](const QString &message) {
            if (i < m_candidates.size()) {
                m_candidates[i].state = "failed";
                m_candidates[i].error = message;
            }
            emit candidatesChanged();
        });
        job->start({prompt, m_references, m_candidates.at(i).path});
        m_jobs.push_back(std::move(job));
    }
    setMessage(tr("Generating %n candidate(s)…", "", m_count));
    emit candidatesChanged();
}

void Studio::stopJobs() {
    for (auto &job : m_jobs) {
        job->disconnect(this);
        job->cancel();
    }
    m_jobs.clear();
}

void Studio::cancel() {
    if (!busy())
        return;
    stopJobs();
    for (Candidate &candidate : m_candidates)
        if (candidate.state == "pending") {
            candidate.state = "failed";
            candidate.error = tr("Cancelled.");
        }
    emit candidatesChanged();
    setMessage(tr("Cancelled."));
}

void Studio::keep(int index, const QString &name) {
    if (index < 0 || index >= m_candidates.size() || m_candidates.at(index).state != "ready")
        return;
    QString error;
    const auto kept = m_store.keep(m_candidates.at(index).path, m_theme, m_target, name, &error);
    if (!kept) {
        setMessage(error);
        return;
    }
    m_candidates[index].state = "kept";
    m_candidates[index].keptAs = kept->exported;
    emit candidatesChanged();
    reloadBackgrounds();
    setMessage(tr("Kept as %1. It's now in %2's background cycle.").arg(QFileInfo(kept->exported).fileName(), m_theme.name));
}

void Studio::discard(int index) {
    if (index < 0 || index >= m_candidates.size() || m_candidates.at(index).state == "pending")
        return;
    if (m_previewing == index)
        endPreview();
    // A kept candidate's master and export stay; only the cached copy goes.
    QFile::remove(m_candidates.at(index).path);
    m_candidates.removeAt(index);
    if (m_previewing > index) {
        --m_previewing;
        emit previewChanged();
    }
    emit candidatesChanged();
}

// The candidate becomes the only reference for a new batch, so the next
// round refines it instead of starting over.
void Studio::vary(int index) {
    if (index < 0 || index >= m_candidates.size())
        return;
    const QString &state = m_candidates.at(index).state;
    if (state != "ready" && state != "kept")
        return;
    // Copied out of the cache batch, which the new batch may replace.
    const QString reference = m_paths.cache + "/" + m_theme.slug + "/vary-" +
                              QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz") + ".png";
    QDir().mkpath(QFileInfo(reference).absolutePath());
    if (!QFile::copy(m_candidates.at(index).path, reference)) {
        setMessage(tr("Could not copy the candidate to vary it."));
        return;
    }
    m_references = {reference};
    emit referencesChanged();
    generate();
}

void Studio::preview(int index) {
    if (index < 0 || index >= m_candidates.size())
        return;
    const Candidate &candidate = m_candidates.at(index);
    const QString path = candidate.state == "kept" ? candidate.keptAs : candidate.path;
    if (candidate.state == "pending" || candidate.state == "failed")
        return;
    if (m_previewing < 0)
        m_backgroundBeforePreview = QFileInfo(m_options.currentBackgroundLink).symLinkTarget();
    if (!QProcess::startDetached(m_options.setBackground, {path})) {
        setMessage(tr("Could not run %1 to preview.").arg(m_options.setBackground));
        return;
    }
    m_previewing = index;
    emit previewChanged();
    setMessage(tr("Previewing on the desktop. Press P or Esc to put your background back."));
}

void Studio::endPreview() {
    if (m_previewing < 0)
        return;
    if (!m_backgroundBeforePreview.isEmpty() && QFileInfo(m_backgroundBeforePreview).isFile())
        QProcess::startDetached(m_options.setBackground, {m_backgroundBeforePreview});
    m_previewing = -1;
    m_backgroundBeforePreview.clear();
    emit previewChanged();
}

void Studio::copyPrompt() {
    if (m_manualPrompt.isEmpty())
        m_manualPrompt = buildPrompt();
    QGuiApplication::clipboard()->setText(m_manualPrompt);
    setMessage(tr("Prompt copied."));
}

void Studio::copyReferencePaths() {
    if (m_references.isEmpty()) {
        setMessage(tr("No references chosen."));
        return;
    }
    QGuiApplication::clipboard()->setText(m_references.join('\n'));
    setMessage(tr("Copied %n reference path(s).", "", m_references.size()));
}

void Studio::openReferenceFolder() {
    const QString folder = m_references.isEmpty() ? QString() : QFileInfo(m_references.first()).absolutePath();
    if (folder.isEmpty() || !QDesktopServices::openUrl(QUrl::fromLocalFile(folder)))
        setMessage(tr("No reference folder to open."));
}

void Studio::pickImports() {
    m_picker.openImages(tr("Import images for %1").arg(m_theme.name));
}

void Studio::importUrls(const QList<QUrl> &urls) {
    QStringList files;
    for (const QUrl &url : urls)
        if (url.isLocalFile())
            files << url.toLocalFile();
    importFiles(files);
}

// Imported images join the current manual batch, or start one that records
// the scene and prompt they were made from.
bool Studio::ensureManualBatch() {
    if (!m_batch.isEmpty() && Library::readBatch(m_batch).value("backend").toString() == "manual")
        return true;
    QString error;
    m_batch = m_store.createBatch(m_theme.slug, batchInfo("manual"), &error);
    if (m_batch.isEmpty())
        setMessage(error);
    return !m_batch.isEmpty();
}

void Studio::importFiles(const QStringList &files) {
    if (files.isEmpty() || !ensureManualBatch())
        return;
    QString error;
    const QStringList imported = Library::importInto(m_batch, files, &error);
    if (imported.isEmpty()) {
        setMessage(error);
        return;
    }
    // A generated batch's candidates are replaced by the manual batch's.
    if (!m_candidates.isEmpty() && !QFileInfo(m_candidates.first().path).absolutePath().startsWith(m_batch))
        m_candidates.clear();
    for (const QString &path : imported)
        m_candidates.append({path, "ready", {}, {}});
    emit candidatesChanged();
    setMessage(tr("Imported %n image(s).", "", imported.size()));
}

#include "studio.h"

#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <algorithm>

#include "codexgenerator.h"
#include "promptbuilder.h"

Studio::Studio(const Paths &paths, const Options &options, QObject *parent)
    : QObject(parent), m_paths(paths), m_options(options), m_library(paths), m_store(paths),
      m_preview(options.preview) {
    m_target = m_options.target ? *m_options.target : Screens::target(Screens::query());

    if (!m_options.backend.isEmpty()) {
        m_backend = m_options.backend;
    } else if (m_options.detectBackends) {
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
    connect(&m_preview, &PreviewSession::decided, this, &Studio::previewDecided);
    // Ticks the elapsed time shown for running generation.
    m_clock.setInterval(1000);
    connect(&m_clock, &QTimer::timeout, this, &Studio::activityChanged);

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
    const QHash<QString, QString> added = m_store.addedBackgrounds(m_theme.slug);
    QVariantList list;
    for (const QString &path : m_theme.backgrounds)
        list.append(QVariantMap{{"path", path},
                                {"name", QFileInfo(path).fileName()},
                                {"selected", m_references.contains(path)},
                                {"easel", added.contains(path)}});
    return list;
}

// Takes a background easel added out of the theme's cycle. If the desktop is
// showing it, the desktop moves to another of the theme's backgrounds first.
void Studio::removeBackground(const QString &path) {
    const QString currentLink = m_options.preview.currentBackgroundLink.isEmpty()
                                    ? QDir::homePath() + "/.local/state/omarchy/current/background"
                                    : m_options.preview.currentBackgroundLink;
    const bool showing = QFileInfo(currentLink).symLinkTarget() == path;
    QString error;
    if (!m_store.remove(path, m_theme.slug, &error)) {
        setMessage(error);
        return;
    }
    m_references.removeAll(path);
    for (Candidate &candidate : m_candidates)
        if (candidate.keptAs == path) {
            candidate.state = "ready";
            candidate.keptAs.clear();
        }
    reloadBackgrounds();
    if (showing && !m_theme.backgrounds.isEmpty())
        m_preview.setBackground(m_theme.backgrounds.first());
    emit candidatesChanged();
    reloadHistory();
    setMessage(tr("Removed %1 from %2. It's in the trash if you want it back.").arg(QFileInfo(path).fileName(), m_theme.name));
}

// References that aren't the theme's backgrounds: candidates from this or
// earlier batches.
QVariantList Studio::extraReferences() const {
    QVariantList list;
    for (const QString &path : m_references)
        if (!m_theme.backgrounds.contains(path))
            list.append(QVariantMap{{"path", path}, {"name", QFileInfo(path).fileName()}});
    return list;
}

void Studio::setScene(const QString &scene) {
    if (scene == m_scene)
        return;
    m_scene = scene;
    emit sceneChanged();
}

void Studio::setCount(int count) {
    count = std::clamp(count, 1, 6);
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
                                {"keptAs", candidate.keptAs},
                                {"reference", m_references.contains(candidate.path)}});
    return list;
}

bool Studio::busy() const {
    for (const Job &job : m_jobs)
        if (!m_batch.isEmpty() && job.batch == m_batch)
            return true;
    return false;
}

bool Studio::anyRunning() const {
    return !m_jobs.isEmpty();
}

QVariantList Studio::activity() const {
    QVariantList list;
    QStringList seen;
    for (const Job &job : m_jobs) {
        if (seen.contains(job.batch))
            continue;
        seen << job.batch;
        int pending = 0;
        QDateTime started = job.started;
        for (const Job &other : m_jobs)
            if (other.batch == job.batch) {
                ++pending;
                started = std::min(started, other.started);
            }
        list.append(QVariantMap{{"theme", job.theme},
                                {"themeName", ThemeLibrary::displayName(job.theme)},
                                {"batch", job.batch},
                                {"scene", job.scene},
                                {"pending", pending},
                                {"total", job.total},
                                {"seconds", started.secsTo(QDateTime::currentDateTime())},
                                {"current", job.batch == m_batch}});
    }
    return list;
}

// Ready candidates that haven't been kept. They stay in the history drawer,
// so quitting only takes them off the screen, not off the disk.
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
    endPreview();
    m_theme = *theme;
    m_references.clear();
    m_candidates.clear();
    m_batch.clear();
    m_batchScene.clear();
    m_batchChange.clear();
    m_change.clear();
    m_manualPrompt.clear();
    m_manualSteps.clear();
    setMessage({});
    emit themeChanged();
    emit referencesChanged();
    emit candidatesChanged();
    emit manualPromptChanged();
    reloadHistory();
    // Coming back to a theme that is still generating shows that batch again.
    for (auto it = m_jobs.crbegin(); it != m_jobs.crend(); ++it)
        if (it->theme == m_theme.slug) {
            openBatch(it->batch);
            break;
        }
    emit activityChanged();
}

void Studio::reloadBackgrounds() {
    if (const auto theme = m_library.load(m_theme.slug))
        m_theme.backgrounds = theme->backgrounds;
    emit referencesChanged();
}

// Kept masters record the candidate they came from, which tells the history
// which candidates were kept and as what.
QHash<QString, QString> Studio::keptCandidates() const {
    QHash<QString, QString> kept;
    const QDir masters(m_paths.data + "/" + m_theme.slug);
    for (const QString &name : masters.entryList({"*.json"}, QDir::Files)) {
        QFile file(masters.filePath(name));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject sidecar = QJsonDocument::fromJson(file.readAll()).object();
        kept.insert(sidecar.value("candidate").toString(), sidecar.value("exported").toString());
    }
    return kept;
}

// Every batch for the theme, newest first, with its prompt and the
// candidates still on disk.
void Studio::reloadHistory() {
    const QHash<QString, QString> kept = keptCandidates();
    const QDir root(m_paths.cache + "/" + m_theme.slug);
    QVariantList history;
    QStringList batches = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    std::reverse(batches.begin(), batches.end());
    for (const QString &name : batches) {
        const QString batch = root.filePath(name);
        QVariantList images;
        for (const QString &file : QDir(batch).entryList({"candidate-*.png"}, QDir::Files, QDir::Name)) {
            const QString path = QDir(batch).filePath(file);
            images.append(QVariantMap{{"path", path},
                                      {"keptAs", kept.value(path)},
                                      {"reference", m_references.contains(path)}});
        }
        if (images.isEmpty())
            continue;
        const QJsonObject info = Library::readBatch(batch);
        history.append(QVariantMap{{"batch", batch},
                                   {"scene", info.value("scene").toString()},
                                   {"change", info.value("change").toString()},
                                   {"backend", info.value("backend").toString()},
                                   {"createdAt", info.value("createdAt").toString()},
                                   {"candidates", images}});
    }
    m_history = history;
    emit historyChanged();
}

// Any image can be a reference: the theme's backgrounds, candidates on
// screen, or candidates from the history drawer.
void Studio::toggleReference(const QString &path) {
    if (!m_references.removeOne(path))
        m_references.append(path);
    // Choosing references by hand ends a variation.
    m_change.clear();
    emit referencesChanged();
    emit candidatesChanged();
    reloadHistory();
    if (!m_manualPrompt.isEmpty()) {
        m_manualPrompt = buildPrompt();
        m_manualSteps = Backends::manualSteps(m_theme.slug, m_references.size());
        emit manualPromptChanged();
    }
}

QString Studio::buildPrompt() const {
    return PromptBuilder::build(m_scene, m_theme.colors, m_references.size(), m_target, m_change);
}

QJsonObject Studio::batchInfo(const QString &backend) const {
    return {{"scene", m_scene.trimmed()},
            {"change", m_change},
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
    m_candidates.clear();
    m_batchScene = m_scene.trimmed();
    m_batchChange = m_change;
    const QString prompt = buildPrompt();
    for (int i = 0; i < m_count; ++i) {
        const QString path = Library::candidatePath(m_batch, i);
        m_candidates.append({path, "pending", {}, {}});
        auto *generator = new CodexGenerator(m_options.codex, m_paths.codexImages, this);
        m_jobs.append({generator, m_theme.slug, m_batch, m_batchScene, path, m_count, QDateTime::currentDateTime()});
        connect(generator, &Generator::finished, this, [this, generator](const QString &) { jobEnded(generator, true, {}); });
        connect(generator, &Generator::failed, this, [this, generator](const QString &error) { jobEnded(generator, false, error); });
        generator->start({prompt, m_references, path});
    }
    m_clock.start();
    emit activityChanged();
    // The run is underway and its scene is shown with the batch, so the field
    // clears for the next idea.
    m_scene.clear();
    emit sceneChanged();
    setMessage(tr("Generating %n candidate(s)…", "", m_count));
    emit candidatesChanged();
}

void Studio::stopJobs(const QString &batch) {
    for (int i = m_jobs.size() - 1; i >= 0; --i) {
        if (!batch.isEmpty() && m_jobs.at(i).batch != batch)
            continue;
        Generator *generator = m_jobs.at(i).generator;
        generator->disconnect(this);
        generator->cancel();
        generator->deleteLater();
        m_jobs.removeAt(i);
    }
    if (m_jobs.isEmpty())
        m_clock.stop();
    emit activityChanged();
}

void Studio::cancel() {
    if (!busy())
        return;
    stopJobs(m_batch);
    for (Candidate &candidate : m_candidates)
        if (candidate.state == "pending") {
            candidate.state = "failed";
            candidate.error = tr("Cancelled.");
        }
    emit candidatesChanged();
    setMessage(tr("Cancelled."));
}

// A job finished or failed. Its candidate updates on screen only when its
// batch is the one showing; either way the history picks the image up.
void Studio::jobEnded(Generator *generator, bool ok, const QString &error) {
    for (int i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs.at(i).generator != generator)
            continue;
        const Job job = m_jobs.takeAt(i);
        generator->deleteLater();
        if (job.batch == m_batch)
            for (Candidate &candidate : m_candidates)
                if (candidate.path == job.path) {
                    candidate.state = ok ? "ready" : "failed";
                    candidate.error = error;
                }
        if (job.theme == m_theme.slug)
            reloadHistory();
        if (job.batch == m_batch) {
            emit candidatesChanged();
            if (!busy())
                setMessage(tr("Done. Keep the ones you like."));
        } else if (ok) {
            bool batchDone = true;
            for (const Job &other : m_jobs)
                batchDone = batchDone && other.batch != job.batch;
            if (batchDone)
                setMessage(tr("Finished generating for %1.").arg(ThemeLibrary::displayName(job.theme)));
        }
        break;
    }
    if (m_jobs.isEmpty())
        m_clock.stop();
    emit activityChanged();
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
    reloadHistory();
    setMessage(tr("Kept as %1. It's now in %2's background cycle.").arg(QFileInfo(kept->exported).fileName(), m_theme.name));
}

void Studio::discard(int index) {
    if (index < 0 || index >= m_candidates.size() || m_candidates.at(index).state == "pending")
        return;
    if (m_previewing == index)
        endPreview();
    // A kept candidate's master and export stay; only the cached copy goes.
    const QString path = m_candidates.at(index).path;
    QFile::remove(path);
    if (m_references.removeOne(path))
        emit referencesChanged();
    m_candidates.removeAt(index);
    if (m_previewing > index) {
        --m_previewing;
        emit previewChanged();
    }
    emit candidatesChanged();
    reloadHistory();
}

// A new batch from one candidate. With a change ("warmer light, add fog"),
// the candidate is the starting point and only that changes; without one, it
// is the only style reference for fresh compositions of the same scene.
void Studio::vary(int index, const QString &change) {
    if (index < 0 || index >= m_candidates.size())
        return;
    const QString &state = m_candidates.at(index).state;
    if (state != "ready" && state != "kept")
        return;
    if (m_scene.trimmed().isEmpty()) {
        m_scene = m_batchScene;
        emit sceneChanged();
    }
    m_references = {m_candidates.at(index).path};
    emit referencesChanged();
    m_change = change.trimmed();
    generate();
}

void Studio::preview(int index) {
    if (index < 0 || index >= m_candidates.size())
        return;
    const Candidate &candidate = m_candidates.at(index);
    if (candidate.state != "ready" && candidate.state != "kept")
        return;
    const QString path = candidate.state == "kept" ? candidate.keptAs : candidate.path;
    QString error;
    if (!m_preview.start(path, m_batchScene.isEmpty() ? QFileInfo(path).fileName() : m_batchScene, &error)) {
        setMessage(error);
        return;
    }
    m_previewing = index;
    emit previewChanged();
    setMessage(tr("Previewing on an empty workspace."));
}

// From the preview's floating prompt. Keeping a background that isn't kept
// yet keeps the candidate first, so the desktop points at the exported file
// rather than at a cache that may be cleared.
void Studio::previewDecided(bool keep) {
    const int index = m_previewing;
    if (keep && index >= 0 && index < m_candidates.size()) {
        if (m_candidates.at(index).state == "ready")
            this->keep(index);
        if (m_candidates.at(index).state == "kept")
            m_preview.setBackground(m_candidates.at(index).keptAs);
    }
    m_preview.finish(keep);
    m_previewing = -1;
    emit previewChanged();
    if (keep)
        setMessage(tr("Background set."));
}

void Studio::endPreview() {
    if (m_previewing < 0 && !m_preview.active())
        return;
    m_preview.finish(false);
    m_previewing = -1;
    emit previewChanged();
}

// Puts an earlier batch back on screen, with the scene it was made from.
void Studio::openBatch(const QString &batch, const QString &select) {
    const QJsonObject info = Library::readBatch(batch);
    m_candidates = candidatesFor(batch);
    int selected = 0;
    for (int i = 0; i < m_candidates.size(); ++i)
        if (m_candidates.at(i).path == select)
            selected = i;
    m_batch = batch;
    m_batchScene = info.value("scene").toString();
    m_batchChange = info.value("change").toString();
    m_scene = m_batchScene;
    emit sceneChanged();
    emit candidatesChanged();
    emit activityChanged();
    emit selectCandidate(selected);
}

// A batch's candidates: images on disk, plus any still being generated.
QList<Studio::Candidate> Studio::candidatesFor(const QString &batch) const {
    const QHash<QString, QString> kept = keptCandidates();
    QMap<QString, Candidate> byPath;
    for (const QString &file : QDir(batch).entryList({"candidate-*.png"}, QDir::Files)) {
        const QString path = QDir(batch).filePath(file);
        const QString keptAs = kept.value(path);
        byPath.insert(path, {path, keptAs.isEmpty() ? "ready" : "kept", {}, keptAs});
    }
    for (const Job &job : m_jobs)
        if (job.batch == batch)
            byPath.insert(job.path, {job.path, "pending", {}, {}});
    // candidate-10 sorts after candidate-9.
    QList<Candidate> list = byPath.values();
    std::sort(list.begin(), list.end(), [](const Candidate &a, const Candidate &b) {
        return QFileInfo(a.path).completeBaseName().section('-', 1).toInt() <
               QFileInfo(b.path).completeBaseName().section('-', 1).toInt();
    });
    return list;
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
    if (m_batch.isEmpty()) {
        setMessage(error);
        return false;
    }
    m_batchScene = m_scene.trimmed();
    m_batchChange = m_change;
    return true;
}

void Studio::importFiles(const QStringList &files) {
    if (files.isEmpty())
        return;
    const bool newBatch = m_batch.isEmpty() || Library::readBatch(m_batch).value("backend").toString() != "manual";
    if (!ensureManualBatch())
        return;
    QString error;
    const QStringList imported = Library::importInto(m_batch, files, &error);
    if (imported.isEmpty()) {
        setMessage(error);
        return;
    }
    if (newBatch)
        m_candidates.clear();
    for (const QString &path : imported)
        m_candidates.append({path, "ready", {}, {}});
    emit candidatesChanged();
    reloadHistory();
    setMessage(tr("Imported %n image(s).", "", imported.size()));
}

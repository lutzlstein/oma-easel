#include "studio.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QSaveFile>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <algorithm>

#include "codexgenerator.h"
#include "keystore.h"
#include "promptbuilder.h"

Studio::Studio(const Paths &paths, const Options &options, QObject *parent)
    : QObject(parent), m_paths(paths), m_options(options), m_settings(paths.settings), m_library(paths), m_store(paths),
      m_preview(options.preview) {
    if (!m_options.shapes.isEmpty())
        m_shapes = m_options.shapes;
    else if (m_options.target)
        m_shapes = {*m_options.target};
    else
        m_shapes = Screens::shapes(Screens::query());
    m_target = m_shapes.first();

    if (!m_options.backend.isEmpty())
        m_backend = m_options.backend;
    else if (m_options.detectBackends)
        refreshBackends();

    connect(&m_picker, &PortalFilePicker::picked, this, &Studio::importFiles);
    connect(&m_picker, &PortalFilePicker::failed, this, &Studio::setMessage);
    connect(&m_preview, &PreviewSession::decided, this, &Studio::previewDecided);
    // Ticks the elapsed time shown for running generation.
    m_clock.setInterval(1000);
    connect(&m_clock, &QTimer::timeout, this, &Studio::activityChanged);
    // The companion bar widget reads this to show what's generating.
    connect(this, &Studio::activityChanged, this, &Studio::writeStatus);

    const QString current = m_library.currentSlug();
    const QStringList slugs = m_library.slugs();
    loadTheme(slugs.contains(current) ? current : slugs.value(0));
    writeStatus();
}

Studio::~Studio() {
    stopJobs();
    endPreview();
    QFile::remove(m_paths.status);
}

// A small public contract with the companion plugin: whether the app is
// open, and what each running batch is doing. Written atomically; the pid
// lets a reader ignore a file left behind by a crash.
void Studio::writeStatus() const {
    if (m_paths.status.isEmpty())
        return;
    int pending = 0, total = 0;
    for (const QVariant &item : activity()) {
        pending += item.toMap().value("pending").toInt();
        total += item.toMap().value("total").toInt();
    }
    const QJsonObject status{{"pid", QCoreApplication::applicationPid()},
                             {"theme", m_theme.slug},
                             {"generating", pending},
                             {"total", total},
                             {"batches", QJsonArray::fromVariantList(activity())},
                             {"updatedAt", QDateTime::currentDateTime().toString(Qt::ISODate)}};
    QDir().mkpath(QFileInfo(m_paths.status).absolutePath());
    QSaveFile file(m_paths.status);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(status).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

// The saved choice when it's ready, else the first that is, so a missing
// login or key falls back instead of failing at Generate.
void Studio::pickBackend() {
    m_backend = Backends::preferred(m_statuses);
    for (const BackendStatus &status : m_statuses)
        if (status.name == m_settings.backend() && status.available)
            m_backend = status.name;
    m_backendDetail.clear();
    for (const BackendStatus &status : m_statuses)
        if (status.name == m_backend)
            m_backendDetail = status.detail;
    emit backendChanged();
}

void Studio::refreshBackends() {
    m_statuses = Backends::all(m_options.codex);
    pickBackend();
}

QVariantList Studio::backends() const {
    QVariantList list;
    for (const BackendStatus &status : m_statuses) {
        const bool keyed = status.name == "openai" || status.name == "gemini";
        list.append(QVariantMap{{"name", status.name},
                                {"label", Backends::label(status.name)},
                                {"available", status.available},
                                {"detail", status.detail},
                                {"active", status.name == m_backend},
                                {"keyed", keyed},
                                {"keyFromEnvironment", keyed && KeyStore::fromEnvironment(status.name)},
                                {"model", status.name == "openai" ? m_settings.openaiModel()
                                          : status.name == "gemini" ? m_settings.geminiModel() : QString()}});
    }
    return list;
}

void Studio::chooseBackend(const QString &name) {
    for (const BackendStatus &status : m_statuses)
        if (status.name == name && !status.available) {
            setMessage(status.detail);
            return;
        }
    m_settings.setBackend(name);
    pickBackend();
    setMessage(tr("Making images with %1.").arg(Backends::label(m_backend)));
}

void Studio::saveKey(const QString &provider, const QString &key) {
    if (key.trimmed().isEmpty())
        return;
    QString error;
    if (!KeyStore::set(provider, key, &error)) {
        setMessage(error);
        return;
    }
    refreshBackends();
    setMessage(tr("Saved the %1 key to the keyring.").arg(Backends::label(provider)));
}

void Studio::forgetKey(const QString &provider) {
    QString error;
    if (!KeyStore::clear(provider, &error))
        setMessage(error);
    refreshBackends();
}

void Studio::setExtraExportFolder(const QString &folder) {
    m_settings.setExtraExportFolder(m_theme.slug, folder);
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
                                {"added", added.contains(path)},
                                {"variants", Library::variantsOf(m_theme, path)}});
    return list;
}

QList<Target> Studio::enabledShapes() const {
    QList<Target> shapes;
    for (const Target &shape : m_shapes)
        if (!m_settings.skippedShapes().contains(shape.key()))
            shapes << shape;
    if (shapes.isEmpty())
        shapes << m_target;
    return shapes;
}

Target Studio::shapeFor(const QString &key) const {
    for (const Target &shape : m_shapes)
        if (shape.key() == key)
            return shape;
    return m_target;
}

QVariantList Studio::displays() const {
    const QList<Target> enabled = enabledShapes();
    QVariantList list;
    for (const Target &shape : m_shapes) {
        bool on = false;
        for (const Target &other : enabled)
            on = on || other.key() == shape.key();
        list.append(QVariantMap{{"key", shape.key()},
                                {"ratio", shape.ratio()},
                                {"size", QString("%1x%2").arg(shape.width).arg(shape.height)},
                                {"monitors", shape.monitors.join(", ")},
                                {"aspect", shape.aspect()},
                                {"primary", shape.key() == m_target.key()},
                                {"enabled", on}});
    }
    return list;
}

// At least one shape stays on, or Generate would have nothing to make.
void Studio::toggleDisplay(const QString &key) {
    QStringList skipped = m_settings.skippedShapes();
    if (!skipped.removeOne(key)) {
        if (enabledShapes().size() <= 1)
            return;
        skipped << key;
    }
    m_settings.setSkippedShapes(skipped);
    emit displaysChanged();
}

bool Studio::isVariant(const Candidate &candidate) const {
    return candidate.target.key() != m_target.key();
}

// The batch on screen, one section per shape, the main shape first.
QVariantList Studio::sections() const {
    QStringList order{m_target.key()};
    for (const Candidate &candidate : m_candidates)
        if (!order.contains(candidate.target.key()))
            order << candidate.target.key();
    QVariantList list;
    for (const QString &key : order) {
        QVariantList indices;
        Target shape;
        for (int i = 0; i < m_candidates.size(); ++i)
            if (m_candidates.at(i).target.key() == key) {
                indices << i;
                shape = m_candidates.at(i).target;
            }
        if (indices.isEmpty())
            continue;
        if (shape.monitors.isEmpty())
            shape.monitors = shapeFor(key).monitors;
        list.append(QVariantMap{{"key", key},
                                {"ratio", shape.ratio()},
                                {"aspect", shape.aspect()},
                                {"monitors", shape.monitors.join(", ")},
                                {"primary", key == m_target.key()},
                                {"indices", indices}});
    }
    return list;
}

// Takes a background Oma Easel added out of the theme's cycle. If the desktop is
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
                                {"reference", m_references.contains(candidate.path)},
                                {"shape", candidate.target.key()},
                                {"ratio", candidate.target.ratio()},
                                {"aspect", candidate.target.aspect()},
                                {"monitors", candidate.target.monitors.join(", ")},
                                {"variant", isVariant(candidate)}});
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
    // Versions for other screen shapes keep their sidecars in shape folders.
    QDirIterator it(m_paths.data + "/" + m_theme.slug, {"*.json"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFile file(it.next());
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
                                      {"aspect", Library::targetFor(path, m_target).aspect()},
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
        m_manualPrompt = buildPrompt(m_target);
        m_manualSteps = Backends::manualSteps(m_theme.slug, m_references.size());
        emit manualPromptChanged();
    }
}

QString Studio::buildPrompt(const Target &target, bool recompose) const {
    return PromptBuilder::build(m_scene, m_theme.colors, m_references.size(), target, recompose ? QString() : m_change,
                                recompose);
}

// How a batch was made. Each shape records its own prompt, and each
// candidate which shape it is for, so keeping one later sizes it right.
QJsonObject Studio::batchInfo(const QString &backend, const QList<Target> &shapes, int count, bool recompose) const {
    const QList<Target> made = shapes.isEmpty() ? QList<Target>{m_target} : shapes;
    QJsonArray shapeList;
    QJsonObject byCandidate;
    int index = 0;
    for (const Target &shape : made) {
        QJsonObject json = shape.toJson();
        json.insert("prompt", buildPrompt(shape, recompose));
        shapeList.append(json);
        for (int i = 0; i < count; ++i)
            byCandidate.insert(QFileInfo(Library::candidatePath({}, index++)).fileName(), shape.key());
    }
    return {{"scene", m_scene.trimmed()},
            {"change", recompose ? QString() : m_change},
            {"prompt", buildPrompt(made.first(), recompose)},
            {"shapes", shapeList},
            {"candidateShapes", byCandidate},
            {"references", QJsonArray::fromStringList(m_references)},
            {"backend", backend},
            {"model", backend == "openai" ? m_settings.openaiModel() : backend == "gemini" ? m_settings.geminiModel() : QString()},
            {"theme", m_theme.slug},
            {"target", made.first().toJson()},
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
        m_manualPrompt = buildPrompt(m_target);
        m_manualSteps = Backends::manualSteps(m_theme.slug, m_references.size());
        emit manualPromptChanged();
        setMessage(tr("Copy the prompt into an image platform, then import or drop the result here."));
        return;
    }

    startBatch(enabledShapes(), false, {});
}

// One batch with `m_count` candidates for each shape. A companion batch
// recomposes `companionOf` for the other shapes instead of making new scenes.
void Studio::startBatch(const QList<Target> &shapes, bool recompose, const QString &companionOf) {
    QString error;
    QJsonObject info = batchInfo(m_backend, shapes, m_count, recompose);
    if (!companionOf.isEmpty())
        info.insert("companionOf", companionOf);
    m_batch = m_store.createBatch(m_theme.slug, info, &error);
    if (m_batch.isEmpty()) {
        setMessage(error);
        return;
    }
    m_candidates.clear();
    m_batchScene = m_scene.trimmed();
    m_batchChange = recompose ? QString() : m_change;
    const int total = m_count * int(shapes.size());
    int index = 0;
    for (const Target &shape : shapes) {
        const QString prompt = buildPrompt(shape, recompose);
        for (int i = 0; i < m_count; ++i) {
            const QString path = Library::candidatePath(m_batch, index++);
            m_candidates.append({path, "pending", {}, {}, shape});
            Generator *generator =
                Backends::create(m_backend, m_settings, shape, m_options.codex, m_paths.codexImages, this, &error);
            if (!generator) {
                m_candidates.last().state = "failed";
                m_candidates.last().error = error;
                continue;
            }
            m_jobs.append({generator, m_theme.slug, m_batch, m_batchScene, path, total, QDateTime::currentDateTime()});
            connect(generator, &Generator::finished, this, [this, generator](const QString &) { jobEnded(generator, true, {}); });
            connect(generator, &Generator::failed, this, [this, generator](const QString &error) { jobEnded(generator, false, error); });
            generator->start({prompt, m_references, path});
        }
    }
    m_clock.start();
    emit activityChanged();
    // The run is underway and its scene is shown with the batch, so the field
    // clears for the next idea.
    m_scene.clear();
    emit sceneChanged();
    setMessage(tr("Generating %n candidate(s)…", "", total));
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

// Which kept background a candidate for another shape belongs with: the one
// its companion batch was made from, or else the only main image kept from
// its own batch. Anything less clear is left to the user.
QString Studio::pairFor(int index) const {
    const QHash<QString, QString> kept = keptCandidates();
    const QString source = Library::readBatch(m_batch).value("companionOf").toString();
    if (m_theme.backgrounds.contains(source))
        return source;
    const QString fromSource = kept.value(source);
    if (!fromSource.isEmpty() && QFileInfo(fromSource).absolutePath() == QFileInfo(m_theme.userBackgroundDirectory).absoluteFilePath())
        return fromSource;
    QStringList mains;
    for (int i = 0; i < m_candidates.size(); ++i)
        if (i != index && m_candidates.at(i).state == "kept" && !isVariant(m_candidates.at(i)))
            mains << m_candidates.at(i).keptAs;
    return mains.size() == 1 ? mains.first() : QString();
}

void Studio::keep(int index, const QString &name) {
    if (index < 0 || index >= m_candidates.size() || m_candidates.at(index).state != "ready")
        return;
    if (isVariant(m_candidates.at(index))) {
        const QString background = pairFor(index);
        if (background.isEmpty()) {
            setMessage(tr("Choose the background this %1 version belongs to.").arg(m_candidates.at(index).target.ratio()));
            emit pairNeeded(index);
            return;
        }
        keepVariant(index, background);
        return;
    }
    QString error;
    const Library::ExportOptions options{m_settings.exportFormat(), m_settings.exportQuality(),
                                         m_settings.extraExportFolder(m_theme.slug)};
    const auto kept = m_store.keep(m_candidates.at(index).path, m_theme, m_candidates.at(index).target, name, &error, options);
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

void Studio::keepVariant(int index, const QString &background) {
    if (index < 0 || index >= m_candidates.size() || background.isEmpty())
        return;
    const Candidate candidate = m_candidates.at(index);
    if (candidate.state != "ready" && candidate.state != "kept")
        return;
    QString error;
    const Library::ExportOptions options{m_settings.exportFormat(), m_settings.exportQuality(),
                                         m_settings.extraExportFolder(m_theme.slug)};
    const auto kept = m_store.keep(candidate.path, m_theme, candidate.target, {}, &error, options, background);
    if (!kept) {
        setMessage(error);
        return;
    }
    m_candidates[index].state = "kept";
    m_candidates[index].keptAs = kept->exported;
    m_candidates[index].pairedWith = background;
    // The desktop picks the new version up without a theme switch.
    m_preview.rescanVersions();
    emit candidatesChanged();
    reloadBackgrounds();
    reloadHistory();
    setMessage(tr("Saved as the %1 version of %2, for %3.")
                   .arg(candidate.target.ratio(), QFileInfo(background).fileName(),
                        candidate.target.monitors.isEmpty() ? candidate.target.ratio() + " screens"
                                                            : candidate.target.monitors.join(", ")));
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
    const Target shape = m_candidates.at(index).target;
    m_references = {m_candidates.at(index).path};
    emit referencesChanged();
    m_change = change.trimmed();
    // Variations keep the shape of the candidate they start from.
    if (manual() || m_scene.trimmed().isEmpty() || busy())
        generate();
    else
        startBatch({shape}, false, {});
}

void Studio::companion(int index) {
    if (index < 0 || index >= m_candidates.size())
        return;
    const Candidate source = m_candidates.at(index);
    if (source.state != "ready" && source.state != "kept")
        return;
    QList<Target> others;
    for (const Target &shape : m_shapes)
        if (shape.key() != source.target.key())
            others << shape;
    startCompanion(source.path, others, m_batchScene);
}

// The shapes a background's companion is made for: every screen shape
// except the one the image already fits, judged by its own pixels.
QList<Target> Studio::companionShapes(const QString &path) const {
    const QSize size = QImageReader(path).size();
    QString fits;
    if (size.isValid() && size.height() > 0) {
        const double aspect = double(size.width()) / size.height();
        double nearest = 0.15;
        for (const Target &shape : m_shapes) {
            const double difference = std::abs(shape.aspect() - aspect) / shape.aspect();
            if (difference < nearest) {
                nearest = difference;
                fits = shape.key();
            }
        }
    }
    QList<Target> shapes;
    for (const Target &shape : m_shapes)
        if (shape.key() != fits)
            shapes << shape;
    return shapes;
}

QString Studio::companionPlan(const QString &path) const {
    QStringList parts;
    for (const Target &shape : companionShapes(path))
        parts << (shape.monitors.isEmpty() ? shape.ratio() : shape.monitors.join(", ") + " " + shape.ratio());
    return parts.join(" · ");
}

void Studio::companionForBackground(const QString &path) {
    if (!m_theme.backgrounds.contains(path))
        return;
    // A background Oma Easel made remembers its scene; any other is
    // described by the image itself.
    QString scene;
    const QString master = m_store.addedBackgrounds(m_theme.slug).value(path);
    if (!master.isEmpty()) {
        QFile sidecar(QFileInfo(master).absolutePath() + "/" + QFileInfo(master).completeBaseName() + ".json");
        if (sidecar.open(QIODevice::ReadOnly))
            scene = QJsonDocument::fromJson(sidecar.readAll()).object().value("scene").toString();
    }
    if (scene.isEmpty())
        scene = tr("the scene in the attached image");
    startCompanion(path, companionShapes(path), scene);
}

void Studio::startCompanion(const QString &source, const QList<Target> &shapes, const QString &scene) {
    if (busy())
        return;
    if (manual()) {
        setMessage(tr("Companions need an image model. Choose one under models & export."));
        return;
    }
    if (shapes.isEmpty() || m_shapes.size() < 2) {
        setMessage(tr("This image already fits all your screens, so there's nothing to compose a companion for."));
        return;
    }
    if (m_scene.trimmed().isEmpty()) {
        m_scene = scene;
        emit sceneChanged();
    }
    m_references = {source};
    m_change.clear();
    emit referencesChanged();
    startBatch(shapes, true, source);
}

void Studio::preview(int index) {
    if (index < 0 || index >= m_candidates.size())
        return;
    const Candidate &candidate = m_candidates.at(index);
    if (candidate.state != "ready" && candidate.state != "kept")
        return;
    const QString path = candidate.state == "kept" ? candidate.keptAs : candidate.path;
    // A version for another shape is previewed on a monitor of that shape,
    // beside the background it would belong to.
    const bool variant = isVariant(candidate);
    const QString monitor = variant ? candidate.target.monitors.value(0) : QString();
    const QString paired = !variant ? QString() : candidate.pairedWith.isEmpty() ? pairFor(index) : candidate.pairedWith;
    QString error;
    if (!m_preview.start(path, m_batchScene.isEmpty() ? QFileInfo(path).fileName() : m_batchScene, &error, monitor,
                         paired)) {
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
    // Keeping a version keeps it and makes its background the desktop's, so
    // each screen shows its own shape of the pair. Without a background to
    // pair with yet, the window asks and the old background comes back.
    if (index >= 0 && index < m_candidates.size() && isVariant(m_candidates.at(index))) {
        if (keep && m_candidates.at(index).state == "ready")
            this->keep(index);
        const QString paired = m_candidates.at(index).state == "kept" ? m_candidates.at(index).pairedWith : QString();
        if (keep && !paired.isEmpty())
            m_preview.setBackground(paired);
        m_preview.finish(keep && !paired.isEmpty());
        m_previewing = -1;
        emit previewChanged();
        if (keep && !paired.isEmpty())
            setMessage(tr("Background set, with its %1 version on %2.")
                           .arg(m_candidates.at(index).target.ratio(), m_candidates.at(index).target.monitors.join(", ")));
        return;
    }
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
        byPath.insert(path, {path, keptAs.isEmpty() ? "ready" : "kept", {}, keptAs, Library::targetFor(path, m_target)});
    }
    for (const Job &job : m_jobs)
        if (job.batch == batch)
            byPath.insert(job.path, {job.path, "pending", {}, {}, Library::targetFor(job.path, m_target)});
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
        m_manualPrompt = buildPrompt(m_target);
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
    // An imported image is for whichever of the user's shapes it is closest to.
    for (const QString &path : imported) {
        const QSize size = QImageReader(path).size();
        Target shape = m_target;
        if (size.isValid() && size.height() > 0) {
            const double aspect = double(size.width()) / size.height();
            for (const Target &other : m_shapes)
                if (std::abs(other.aspect() - aspect) < std::abs(shape.aspect() - aspect))
                    shape = other;
        }
        Library::recordShape(path, shape);
        m_candidates.append({path, "ready", {}, {}, shape});
    }
    emit candidatesChanged();
    reloadHistory();
    setMessage(tr("Imported %n image(s).", "", imported.size()));
}

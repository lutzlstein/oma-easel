// Theme watching follows Omacut (MIT, David Heinemeier Hansson).
#include "theme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QHash>
#include <QStandardPaths>
#include <cmath>

Theme::Theme(const QString &directory, QObject *parent, const QString &hyprctl)
    : QObject(parent),
      m_directory(directory.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current" : directory) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);

    // Corners follow Hyprland's effective rounding, which themes and personal
    // overrides can both set, so ask hyprctl rather than reading its config.
    // Without Hyprland the corners stay sharp, like Omarchy's default.
    m_hyprctl = !hyprctl.isEmpty() ? hyprctl
              : directory.isEmpty() && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")
                  ? QStandardPaths::findExecutable("hyprctl") : QString();
    m_roundingTimeout.setSingleShot(true);
    m_roundingTimeout.setInterval(500);
    connect(&m_roundingTimeout, &QTimer::timeout, &m_roundingProcess, &QProcess::kill);
    connect(&m_roundingProcess, &QProcess::errorOccurred, this, [this] { m_roundingTimeout.stop(); });
    connect(&m_roundingProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_roundingTimeout.stop();
        const auto value = QJsonDocument::fromJson(m_roundingProcess.readAllStandardOutput()).object().value("int");
        if (code != 0 || status != QProcess::NormalExit || !value.isDouble() || value.toInt(-1) < 0)
            return;
        if (value.toInt() != m_radius) {
            m_radius = value.toInt();
            emit changed();
        }
    });
    // Omarchy sets the system font through fontconfig; omarchy-font-current
    // reads it the same way.
    if (directory.isEmpty()) {
        QProcess fc;
        fc.start("fc-match", {"monospace", "-f", "%{family}"});
        if (fc.waitForFinished(2000))
            m_fontFamily = QString::fromUtf8(fc.readAllStandardOutput()).section(',', 0, 0).trimmed();
    }
    m_roundingPoll.setInterval(1000);
    connect(&m_roundingPoll, &QTimer::timeout, this, &Theme::refreshRounding);
    if (!m_hyprctl.isEmpty())
        m_roundingPoll.start();
    reload();
}

Theme::~Theme() {
    if (m_roundingProcess.state() != QProcess::NotRunning) {
        m_roundingProcess.kill();
        m_roundingProcess.waitForFinished(1000);
    }
}

void Theme::refreshRounding() {
    if (m_hyprctl.isEmpty() || m_roundingProcess.state() != QProcess::NotRunning)
        return;
    m_roundingProcess.start(m_hyprctl, {"-j", "getoption", "decoration:rounding"});
    m_roundingTimeout.start();
}

QString Theme::readAccent(const QString &path) {
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        const QRegularExpression expression(R"re(^\s*accent\s*=\s*["'](#[0-9a-fA-F]{6})["'])re",
                                            QRegularExpression::MultilineOption);
        const auto match = expression.match(QString::fromUtf8(file.readAll()));
        if (match.hasMatch())
            return match.captured(1);
    }
    return "#FFD60A";
}

// Black or white, whichever reads on the accent (WCAG relative luminance).
QString Theme::accentForeground() const {
    const QColor c(m_accent);
    auto linear = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
    const double luminance = .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
    return luminance > .179 ? "black" : "white";
}

// "#rrggbb", Hyprland's "rgba(rrggbbaa)" or "rgb(rrggbb)", or a gradient of
// those ("rgba(..) rgba(..) 45deg"), whose first stop stands in for it.
QString Theme::parseColor(const QString &value) {
    static const QRegularExpression hex(R"(#([0-9a-fA-F]{6}))");
    static const QRegularExpression hypr(R"(rgba?\(([0-9a-fA-F]{6})(?:[0-9a-fA-F]{2})?\))");
    const auto plain = hex.match(value);
    const auto wrapped = hypr.match(value);
    if (wrapped.hasMatch() && (!plain.hasMatch() || wrapped.capturedStart() < plain.capturedStart()))
        return "#" + wrapped.captured(1).toLower();
    return plain.hasMatch() ? "#" + plain.captured(1).toLower() : QString();
}

QVariantMap Theme::readStyle(const QString &themeDirectory, const QString &fontFamily) {
    // Flatten shell.toml to "section.key" -> raw value.
    QHash<QString, QString> toml;
    QFile shell(themeDirectory + "/shell.toml");
    if (shell.open(QIODevice::ReadOnly)) {
        QString section;
        static const QRegularExpression header(R"(^\s*\[([^\]]+)\]\s*$)");
        static const QRegularExpression entry(R"re(^\s*([A-Za-z0-9_-]+)\s*=\s*("([^"]*)"|[^#\s]+))re");
        for (const QString &line : QString::fromUtf8(shell.readAll()).split('\n')) {
            if (const auto match = header.match(line); match.hasMatch())
                section = match.captured(1).trimmed();
            else if (const auto match = entry.match(line); match.hasMatch())
                toml.insert(section + "." + match.captured(1),
                            match.captured(3).isNull() ? match.captured(2) : match.captured(3));
        }
    }

    // Without shell.toml, colors.toml supplies the same base colors.
    QHash<QString, QString> colors;
    QFile colorFile(themeDirectory + "/colors.toml");
    if (colorFile.open(QIODevice::ReadOnly)) {
        static const QRegularExpression line(R"re(^\s*([a-z_]+)\s*=\s*["'](#[0-9a-fA-F]{6})["'])re",
                                             QRegularExpression::MultilineOption);
        auto it = line.globalMatch(QString::fromUtf8(colorFile.readAll()));
        while (it.hasNext()) {
            const auto match = it.next();
            colors.insert(match.captured(1), match.captured(2).toLower());
        }
    }
    const QString background = colors.value("background", "#121212");
    const QString foreground = colors.value("foreground", "#dddddd");

    auto color = [&](const QString &key, const QString &fallback) {
        QString value = toml.value(key);
        if (value.startsWith("hyprland."))
            value = toml.value(value);
        const QString parsed = parseColor(value);
        return parsed.isEmpty() ? fallback : parsed;
    };
    auto number = [&](const QString &key, double fallback) {
        bool ok = false;
        const double value = toml.value(key).toDouble(&ok);
        return ok ? value : fallback;
    };

    const QString text = color("menu.text", foreground);
    return {
        {"background", color("menu.background", background)},
        {"text", text},
        {"muted", colors.value("muted", colors.value("dark_foreground", text))},
        {"accent", colors.value("accent", "#ffd60a")},
        {"border", color("hyprland.active-border", text)},
        {"popupBackground", color("popups.background", background)},
        {"controlColor", color("controls.normal-color", text)},
        {"normalFill", number("controls.normal-fill-alpha", 0.04)},
        {"normalBorderAlpha", number("controls.normal-border-alpha", 0.4)},
        {"borderWidth", number("controls.normal-border-width", 1)},
        {"hoverFill", number("controls.hover-cursor-fill-alpha", 0.08)},
        {"hoverBorderAlpha", number("controls.hover-cursor-border-alpha", 0.25)},
        {"focusFill", number("controls.focus-fill-alpha", 0.08)},
        {"focusBorderAlpha", number("controls.focus-border-alpha", 0.25)},
        {"selectedFill", number("controls.selected-fill-alpha", 0.18)},
        {"pressedFill", number("controls.pressed-fill-alpha", 0.22)},
        {"selectionFill", number("controls.selection-fill-alpha", 0.35)},
        {"fontSize", number("font.base-size", 12)},
        {"spacing", number("spacing.scale", 1.0)},
        {"fontFamily", fontFamily.isEmpty() ? QString("monospace") : fontFamily},
    };
}

void Theme::reload() {
    refreshRounding();
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    // A theme switch replaces symlinks and files, so watch the parents too and
    // re-arm after every change. Watching the nearest existing ancestor means
    // installing Omarchy later works without restarting the app.
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory,
                           m_directory + "/theme", m_directory + "/theme/colors.toml"};
    candidates.removeDuplicates();
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    const auto accent = readAccent(m_directory + "/theme/colors.toml");
    const auto style = readStyle(m_directory + "/theme", m_fontFamily);
    if (accent != m_accent || style != m_style) {
        m_accent = accent;
        m_style = style;
        emit changed();
    }
}

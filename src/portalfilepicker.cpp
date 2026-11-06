#include "portalfilepicker.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRandomGenerator>
#include <QUrl>

namespace {
const char *service = "org.freedesktop.portal.Desktop";
const char *requestInterface = "org.freedesktop.portal.Request";
}

void PortalFilePicker::openImages(const QString &title) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        emit failed(tr("No D-Bus session, so the file picker can't open. Drop files onto the window instead."));
        return;
    }

    // The portal answers on a request object whose path it derives from our
    // bus name and a token we choose. Subscribing before the call means a
    // fast answer can't arrive before we listen for it.
    const QString token = QString("easel%1").arg(QRandomGenerator::global()->generate());
    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    if (!m_handle.isEmpty())
        bus.disconnect(service, m_handle, requestInterface, "Response", this, SLOT(response(uint, QVariantMap)));
    m_handle = QString("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
    bus.connect(service, m_handle, requestInterface, "Response", this, SLOT(response(uint, QVariantMap)));

    QDBusMessage call = QDBusMessage::createMethodCall(service, "/org/freedesktop/portal/desktop",
                                                       "org.freedesktop.portal.FileChooser", "OpenFile");
    call << QString() << title << QVariantMap{{"handle_token", token}, {"multiple", true}};
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError())
            emit failed(tr("The file picker isn't available (%1). Drop files onto the window instead.")
                            .arg(reply.error().message()));
    });
}

void PortalFilePicker::response(uint code, const QVariantMap &results) {
    // 0 is success; 1 means the user cancelled, which needs no message.
    if (code != 0)
        return;
    QStringList paths;
    for (const QString &uri : results.value("uris").toStringList()) {
        const QUrl url(uri);
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }
    if (!paths.isEmpty())
        emit picked(paths);
}

#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

// Opens files through xdg-desktop-portal's FileChooser, the way Omarchy apps
// do, so the picker is the desktop's own rather than Qt's.
class PortalFilePicker : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    void openImages(const QString &title);

signals:
    void picked(const QStringList &paths);
    void failed(const QString &error);

private slots:
    void response(uint code, const QVariantMap &results);

private:
    QString m_handle;
};

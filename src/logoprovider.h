#pragma once

#include <QPainter>
#include <QQuickImageProvider>

// Serves the app mark with its outline painted in a given color, so the
// logo follows the theme: "image://logo/#dcd7ba" is the mark with that
// outline over the canvas in its own colors.
class LogoProvider : public QQuickImageProvider {
public:
    LogoProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override {
        QImage outline(":/assets/logo-outline.png");
        const QImage canvas(":/assets/logo-color.png");
        QImage logo(outline.size(), QImage::Format_ARGB32_Premultiplied);
        logo.fill(Qt::transparent);
        {
            // The outline mask's alpha, filled with the color...
            QPainter painter(&logo);
            painter.drawImage(0, 0, outline);
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(logo.rect(), QColor(id.isEmpty() ? QString("#ffffff") : id));
            // ...then the canvas on top.
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.drawImage(0, 0, canvas);
        }
        if (size)
            *size = logo.size();
        if (requested.isValid())
            return logo.scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return logo;
    }
};

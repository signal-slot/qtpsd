// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdimageitem.h"
#include <QtCore/QBuffer>
#include <QtCore/QtMath>
#include <QtGui/QImageReader>
#include <QtGui/QPainter>
#include <QtPsdCore/QPsdOglwEffect>
#include <QtPsdCore/QPsdSofiEffect>
#include <QtPsdCore/QPsdShadowEffect>

QT_BEGIN_NAMESPACE

static void boxBlurAlpha(QImage &img, int radius)
{
    if (radius <= 0 || img.isNull())
        return;
    img = img.convertToFormat(QImage::Format_ARGB32);
    const int w = img.width();
    const int h = img.height();
    const int size = 2 * radius + 1;

    // Horizontal pass
    QImage temp(w, h, QImage::Format_ARGB32);
    temp.fill(Qt::transparent);
    for (int y = 0; y < h; ++y) {
        const QRgb *src = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        QRgb *dst = reinterpret_cast<QRgb *>(temp.scanLine(y));
        int sum = 0;
        for (int x = -radius; x <= radius; ++x)
            sum += qAlpha(src[qBound(0, x, w - 1)]);
        for (int x = 0; x < w; ++x) {
            const int a = qBound(0, sum / size, 255);
            const QRgb s = src[x];
            dst[x] = qRgba(qRed(s), qGreen(s), qBlue(s), a);
            sum -= qAlpha(src[qBound(0, x - radius, w - 1)]);
            sum += qAlpha(src[qBound(0, x + radius + 1, w - 1)]);
        }
    }
    // Vertical pass
    for (int x = 0; x < w; ++x) {
        int sum = 0;
        for (int y = -radius; y <= radius; ++y)
            sum += qAlpha(reinterpret_cast<const QRgb *>(temp.constScanLine(qBound(0, y, h - 1)))[x]);
        for (int y = 0; y < h; ++y) {
            const int a = qBound(0, sum / size, 255);
            const QRgb s = reinterpret_cast<const QRgb *>(temp.constScanLine(y))[x];
            reinterpret_cast<QRgb *>(img.scanLine(y))[x] = qRgba(qRed(s), qGreen(s), qBlue(s), a);
            sum -= qAlpha(reinterpret_cast<const QRgb *>(temp.constScanLine(qBound(0, y - radius, h - 1)))[x]);
            sum += qAlpha(reinterpret_cast<const QRgb *>(temp.constScanLine(qBound(0, y + radius + 1, h - 1)))[x]);
        }
    }
}

QPsdImageItem::QPsdImageItem(const QModelIndex &index, const QPsdImageLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QGraphicsItem *parent)
    : QPsdAbstractItem(index, psdData, maskItem, group, parent)
{
}

void QPsdImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    
    setMask(painter);

    const auto *layer = this->layer<QPsdImageLayerItem>();
    QRect r = QRect{{0, 0}, layer->rect().size()};
    QImage image = layer->image();

    QImage linkedImage = layer->linkedImage();
    if (!linkedImage.isNull()) {
        image = linkedImage.scaled(r.width(), r.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        r = QRect((r.width() - image.width()) / 2, (r.height() - image.height()) / 2, image.width(), image.height());
    }

    const auto effects = layer->effects();
    
    // First pass: Draw drop shadows (they go behind the layer)
    for (const auto &effect : effects) {
        if (effect.canConvert<QPsdShadowEffect>()) {
            const auto dropShadow = effect.value<QPsdShadowEffect>();
            
            // Get shadow parameters (values are in 16.16 fixed-point format)
            const qreal angleDeg = dropShadow.angle() / 65536.0;
            const qreal distance = dropShadow.distance() / 65536.0;
            const qreal blur = dropShadow.blur() / 65536.0;
            const auto color = QColor(dropShadow.nativeColor());
            const auto opacity = dropShadow.opacity();

            // Calculate offset from angle and distance
            // PSD angles: 0° is right, 90° is up, 180° is left, 270° is down
            const qreal angleRad = qDegreesToRadians(angleDeg);
            const QPointF offset(
                qCos(angleRad) * distance,
                -qSin(angleRad) * distance  // Negate for PSD coordinate system
            );
            
            // Create shadow from the original image
            QImage shadowImage = image.convertToFormat(QImage::Format_ARGB32);
            
            // Fill shadow with shadow color while preserving alpha
            for (int y = 0; y < shadowImage.height(); ++y) {
                QRgb *scanLine = reinterpret_cast<QRgb *>(shadowImage.scanLine(y));
                for (int x = 0; x < shadowImage.width(); ++x) {
                    const int alpha = qAlpha(scanLine[x]);
                    if (alpha > 0) {
                        scanLine[x] = qRgba(color.red(), color.green(), color.blue(), alpha);
                    }
                }
            }
            
            // Apply blur (3-pass box blur approximates gaussian)
            if (blur > 0) {
                const int blurRadius = qMax(1, static_cast<int>(blur));
                // Expand the image to accommodate blur spread
                const int margin = blurRadius * 2;
                QImage expanded(shadowImage.width() + margin * 2,
                                shadowImage.height() + margin * 2,
                                QImage::Format_ARGB32);
                expanded.fill(Qt::transparent);
                QPainter ep(&expanded);
                ep.drawImage(margin, margin, shadowImage);
                ep.end();
                // 3-pass box blur ≈ gaussian
                boxBlurAlpha(expanded, blurRadius);
                boxBlurAlpha(expanded, blurRadius);
                boxBlurAlpha(expanded, blurRadius);
                shadowImage = expanded;
                // Adjust draw position for the expanded margin
                painter->save();
                painter->setOpacity(layer->opacity() * opacity);
                painter->drawImage(r.topLeft() + offset.toPoint() - QPoint(margin, margin), shadowImage);
                painter->restore();
            } else {
                painter->save();
                painter->setOpacity(layer->opacity() * opacity);
                painter->drawImage(r.translated(offset.toPoint()), shadowImage);
                painter->restore();
            }
        }
    }
    
    // 1.5 pass: Draw outer glow behind the layer but in front of drop shadow
    for (const auto &effect : effects) {
        if (effect.canConvert<QPsdOglwEffect>() && !effect.canConvert<QPsdShadowEffect>()) {
            const auto glow = effect.value<QPsdOglwEffect>();
            const qreal glowBlur = glow.blur() / 65536.0;  // 16.16 fixed-point
            const auto glowColor = QColor(glow.nativeColor());
            const auto glowOpacity = glow.opacity();

            if (glowBlur > 0) {
                // Create glow from the original image alpha
                const int blurRadius = qMax(1, static_cast<int>(glowBlur));
                const int margin = blurRadius * 2;
                QImage glowImage(image.width() + margin * 2,
                                 image.height() + margin * 2,
                                 QImage::Format_ARGB32);
                glowImage.fill(Qt::transparent);

                // Extract alpha from original and fill with glow color
                QImage alphaSource = image.convertToFormat(QImage::Format_ARGB32);
                for (int y = 0; y < alphaSource.height(); ++y) {
                    QRgb *scanLine = reinterpret_cast<QRgb *>(alphaSource.scanLine(y));
                    for (int x = 0; x < alphaSource.width(); ++x) {
                        const int alpha = qAlpha(scanLine[x]);
                        scanLine[x] = qRgba(glowColor.red(), glowColor.green(), glowColor.blue(), alpha);
                    }
                }
                QPainter gp(&glowImage);
                gp.drawImage(margin, margin, alphaSource);
                gp.end();

                // Blur to create glow spread
                boxBlurAlpha(glowImage, blurRadius);
                boxBlurAlpha(glowImage, blurRadius);
                boxBlurAlpha(glowImage, blurRadius);

                painter->save();
                painter->setCompositionMode(QtPsdGui::compositionMode(glow.blendMode()));
                painter->setOpacity(layer->opacity() * glowOpacity);
                painter->drawImage(r.topLeft() - QPoint(margin, margin), glowImage);
                painter->restore();
            }
        }
    }

    // Second pass: Apply other effects to the image
    for (const auto &effect : effects) {
        if (effect.canConvert<QPsdSofiEffect>() && !effect.canConvert<QPsdOglwEffect>()) {
            const auto sofi = effect.value<QPsdSofiEffect>();
            QColor color(sofi.nativeColor());
            color.setAlphaF(sofi.opacity());
            switch (sofi.blendMode()) {
            case QPsdBlend::Mode::Normal: {
                // override pixels in the image with the color and opacity
                QPainter p(&image);
                p.setCompositionMode(QPainter::CompositionMode_SourceIn);
                p.fillRect(image.rect(), color);
                p.end();
                break; }
            default:
                qWarning() << sofi.blendMode() << "not supported blend mode";
                break;
            }
        }
    }

    painter->setCompositionMode(QtPsdGui::compositionMode(layer->record().blendMode()));
    // Apply both opacity and fill opacity to the layer content
    // In Photoshop: opacity affects everything, fill opacity affects only layer pixels (not effects)
    // Effects (drop shadow etc.) were already drawn above with just opacity
    painter->setOpacity(layer->opacity() * layer->fillOpacity());
    // Finally, draw the layer itself
    painter->drawImage(r, image);

    const auto *gradient = layer->gradient();
    if (gradient) {
        painter->setOpacity(0.71);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(*gradient));
        painter->drawRect(r);
    }
}

QT_END_NAMESPACE

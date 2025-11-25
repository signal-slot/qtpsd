// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdimageitem.h"
#include <QtCore/QBuffer>
#include <QtCore/QtMath>
#include <QtGui/QImageReader>
#include <QtGui/QPainter>
#include <QtPsdCore/QPsdSofiEffect>
#include <QtPsdCore/QPsdShadowEffect>

QT_BEGIN_NAMESPACE

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
            
            // Get shadow parameters
            const auto angle = dropShadow.angle();
            const auto distance = dropShadow.distance();
            const auto blur = dropShadow.blur();
            const auto color = QColor(dropShadow.nativeColor());
            const auto opacity = dropShadow.opacity();
            
            // Calculate offset from angle and distance
            // PSD angles: 0° is right, 90° is up, 180° is left, 270° is down
            // Qt angles: 0° is right, 90° is down, 180° is left, 270° is up
            // So we need to negate the y-component
            const qreal angleRad = qDegreesToRadians(qreal(angle));
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
            
            // TODO: Apply gaussian blur based on blur parameter
            // For now, just use the shadow as-is
            Q_UNUSED(blur);
            
            // Draw shadow behind the layer
            painter->save();
            painter->setOpacity(opacity);
            painter->drawImage(r.translated(offset.toPoint()), shadowImage);
            painter->restore();
        }
    }
    
    // Second pass: Apply other effects to the image
    for (const auto &effect : effects) {
        if (effect.canConvert<QPsdSofiEffect>()) {
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

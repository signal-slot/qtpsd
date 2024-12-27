// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "psdimageitem.h"
#include <QtCore/QBuffer>
#include <QtGui/QImageReader>
#include <QtGui/QPainter>
#include <QtPsdCore/QPsdSofiEffect>

PsdImageItem::PsdImageItem(const QPsdImageLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, QWidget *parent)
    : PsdAbstractItem(psdData, maskItem, parent)
{}

void PsdImageItem::paintEvent(QPaintEvent *event)
{
    PsdAbstractItem::paintEvent(event);

    QPainter painter(this);
    setMask(&painter);

    const auto *layer = this->layer<QPsdImageLayerItem>();
    QRect r = rect();
    QImage image = layer->image();

    QImage linkedImage = layer->linkedImage();
    if (!linkedImage.isNull()) {
        image = linkedImage.scaled(width(), height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        r = QRect((width() - image.width()) / 2, (height() - image.height()) / 2, image.width(), image.height());
    }

    const auto effects = layer->effects();
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
        // if (effect->type() == QPsdLayerEffect::Type::DropShadow) {
        //     const auto *dropShadow = static_cast<const QPsdDropShadowEffect *>(effect);
        //     QImage shadow = dropShadow->image();
        //     if (!shadow.isNull()) {
        //         painter.drawImage(QRect(r.topLeft() + dropShadow->offset(), shadow.size()), shadow);
        //     }
        // }
    }
    painter.drawImage(r, image);

    const auto *gradient = layer->gradient();
    if (gradient) {
        painter.setOpacity(0.71);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(*gradient));
        painter.drawRect(rect());
    }
}

// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdadjustmentitem.h"

#include <QtPsdGui/qpsdadjustments.h>
#include <QtPsdGui/QPsdImageLayerItem>
#include <QtPsdGui/QPsdShapeLayerItem>

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QPainter>

QT_BEGIN_NAMESPACE

class QPsdAdjustmentItem::Private
{
public:
    QSize canvasSize;
    const QPsdAbstractLayerItem *clipBase = nullptr;
    QImage weightMask;      // document coordinates, Grayscale8; null = full weight
    bool weightMaskBuilt = false;
    bool warnedUnsupported = false;
};

QPsdAdjustmentItem::QPsdAdjustmentItem(const QModelIndex &index, const QPsdAdjustmentLayerItem *layer,
                                       const QPsdAbstractLayerItem *maskItem,
                                       const QMap<quint32, QString> group,
                                       const QSize &canvasSize, QGraphicsItem *parent)
    : QPsdAbstractItem(index, layer, maskItem, group, parent)
    , d(new Private)
{
    d->canvasSize = canvasSize;
    d->clipBase = maskItem;
    // The item spans the whole canvas; it must not swallow clicks or
    // selection meant for the layers underneath
    setFlag(ItemIsSelectable, false);
    setAcceptedMouseButtons(Qt::NoButton);
}

QPsdAdjustmentItem::~QPsdAdjustmentItem() = default;

QRectF QPsdAdjustmentItem::boundingRect() const
{
    // Adjustment layers affect the whole canvas regardless of their own
    // (usually empty) layer rect.
    return mapRectFromScene(QRectF(QPointF(0, 0), QSizeF(d->canvasSize)));
}

namespace {

QImage buildDocumentWeightMask(const QPsdAdjustmentLayerItem *layer, const QSize &canvasSize)
{
    const QImage mask = layer->layerMask();
    const QRect maskRect = layer->layerMaskRect();
    const quint8 defaultColor = layer->layerMaskDefaultColor();
    const qreal density = layer->layerMaskDensity() / 255.0;
    const bool hasMaskImage = !mask.isNull() && maskRect.isValid() && !maskRect.isEmpty();

    if (!hasMaskImage && defaultColor == 255)
        return QImage(); // full weight everywhere

    QImage doc(canvasSize, QImage::Format_Grayscale8);
    doc.fill(defaultColor);
    if (hasMaskImage) {
        QPainter p(&doc);
        p.drawImage(maskRect.topLeft(), mask.convertToFormat(QImage::Format_Grayscale8));
    }
    if (density < 1.0) {
        // Reduced mask density fades the mask toward "no mask" (full weight)
        for (int y = 0; y < doc.height(); ++y) {
            uchar *line = doc.scanLine(y);
            for (int x = 0; x < doc.width(); ++x)
                line[x] = 255 - qRound(density * (255 - line[x]));
        }
    }
    return doc;
}

// Coverage of the clipping-mask base layer (the layer the adjustment is
// clipped to): the adjustment only affects pixels the base covers
QImage clipBaseCoverage(const QPsdAbstractLayerItem *base, const QSize &canvasSize)
{
    QImage doc(canvasSize, QImage::Format_Grayscale8);
    doc.fill(0);
    QPainter p(&doc);
    const QRect rect = base->rect();

    QPsdAbstractLayerItem::PathInfo pathInfo;
    if (base->type() == QPsdAbstractLayerItem::Shape)
        pathInfo = reinterpret_cast<const QPsdShapeLayerItem *>(base)->pathInfo();
    else
        pathInfo = base->vectorMask();

    if (pathInfo.type != QPsdAbstractLayerItem::PathInfo::None) {
        QPainterPath path;
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::Rectangle:
            path.addRect(pathInfo.rect);
            break;
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
            path.addRoundedRect(pathInfo.rect, pathInfo.radius, pathInfo.radius);
            break;
        default:
            path = pathInfo.path;
            break;
        }
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(rect.topLeft());
        p.fillPath(path, Qt::white);
        return doc;
    }

    const QImage transparency = base->transparencyMask();
    if (!transparency.isNull()) {
        p.drawImage(rect.topLeft(), transparency);
        return doc;
    }

    if (base->type() == QPsdAbstractLayerItem::Image) {
        const QImage image = reinterpret_cast<const QPsdImageLayerItem *>(base)->image();
        if (!image.isNull() && image.hasAlphaChannel()) {
            const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
            QImage alpha(argb.size(), QImage::Format_Grayscale8);
            for (int y = 0; y < argb.height(); ++y) {
                const QRgb *src = reinterpret_cast<const QRgb *>(argb.constScanLine(y));
                uchar *dst = alpha.scanLine(y);
                for (int x = 0; x < argb.width(); ++x)
                    dst[x] = qAlpha(src[x]);
            }
            p.drawImage(rect.topLeft(), alpha);
            return doc;
        }
    }

    p.fillRect(rect, Qt::white);
    return doc;
}

} // namespace

void QPsdAdjustmentItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const auto *adj = layer<QPsdAdjustmentLayerItem>();

    if (!d->weightMaskBuilt) {
        d->weightMask = buildDocumentWeightMask(adj, d->canvasSize);
        if (d->clipBase) {
            const QImage coverage = clipBaseCoverage(d->clipBase, d->canvasSize);
            if (d->weightMask.isNull()) {
                d->weightMask = coverage;
            } else {
                for (int y = 0; y < d->weightMask.height(); ++y) {
                    uchar *w = d->weightMask.scanLine(y);
                    const uchar *c = coverage.constScanLine(y);
                    for (int x = 0; x < d->weightMask.width(); ++x)
                        w[x] = (w[x] * c[x]) / 255;
                }
            }
        }
        d->weightMaskBuilt = true;
    }

    painter->save();

    const QTransform xf = painter->combinedTransform();
    const QRect deviceRect = xf.mapRect(boundingRect()).toAlignedRect();
    const QTransform sceneToDevice = sceneTransform().inverted() * xf;

    auto deviceWeightMask = [&](const QRect &clipped) -> QImage {
        if (d->weightMask.isNull())
            return QImage();
        const QRect docRect = sceneToDevice.inverted().mapRect(QRectF(clipped))
                                  .toAlignedRect().intersected(d->weightMask.rect());
        if (docRect.isEmpty())
            return QImage();
        QImage region = d->weightMask.copy(docRect);
        if (region.size() != clipped.size())
            region = region.scaled(clipped.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        return region;
    };

    auto applyAndWriteBack = [&](QImage &region, const QRect &clipped) {
        if (!QtPsdGui::applyAdjustmentToImage(region, adj, deviceWeightMask(clipped))) {
            if (!d->warnedUnsupported) {
                qWarning("QPsdAdjustmentItem: adjustment type '%s' is not implemented; layer '%s' is ignored",
                         adj->adjustmentKey().constData(), qPrintable(adj->name()));
                d->warnedUnsupported = true;
            }
            return;
        }
        painter->resetTransform();
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->setOpacity(1.0);
        painter->drawImage(clipped.topLeft(), region);
    };

    // Fast path: QImage paint device — direct pixel access
    if (QImage *backbuffer = dynamic_cast<QImage *>(painter->device())) {
        const QRect clipped = deviceRect.intersected(backbuffer->rect());
        if (!clipped.isEmpty()) {
            QImage region = backbuffer->copy(clipped);
            applyAndWriteBack(region, clipped);
        }
        painter->restore();
        return;
    }

    // QPixmap paint device — e.g. inside an enclosing QGraphicsEffect's
    // subtree rasterization
    if (QPixmap *pixmapDevice = dynamic_cast<QPixmap *>(painter->device())) {
        const QRect clipped = deviceRect.intersected(pixmapDevice->rect());
        if (!clipped.isEmpty()) {
            QImage region = pixmapDevice->copy(clipped).toImage();
            applyAndWriteBack(region, clipped);
        }
        painter->restore();
        return;
    }

    // OpenGL path: read back the framebuffer via glReadPixels
    if (QOpenGLContext *ctx = QOpenGLContext::currentContext()) {
        const QSize devSize(painter->device()->width(), painter->device()->height());
        const QRect clipped = deviceRect.intersected(QRect(QPoint(0, 0), devSize));
        if (!clipped.isEmpty()) {
            painter->beginNativePainting();
            QOpenGLFunctions *f = ctx->functions();
            QImage readback(clipped.width(), clipped.height(), QImage::Format_RGBA8888);
            f->glReadPixels(clipped.x(),
                            devSize.height() - clipped.y() - clipped.height(),
                            clipped.width(), clipped.height(),
                            GL_RGBA, GL_UNSIGNED_BYTE, readback.bits());
            painter->endNativePainting();
            QImage region = readback.mirrored(false, true);
            applyAndWriteBack(region, clipped);
        }
        painter->restore();
        return;
    }

    // Unknown paint device (e.g. direct widget painting without a readable
    // backbuffer): the adjustment cannot be applied
    if (!d->warnedUnsupported) {
        qWarning("QPsdAdjustmentItem: paint device does not support readback; adjustment '%s' skipped",
                 adj->adjustmentKey().constData());
        d->warnedUnsupported = true;
    }
    painter->restore();
}

QT_END_NAMESPACE

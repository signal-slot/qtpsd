// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdshapeitem.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtPsdCore/QPsdVectorMaskSetting>
#include <QtPsdGui/QPsdBorder>
#include <QtPsdGui/QPsdPatternFill>

QT_BEGIN_NAMESPACE

QPsdShapeItem::QPsdShapeItem(const QModelIndex &index, const QPsdShapeLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QGraphicsItem *parent)
    : QPsdAbstractItem(index, psdData, maskItem, group, parent)
{}

void QPsdShapeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const auto *layer = this->layer<QPsdShapeLayerItem>();

    setMask(painter);
    const auto compositionMode = groupCompositionMode() != QPainter::CompositionMode_SourceOver
        ? groupCompositionMode()
        : QtPsdGui::compositionMode(layer->record().blendMode());
    painter->setCompositionMode(compositionMode);
    // Apply both opacity and fill opacity for shape content
    // In Photoshop, fill opacity affects only the layer content, not effects
    painter->setOpacity(abstractLayer()->opacity() * abstractLayer()->fillOpacity());
    painter->setRenderHint(QPainter::Antialiasing);

    // Check for raster layer mask - if present, render shape into a temp image
    // so we can apply per-pixel alpha masking (same technique as QPsdImageItem)
    const QImage layerMask = layer->layerMask();
    const bool hasMask = !layerMask.isNull();

    QImage tempImage;
    QPainter tempPainterObj;
    QPainter *p = painter;

    if (hasMask) {
        tempImage = QImage(layer->rect().size(), QImage::Format_ARGB32);
        tempImage.fill(Qt::transparent);
        tempPainterObj.begin(&tempImage);
        tempPainterObj.setRenderHint(QPainter::Antialiasing);
        p = &tempPainterObj;
    }

    const auto *gradient = layer->gradient();
    const auto *border = layer->border();
    const auto *patternFill = layer->patternFill();
    const auto pathInfo = layer->pathInfo();
    if (gradient) {
        p->setPen(Qt::NoPen);
        p->setBrush(QBrush(*gradient));
    } else if (border) {
        p->setPen(QPen(border->color(), border->size()));
    } else if (patternFill) {
        if (hasMask)
            tempPainterObj.end();
        const auto record = layer->record();
        const auto patt = record.additionalLayerInformation().value("Patt");
        // TODO: find the pattern from below
        // However, there is no way to access it from here yet
        // parser.layerAndMaskInformation().additionalLayerInformation().value("Patt");
        return;
    } else {
        p->setPen(layer->pen());
        p->setBrush(layer->brush());
    }

    const auto strokeAlignment = layer->strokeAlignment();
    if (strokeAlignment == QPsdShapeLayerItem::StrokeInside && p->pen().style() != Qt::NoPen) {
        // For "inside" stroke: draw fill first, then clip to path and draw stroke with 2x width
        QPen strokePen = p->pen();
        QBrush fillBrush = p->brush();

        // Draw fill without stroke
        p->setPen(Qt::NoPen);
        p->setBrush(fillBrush);
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::Rectangle:
            p->drawRect(pathInfo.rect);
            break;
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
            p->drawRoundedRect(pathInfo.rect, pathInfo.radius, pathInfo.radius);
            break;
        default:
            p->drawPath(pathInfo.path);
            break;
        }

        // Draw stroke clipped to inside of the path
        p->save();
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::Rectangle: {
            QPainterPath clipPath;
            clipPath.addRect(pathInfo.rect);
            p->setClipPath(clipPath, Qt::IntersectClip);
            break;
        }
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
            QPainterPath clipPath;
            clipPath.addRoundedRect(pathInfo.rect, pathInfo.radius, pathInfo.radius);
            p->setClipPath(clipPath, Qt::IntersectClip);
            break;
        }
        default:
            p->setClipPath(pathInfo.path, Qt::IntersectClip);
            break;
        }
        strokePen.setWidthF(strokePen.widthF() * 2.0);
        if (strokePen.style() == Qt::CustomDashLine) {
            QVector<qreal> pattern = strokePen.dashPattern();
            for (int i = 0; i < pattern.size(); ++i) {
                pattern[i] /= 2.0;
            }
            strokePen.setDashPattern(pattern);
            strokePen.setDashOffset(strokePen.dashOffset() / 2.0);
        }
        p->setPen(strokePen);
        p->setBrush(Qt::NoBrush);
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::Rectangle:
            p->drawRect(pathInfo.rect);
            break;
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
            p->drawRoundedRect(pathInfo.rect, pathInfo.radius, pathInfo.radius);
            break;
        default:
            p->drawPath(pathInfo.path);
            break;
        }
        p->restore();
    } else {
        // For "center" or "outside" stroke (or no stroke): draw as-is
        const auto dw = p->pen().widthF() / 2.0;
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::Rectangle:
            p->drawRect(pathInfo.rect.adjusted(-dw, -dw, dw, dw));
            break;
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
            p->drawRoundedRect(pathInfo.rect.adjusted(-dw, -dw, dw, dw), pathInfo.radius, pathInfo.radius);
            break;
        default:
            p->drawPath(pathInfo.path);
            break;
        }
    }

    // Apply raster layer mask and draw result
    if (hasMask) {
        tempPainterObj.end();

        const QRect maskRect = layer->layerMaskRect();
        const QRect layerRect = layer->rect();
        const int defaultColor = layer->layerMaskDefaultColor();

        for (int y = 0; y < tempImage.height(); ++y) {
            QRgb *scanLine = reinterpret_cast<QRgb *>(tempImage.scanLine(y));
            for (int x = 0; x < tempImage.width(); ++x) {
                const int maskX = (layerRect.x() + x) - maskRect.x();
                const int maskY = (layerRect.y() + y) - maskRect.y();

                int maskValue = defaultColor;
                if (maskX >= 0 && maskX < layerMask.width()
                    && maskY >= 0 && maskY < layerMask.height()) {
                    maskValue = qGray(layerMask.pixel(maskX, maskY));
                }

                const int alpha = qAlpha(scanLine[x]);
                const int newAlpha = (alpha * maskValue) / 255;
                scanLine[x] = qRgba(qRed(scanLine[x]), qGreen(scanLine[x]), qBlue(scanLine[x]), newAlpha);
            }
        }

        painter->drawImage(0, 0, tempImage);
    }
}

QT_END_NAMESPACE

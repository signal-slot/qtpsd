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

    // painter.drawImage(0, 0, layer->image());
    // painter.setOpacity(0.5);

    const auto *gradient = layer->gradient();
    const auto *border = layer->border();
    const auto *patternFill = layer->patternFill();
    const auto pathInfo = layer->pathInfo();
    if (gradient) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(*gradient));
    } else if (border) {
        painter->setPen(QPen(border->color(), border->size()));
    } else if (patternFill) {
        const auto record = layer->record();
        const auto patt = record.additionalLayerInformation().value("Patt");
        // TODO: find the pattern from below
        // However, there is no way to access it from here yet
        // parser.layerAndMaskInformation().additionalLayerInformation().value("Patt");
        return;
    } else {
        painter->setPen(layer->pen());
        painter->setBrush(layer->brush());
    }

    const auto strokeAlignment = layer->strokeAlignment();
    if (strokeAlignment == QPsdShapeLayerItem::StrokeInside && painter->pen().style() != Qt::NoPen) {
        // For "inside" stroke: draw fill first, then clip to path and draw stroke with 2x width
        QPen strokePen = painter->pen();
        QBrush fillBrush = painter->brush();

        // Draw fill without stroke
        painter->setPen(Qt::NoPen);
        painter->setBrush(fillBrush);
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::Rectangle:
            painter->drawRect(pathInfo.rect);
            break;
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
            painter->drawRoundedRect(pathInfo.rect, pathInfo.radius, pathInfo.radius);
            break;
        default:
            painter->drawPath(pathInfo.path);
            break;
        }

        // Draw stroke clipped to inside of the path
        painter->save();
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::Rectangle: {
            QPainterPath clipPath;
            clipPath.addRect(pathInfo.rect);
            painter->setClipPath(clipPath, Qt::IntersectClip);
            break;
        }
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
            QPainterPath clipPath;
            clipPath.addRoundedRect(pathInfo.rect, pathInfo.radius, pathInfo.radius);
            painter->setClipPath(clipPath, Qt::IntersectClip);
            break;
        }
        default:
            painter->setClipPath(pathInfo.path, Qt::IntersectClip);
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
        painter->setPen(strokePen);
        painter->setBrush(Qt::NoBrush);
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::Rectangle:
            painter->drawRect(pathInfo.rect);
            break;
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
            painter->drawRoundedRect(pathInfo.rect, pathInfo.radius, pathInfo.radius);
            break;
        default:
            painter->drawPath(pathInfo.path);
            break;
        }
        painter->restore();
    } else {
        // For "center" or "outside" stroke (or no stroke): draw as-is
        const auto dw = painter->pen().widthF() / 2.0;
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::Rectangle:
            painter->drawRect(pathInfo.rect.adjusted(-dw, -dw, dw, dw));
            break;
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
            painter->drawRoundedRect(pathInfo.rect.adjusted(-dw, -dw, dw, dw), pathInfo.radius, pathInfo.radius);
            break;
        default:
            painter->drawPath(pathInfo.path);
            break;
        }
    }
}

QT_END_NAMESPACE

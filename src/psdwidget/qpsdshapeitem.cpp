// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdshapeitem.h"

#include <QtGui/QPainter>
#include <QtPsdCore/QPsdVectorMaskSetting>
#include <QtPsdGui/QPsdBorder>
#include <QtPsdGui/QPsdPatternFill>

QT_BEGIN_NAMESPACE

QPsdShapeItem::QPsdShapeItem(const QModelIndex &index, const QPsdShapeLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QWidget *parent)
    : QPsdAbstractItem(index, psdData, maskItem, group, parent)
{}

void QPsdShapeItem::paintEvent(QPaintEvent *event)
{
    QPsdAbstractItem::paintEvent(event);

    const auto *layer = this->layer<QPsdShapeLayerItem>();

    QPainter painter(this);
    setMask(&painter);
    painter.setOpacity(abstractLayer()->opacity());
    painter.setRenderHint(QPainter::Antialiasing);

    // painter.drawImage(0, 0, layer->image());
    // painter.setOpacity(0.5);

    const auto *gradient = layer->gradient();
    const auto *border = layer->border();
    const auto *patternFill = layer->patternFill();
    const auto pathInfo = layer->pathInfo();
    if (gradient) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(*gradient));
    } else if (border) {
        painter.setPen(QPen(border->color(), border->size()));
    } else if (patternFill) {
        const auto record = layer->record();
        const auto patt = record.additionalLayerInformation().value("Patt"_ba);
        // TODO: find the pattern from below
        // However, there is no way to access it from here yet
        // parser.layerAndMaskInformation().additionalLayerInformation().value("Patt");
        return;
    } else {
        painter.setPen(layer->pen());
        painter.setBrush(layer->brush());
    }

    const auto dw = painter.pen().widthF() / 2.0;
    switch (pathInfo.type) {
    case QPsdAbstractLayerItem::PathInfo::Rectangle:
        painter.drawRect(pathInfo.rect.adjusted(-dw, -dw, dw, dw));
        break;
    case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
        painter.drawRoundedRect(pathInfo.rect.adjusted(-dw, -dw, dw, dw), pathInfo.radius, pathInfo.radius);
        break;
    default:
        painter.drawPath(pathInfo.path);
        break;
    }
}

QT_END_NAMESPACE

// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "psdabstractitem.h"
#include "psdfolderitem.h"

#include <QtGui/QPainter>

class PsdAbstractItem::Private
{
public:
    Private(const QPsdAbstractLayerItem *layer, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, PsdAbstractItem *parent);

private:
    PsdAbstractItem *q;
public:
    const QPsdAbstractLayerItem *layer = nullptr;
    const QPsdAbstractLayerItem *maskItem = nullptr;
    const QMap<quint32, QString> group;
};

PsdAbstractItem::Private::Private(const QPsdAbstractLayerItem *layer, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, PsdAbstractItem *parent)
    : q(parent)
    , layer(layer), maskItem(maskItem), group(group)
{
    q->setVisible(layer->isVisible());
    q->setGeometry(layer->rect());
}

PsdAbstractItem::PsdAbstractItem(const QPsdAbstractLayerItem *layer, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QWidget *parent)
    : QWidget(parent)
    , d(new Private(layer, maskItem, group, this))
{}

PsdAbstractItem::~PsdAbstractItem() = default;

quint32 PsdAbstractItem::id() const
{
    return d->layer->id();
}

QString PsdAbstractItem::name() const
{
    return d->layer->name();
}

void PsdAbstractItem::setMask(QPainter *painter) const
{
    const QPsdAbstractLayerItem *layer = d->layer;
    while (layer) {
        // qDebug() << layer->name() << layer->vectorMask().elementCount();
        if (layer->vectorMask().type != QPsdAbstractLayerItem::PathInfo::None) {
            painter->setClipPath(layer->vectorMask().path, Qt::IntersectClip);
        }
        layer = layer->parent();
    }
    if (d->layer && d->maskItem) {
        QPixmap pixmap(size());
        pixmap.fill(Qt::transparent);
        const auto maskItem = d->maskItem;
        const QImage maskImage = maskItem->transparencyMask();
        if (!maskImage.size().isEmpty() && maskItem->rect().isValid()) {
            const auto intersected = maskItem->rect().intersected(geometry());
            QPainter p(&pixmap);
            p.drawImage(intersected.translated(-x(), -y()), maskImage, intersected.translated(-maskItem->rect().x(), -maskItem->rect().y()));
            p.end();

            painter->setClipRegion(QRegion(pixmap.createHeuristicMask()), Qt::IntersectClip);
        }
    }
}

const QPsdAbstractLayerItem *PsdAbstractItem::abstractLayer() const
{
    return d->layer;
}

QMap<quint32, QString> PsdAbstractItem::groupMap() const
{
    return d->group;
}

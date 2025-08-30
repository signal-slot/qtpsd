// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdabstractitem.h"
#include "qpsdwidgettreeitemmodel.h"

#include <QtGui/QPainter>

QT_BEGIN_NAMESPACE

class QPsdAbstractItem::Private
{
public:
    Private(const QModelIndex &index, const QPsdAbstractLayerItem *layer, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QPsdAbstractItem *parent);

private:
    QPsdAbstractItem *q;
public:
    const QPsdAbstractLayerItem *layer = nullptr;
    const QPsdAbstractLayerItem *maskItem = nullptr;
    const QMap<quint32, QString> group;
    const QModelIndex index;
};

QPsdAbstractItem::Private::Private(const QModelIndex &index, const QPsdAbstractLayerItem *layer, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QPsdAbstractItem *parent)
    : q(parent)
    , layer(layer), maskItem(maskItem), group(group), index(index)
{
    q->setVisible(layer->isVisible());
}

QPsdAbstractItem::QPsdAbstractItem(const QModelIndex &index, const QPsdAbstractLayerItem *layer, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , d(new Private(index, layer, maskItem, group, this))
{
    auto pos = d->layer->rect().topLeft();
    if (parent) {
        pos -= parent->pos().toPoint();
    }
    setPos(pos);
    setFlags(ItemIsSelectable);
}

QPsdAbstractItem::~QPsdAbstractItem() = default;

quint32 QPsdAbstractItem::id() const
{
    return d->layer->id();
}

QString QPsdAbstractItem::name() const
{
    return d->layer->name();
}

void QPsdAbstractItem::setMask(QPainter *painter) const
{
    QModelIndex index = d->index;
    const auto *model = dynamic_cast<const QPsdWidgetTreeItemModel *>(index.model());
    while (index.isValid()) {
        const QPsdAbstractLayerItem *layer = model->layerItem(index);
        if (layer->vectorMask().type != QPsdAbstractLayerItem::PathInfo::None) {
            painter->setClipPath(layer->vectorMask().path, Qt::IntersectClip);
        }
        index = model->parent(index);
    }
    if (d->layer && d->maskItem) {
        QPixmap pixmap(d->layer->rect().size());
        pixmap.fill(Qt::transparent);
        const auto maskItem = d->maskItem;
        const QImage maskImage = maskItem->transparencyMask();
        if (!maskImage.size().isEmpty() && maskItem->rect().isValid()) {
            const auto intersected = maskItem->rect().intersected(d->layer->rect());
            QPainter p(&pixmap);
            p.drawImage(intersected.translated(-x(), -y()), maskImage, intersected.translated(-maskItem->rect().x(), -maskItem->rect().y()));
            p.end();

            painter->setClipRegion(QRegion(pixmap.createHeuristicMask()), Qt::IntersectClip);
        }
    }
}

const QPsdAbstractLayerItem *QPsdAbstractItem::abstractLayer() const
{
    return d->layer;
}

QMap<quint32, QString> QPsdAbstractItem::groupMap() const
{
    return d->group;
}

QModelIndex QPsdAbstractItem::modelIndex() const
{
    return d->index;
}

QRectF QPsdAbstractItem::boundingRect() const
{
    return QRectF{QPointF { 0, 0 }, d->layer->rect().size()};
}

QT_END_NAMESPACE

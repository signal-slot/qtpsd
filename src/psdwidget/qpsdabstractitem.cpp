// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdabstractitem.h"
#include "qpsdwidgettreeitemmodel.h"

#include <QtGui/QPainter>
#include <QtPsdGui/QPsdShapeLayerItem>

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
    QPainter::CompositionMode groupCompositionMode = QPainter::CompositionMode_SourceOver;
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
    const QPointF currentTopLeft = d->layer->rect().topLeft();
    while (index.isValid()) {
        const QPsdAbstractLayerItem *layer = model->layerItem(index);
        const auto vmask = layer->vectorMask();
        if (vmask.type != QPsdAbstractLayerItem::PathInfo::None) {
            // Transform the path from parent's local coordinates to current item's local coordinates
            const QPointF offset = layer->rect().topLeft() - currentTopLeft;
            QPainterPath clipPath;
            switch (vmask.type) {
            case QPsdAbstractLayerItem::PathInfo::Rectangle:
                clipPath.addRect(vmask.rect);
                break;
            case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
                clipPath.addRoundedRect(vmask.rect, vmask.radius, vmask.radius);
                break;
            default:
                clipPath = vmask.path;
                break;
            }
            painter->setClipPath(clipPath.translated(offset), Qt::IntersectClip);
        }
        index = model->parent(index);
    }
    if (d->layer && d->maskItem) {
        const auto maskItem = d->maskItem;
        // Get mask path - for shape layers, use pathInfo() since vectorMask() may be empty
        QPsdAbstractLayerItem::PathInfo maskInfo;
        if (maskItem->type() == QPsdAbstractLayerItem::Shape) {
            const auto *shapeItem = reinterpret_cast<const QPsdShapeLayerItem *>(maskItem);
            maskInfo = shapeItem->pathInfo();
        } else {
            maskInfo = maskItem->vectorMask();
        }
        if (maskInfo.type != QPsdAbstractLayerItem::PathInfo::None) {
            // Transform the path from mask layer's coordinates to current item's coordinates
            const QPointF offset = maskItem->rect().topLeft() - currentTopLeft;
            QPainterPath clipPath;
            switch (maskInfo.type) {
            case QPsdAbstractLayerItem::PathInfo::Rectangle:
                clipPath.addRect(maskInfo.rect);
                break;
            case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
                clipPath.addRoundedRect(maskInfo.rect, maskInfo.radius, maskInfo.radius);
                break;
            default:
                clipPath = maskInfo.path;
                break;
            }
            painter->setClipPath(clipPath.translated(offset), Qt::IntersectClip);
        } else {
            // Fall back to transparency mask (for image layers used as clipping masks)
            const QImage maskImage = maskItem->transparencyMask();
            if (!maskImage.size().isEmpty() && maskItem->rect().isValid()) {
                QPixmap pixmap(d->layer->rect().size());
                pixmap.fill(Qt::transparent);
                const auto intersected = maskItem->rect().intersected(d->layer->rect());
                QPainter p(&pixmap);
                p.drawImage(intersected.translated(-x(), -y()), maskImage, intersected.translated(-maskItem->rect().x(), -maskItem->rect().y()));
                p.end();

                painter->setClipRegion(QRegion(pixmap.createHeuristicMask()), Qt::IntersectClip);
            }
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

void QPsdAbstractItem::setGroupCompositionMode(QPainter::CompositionMode mode)
{
    d->groupCompositionMode = mode;
}

QPainter::CompositionMode QPsdAbstractItem::groupCompositionMode() const
{
    return d->groupCompositionMode;
}

QT_END_NAMESPACE

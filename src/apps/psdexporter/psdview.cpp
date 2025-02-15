// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "psdview.h"
#include "psdtextitem.h"
#include "psdshapeitem.h"
#include "psdimageitem.h"
#include "psdfolderitem.h"

#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>

#include <QtWidgets/QRubberBand>

#include <QtPsdCore>

class PsdView::Private
{
public:
    Private(PsdView *parent);

private:
    PsdView *q;

public:
    void modelChanged(PsdTreeItemModel *model);

    QPsdParser psdParser;
    QRubberBand *rubberBand;
    PsdTreeItemModel *model = nullptr;
    QMetaObject::Connection modelConnection;
};

PsdView::Private::Private(PsdView *parent)
    : q(parent)
    , rubberBand(new QRubberBand(QRubberBand::Rectangle, q))
{
    rubberBand->hide();
}

void PsdView::Private::modelChanged(PsdTreeItemModel *model)
{
    QObject::disconnect(modelConnection);

    if (model) {
        modelConnection = QObject::connect(model, &QAbstractItemModel::modelReset, q, &PsdView::reset);
    }

    q->reset();
}

PsdView::PsdView(QWidget *parent)
    : QWidget(parent)
    , d(new Private(this))
{
    connect(this, &PsdView::modelChanged, this, [=](PsdTreeItemModel *model){ d->modelChanged(model); });
}

PsdView::~PsdView() = default;

PsdTreeItemModel *PsdView::model() const
{
    return d->model;
}

void PsdView::setModel(PsdTreeItemModel *model)
{
    if (model == d->model) {
        return;
    }
    d->model = model;
    
    emit modelChanged(model);
}

void PsdView::reset()
{
    auto items = findChildren<PsdAbstractItem *>(Qt::FindDirectChildrenOnly);
    qDeleteAll(items);

    if (d->model == nullptr) {
        return;
    }

    resize(d->model->size());
    std::function<void(const QModelIndex, QWidget *)> traverseTree = [&](const QModelIndex index, QWidget *parent) {
        if (index.isValid()) {
            const QPsdAbstractLayerItem *layer = d->model->layerItem(index);
            const QModelIndex maskIndex = d->model->clippingMaskIndex(index);
            const QPsdAbstractLayerItem *mask = nullptr;
            if (maskIndex.isValid()) {
                mask = d->model->layerItem(maskIndex);
            }
            const QList<QPersistentModelIndex> groupIndexesList = d->model->groupIndexes(index);
            QMap<quint32, QString> groupMap;
            for (auto &groupIndex : groupIndexesList) {
                quint32 id = d->model->layerId(groupIndex);
                QString name = d->model->layerName(groupIndex);
                groupMap.insert(id, name);
            }

            PsdAbstractItem *item = nullptr;
            switch (layer->type()) {
            case QPsdAbstractLayerItem::Text: {
                item = new PsdTextItem(index, reinterpret_cast<const QPsdTextLayerItem *>(layer), mask, groupMap, parent);
                break; }
            case QPsdAbstractLayerItem::Shape: {
                item = new PsdShapeItem(index, reinterpret_cast<const QPsdShapeLayerItem *>(layer), mask, groupMap, parent);
                break; }
            case QPsdAbstractLayerItem::Image: {
                item = new PsdImageItem(index, reinterpret_cast<const QPsdImageLayerItem *>(layer), mask, groupMap, parent);
                break; }
            case QPsdAbstractLayerItem::Folder: {
                item = new PsdFolderItem(index, reinterpret_cast<const QPsdFolderLayerItem *>(layer), mask, groupMap, parent);
                item->resize(size());
                parent = item;
                break; }
            default:
                return;
            }
            item->lower();
        }

        for (int r = 0; r < d->model->rowCount(index); r++) {
            traverseTree(d->model->index(r, 0, index), parent);
        }
    };

    traverseTree(QModelIndex(), this);
}

void PsdView::setItemVisible(quint32 id, bool visible)
{
    for (auto item : findChildren<PsdAbstractItem *>()) {
        if (item->id() == id) {
            item->setVisible(visible);
            break;
        }
    }
}

void PsdView::clearSelection()
{
    d->rubberBand->hide();
}

void PsdView::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QRect rect = event->rect();

    QPainter painter(this);
    painter.fillRect(rect, QColor(0x10, 0x10, 0x10));
    const auto unitSize = 25;
    for (int y = 0; y < height(); y += unitSize) {
        for (int x = 0; x < width(); x += unitSize) {
            QRect r(x, y, unitSize, unitSize);
            if (!rect.intersects(r))
                continue;
            if ((x / unitSize + y / unitSize) % 2 == 0)
                painter.fillRect(x, y, unitSize, unitSize, Qt::darkGray);
        }
    }
}

void PsdView::mouseDoubleClickEvent(QMouseEvent *event)
{
    auto children = findChildren<PsdAbstractItem *>();
    std::reverse(children.begin(), children.end());
    for (const auto *child : children) {
        if (!child->isVisible())
            continue;
        if (!child->geometry().contains(event->pos()))
            continue;
        if (qobject_cast<const PsdFolderItem *>(child))
            continue;

        emit itemSelected(child->modelIndex());
        d->rubberBand->setGeometry(child->geometry());
        d->rubberBand->raise();
        d->rubberBand->show();

        break;
    }
}

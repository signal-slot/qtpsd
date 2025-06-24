// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdview.h"
#include "qpsdtextitem.h"
#include "qpsdshapeitem.h"
#include "qpsdimageitem.h"
#include "qpsdfolderitem.h"

#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>

#include <QtWidgets/QRubberBand>

QT_BEGIN_NAMESPACE

class QPsdView::Private
{
public:
    Private(QPsdView *parent);

private:
    QPsdView *q;

public:
    void modelChanged(QPsdWidgetTreeItemModel *model);

    QRubberBand *rubberBand;
    QPsdWidgetTreeItemModel *model = nullptr;
    QMetaObject::Connection modelConnection;
    bool showChecker = true;
};

QPsdView::Private::Private(QPsdView *parent)
    : q(parent)
    , rubberBand(new QRubberBand(QRubberBand::Rectangle, q))
{
    rubberBand->hide();
}

void QPsdView::Private::modelChanged(QPsdWidgetTreeItemModel *model)
{
    QObject::disconnect(modelConnection);

    if (model) {
        modelConnection = QObject::connect(model, &QAbstractItemModel::modelReset, q, &QPsdView::reset);
    }

    q->reset();
}

QPsdView::QPsdView(QWidget *parent)
    : QWidget(parent)
    , d(new Private(this))
{
    connect(this, &QPsdView::modelChanged, this, [=](QPsdWidgetTreeItemModel *model){ d->modelChanged(model); });
    connect(this, &QPsdView::showCheckerChanged, this, qOverload<>(&QPsdView::update));
}

QPsdView::~QPsdView() = default;

QPsdWidgetTreeItemModel *QPsdView::model() const
{
    return d->model;
}

void QPsdView::setModel(QPsdWidgetTreeItemModel *model)
{
    if (model == d->model) {
        return;
    }
    d->model = model;

    emit modelChanged(model);
}

bool QPsdView::showChecker() const
{
    return d->showChecker;
}

void QPsdView::setShowChecker(bool show)
{
    if (show == d->showChecker) {
        return;
    }
    d->showChecker = show;
    emit showCheckerChanged(show);
}

void QPsdView::reset()
{
    auto items = findChildren<QPsdAbstractItem *>(Qt::FindDirectChildrenOnly);
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

            QPsdAbstractItem *item = nullptr;
            switch (layer->type()) {
            case QPsdAbstractLayerItem::Text: {
                item = new QPsdTextItem(index, reinterpret_cast<const QPsdTextLayerItem *>(layer), mask, groupMap, parent);
                break; }
            case QPsdAbstractLayerItem::Shape: {
                item = new QPsdShapeItem(index, reinterpret_cast<const QPsdShapeLayerItem *>(layer), mask, groupMap, parent);
                break; }
            case QPsdAbstractLayerItem::Image: {
                item = new QPsdImageItem(index, reinterpret_cast<const QPsdImageLayerItem *>(layer), mask, groupMap, parent);
                break; }
            case QPsdAbstractLayerItem::Folder: {
                item = new QPsdFolderItem(index, reinterpret_cast<const QPsdFolderLayerItem *>(layer), mask, groupMap, parent);
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

void QPsdView::setItemVisible(quint32 id, bool visible)
{
    for (auto item : findChildren<QPsdAbstractItem *>()) {
        if (item->id() == id) {
            item->setVisible(visible);
            break;
        }
    }
}

void QPsdView::clearSelection()
{
    d->rubberBand->hide();
}

void QPsdView::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    const QRect rect = event->rect();

    QPainter painter(this);

    if (d->showChecker) {
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
}

void QPsdView::mouseDoubleClickEvent(QMouseEvent *event)
{
    auto children = findChildren<QPsdAbstractItem *>();
    std::reverse(children.begin(), children.end());
    for (const auto *child : children) {
        if (!child->isVisible())
            continue;
        if (!child->geometry().contains(event->pos()))
            continue;
        if (qobject_cast<const QPsdFolderItem *>(child))
            continue;

        emit itemSelected(child->modelIndex());
        d->rubberBand->setGeometry(child->geometry());
        d->rubberBand->raise();
        d->rubberBand->show();

        break;
    }
}

QT_END_NAMESPACE

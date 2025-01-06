// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "psdview.h"
#include "psdtextitem.h"
#include "psdshapeitem.h"
#include "psdimageitem.h"
#include "psdfolderitem.h"
#include "itemexportsettingdialog.h"

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

PsdView::PsdView(QWidget *parent)
    : QWidget(parent)
    , d(new Private(this))
{}

PsdView::~PsdView() = default;

void PsdView::setModel(PsdTreeItemModel *model) {
    if (model == d->model) {
        return;
    }
    if (d->model) {
        disconnect(d->modelConnection);
    }
    d->model = model;
    
    if (d->model) {
        d->modelConnection = QObject::connect(d->model, &QAbstractItemModel::modelReset, this, &PsdView::reset);
    }

    reset();
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
            const QPsdAbstractLayerItem *layer = d->model->data(index, PsdTreeItemModel::Roles::LayerItemObjectRole).value<const QPsdAbstractLayerItem*>();            
            const QModelIndex maskIndex = d->model->data(index, PsdTreeItemModel::ClippingMaskIndexRole).toModelIndex();
            const QPsdAbstractLayerItem *mask = nullptr;
            if (maskIndex.isValid()) {
                mask = d->model->data(maskIndex, PsdTreeItemModel::Roles::LayerItemObjectRole).value<const QPsdAbstractLayerItem*>();
            }
            const QVariantList groupVariantList = d->model->data(index, PsdTreeItemModel::GroupIndexesRole).toList();
            QMap<quint32, QString> groupMap;
            for (auto &v : groupVariantList) {
                QModelIndex modelIndex = v.toModelIndex();
                quint32 id = d->model->data(modelIndex, PsdTreeItemModel::LayerIdRole).toUInt();
                QString name = d->model->data(modelIndex, PsdTreeItemModel::NameRole).toString();
                groupMap.insert(id, name);
            }

            PsdAbstractItem *item = nullptr;
            switch (layer->type()) {
            case QPsdAbstractLayerItem::Text: {
                item = new PsdTextItem(reinterpret_cast<const QPsdTextLayerItem *>(layer), mask, groupMap, parent);
                break; }
            case QPsdAbstractLayerItem::Shape: {
                item = new PsdShapeItem(reinterpret_cast<const QPsdShapeLayerItem *>(layer), mask, groupMap, parent);
                break; }
            case QPsdAbstractLayerItem::Image: {
                item = new PsdImageItem(reinterpret_cast<const QPsdImageLayerItem *>(layer), mask, groupMap, parent);
                break; }
            case QPsdAbstractLayerItem::Folder: {
                item = new PsdFolderItem(reinterpret_cast<const QPsdFolderLayerItem *>(layer), mask, groupMap, parent);
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
        d->rubberBand->setGeometry(child->geometry());
        d->rubberBand->raise();
        d->rubberBand->show();

        ItemExportSettingDialog dialog(this);
        dialog.setItem(child->abstractLayer(), child->groupMap());
        if (dialog.exec() == QDialog::Accepted) {
            emit updateText(child->abstractLayer());
        }

        d->rubberBand->hide();
        break;
    }
}

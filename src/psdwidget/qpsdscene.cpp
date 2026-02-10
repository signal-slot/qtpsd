// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdscene.h"

#include <QtPsdWidget/QPsdWidgetTreeItemModel>
#include <QtPsdWidget/QPsdAbstractItem>
#include <QtPsdWidget/QPsdFolderItem>
#include <QtPsdWidget/QPsdImageItem>
#include <QtPsdWidget/QPsdShapeItem>
#include <QtPsdWidget/QPsdTextItem>

#include <QtPsdGui/QPsdAbstractLayerItem>
#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsPixmapItem>

QT_BEGIN_NAMESPACE

class QPsdScene::Private
{
public:
    Private(QPsdScene *parent);
    ~Private();

private:
    QPsdScene *q;

public:
    void modelChanged(QPsdWidgetTreeItemModel *model);

    QPsdWidgetTreeItemModel *model = nullptr;
    QMetaObject::Connection modelConnection;
    bool showChecker = false;
};

QPsdScene::Private::Private(QPsdScene *parent)
    : q(parent)
{
}

QPsdScene::Private::~Private()
{
}

void QPsdScene::Private::modelChanged(QPsdWidgetTreeItemModel *model)
{
    QObject::disconnect(modelConnection);

    if (model) {
        modelConnection = QObject::connect(model, &QAbstractItemModel::modelReset, q, &QPsdScene::reset);
    }
    q->reset();
}

QPsdScene::QPsdScene(QObject *parent)
    : QGraphicsScene(parent), d(new Private(this))
{
    connect(this, &QPsdScene::modelChanged, this, [this](QPsdWidgetTreeItemModel *model) {
        d->modelChanged(model); 
    });
}

QPsdScene::~QPsdScene()
{
}

QPsdWidgetTreeItemModel *QPsdScene::model() const
{
    return d->model;
}

void QPsdScene::setModel(QPsdWidgetTreeItemModel *model)
{
    if (model == d->model) {
        return;
    }
    d->model = model;

    emit modelChanged(model);
}

bool QPsdScene::showChecker() const
{
    return d->showChecker;
}

void QPsdScene::setItemVisible(quint32 id, bool visible)
{
    for (auto item : items()) {
        auto *psdItem = dynamic_cast<QPsdAbstractItem*>(item);
        if (psdItem && psdItem->id() == id) {
            psdItem->setVisible(visible);
            break;
        }
    }
}

void QPsdScene::selectItem(const QModelIndex &index)
{
    clearSelection();
    if (!index.isValid())
        return;
    for (auto item : items()) {
        auto *psdItem = dynamic_cast<QPsdAbstractItem*>(item);
        if (psdItem && psdItem->modelIndex() == index) {
            psdItem->setSelected(true);
            break;
        }
    }
}

void QPsdScene::reset()
{
    auto items = this->items();
    qDeleteAll(items);

    if (d->model == nullptr) {
        return;
    }

    // Check if any layers are adjustment layers
    bool hasAdjustmentLayers = false;
    std::function<void(const QModelIndex)> scanForAdjustments = [&](const QModelIndex index) {
        if (hasAdjustmentLayers) return;
        if (index.isValid()) {
            const QPsdAbstractLayerItem *layer = d->model->layerItem(index);
            if (layer && layer->type() == QPsdAbstractLayerItem::Adjustment) {
                hasAdjustmentLayers = true;
                return;
            }
        }
        for (int r = 0; r < d->model->rowCount(index); r++) {
            scanForAdjustments(d->model->index(r, 0, index));
        }
    };
    scanForAdjustments(QModelIndex());

    if (hasAdjustmentLayers || d->model->colorMode() == QPsdFileHeader::CMYK) {
        // Use the merged image from the Image Data section
        // This image is pre-composited by Photoshop and includes all adjustment effects
        // For CMYK documents, this ensures correct color compositing in CMYK space
        QImage mergedImage = d->model->mergedImage();
        if (!mergedImage.isNull()) {
            QGraphicsPixmapItem *pixmapItem = new QGraphicsPixmapItem(QPixmap::fromImage(mergedImage));
            addItem(pixmapItem);
        }
        setSceneRect(QRect{ QPoint{}, d->model->size() });
        return;
    }

    std::function<void(const QModelIndex, QGraphicsItem *, QPainter::CompositionMode)> traverseTree = [&](const QModelIndex index, QGraphicsItem *parent, QPainter::CompositionMode groupMode) {
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
            QGraphicsItem *nextParent = parent;
            QPainter::CompositionMode nextGroupMode = groupMode;
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
                nextParent = item;
                const auto blendMode = layer->record().blendMode();
                if (blendMode != QPsdBlend::PassThrough && blendMode != QPsdBlend::Invalid) {
                    nextGroupMode = QtPsdGui::compositionMode(blendMode);
                }
                break; }
            default:
                return;
            }

            if (groupMode != QPainter::CompositionMode_SourceOver) {
                item->setGroupCompositionMode(groupMode);
            }

            if (parent == nullptr) {
                addItem(item);
            }

            parent = nextParent;
            groupMode = nextGroupMode;
        }

        for (int r = d->model->rowCount(index) - 1; r >= 0; r--) {
            traverseTree(d->model->index(r, 0, index), parent, groupMode);
        }
    };

    traverseTree(QModelIndex(), nullptr, QPainter::CompositionMode_SourceOver);

    // If no layers were added (e.g., bitmap mode PSD with no layer records),
    // fall back to rendering the merged image from the Image Data section
    if (this->items().isEmpty()) {
        QImage mergedImage = d->model->mergedImage();
        if (!mergedImage.isNull()) {
            QGraphicsPixmapItem *pixmapItem = new QGraphicsPixmapItem(QPixmap::fromImage(mergedImage));
            addItem(pixmapItem);
        }
    }

    setSceneRect(QRect{ QPoint{}, d->model->size() });
}

void QPsdScene::setShowChecker(bool showChecker)
{
    if (d->showChecker == showChecker) {
        return;
    }

    if (showChecker) {
        const auto unitSize = 25;
        QImage checker(unitSize * 2, unitSize * 2, QImage::Format_ARGB32);
        QPainter painter(&checker);
        painter.fillRect(0, 0, unitSize * 2, unitSize * 2, QColor(0x10, 0x10, 0x10));
        painter.fillRect(0, 0, unitSize, unitSize, Qt::darkGray);
        painter.fillRect(unitSize + 1, unitSize + 1, unitSize, unitSize, Qt::darkGray);
        setBackgroundBrush(QBrush{checker});
    } else {
        setBackgroundBrush(Qt::NoBrush);
    }

    d->showChecker = showChecker;
    emit showCheckerChanged(showChecker);
}

QT_END_NAMESPACE

// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdview.h"
#include "qpsdscene.h"
#include "qpsdabstractitem.h"

#include <QtGui/QContextMenuEvent>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QWheelEvent>
#include <cmath>

#include <QtWidgets/QMenu>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QRubberBand>

QT_BEGIN_NAMESPACE

class QPsdView::Private
{
public:
    Private(QPsdView *parent);

private:
    QPsdView *q;

public:
    QPsdScene *scene;
    QList<QMetaObject::Connection> sceneConnections;
    QRect rubberBandRect;

    QRegion rubberBandRegion(const QWidget *widget, const QRect &rect) const;
    void sceneChanged(QPsdScene *scene);
};

QPsdView::Private::Private(QPsdView *parent)
    : q(parent)
    , scene(nullptr)
{
}

QRegion QPsdView::Private::rubberBandRegion(const QWidget *widget, const QRect &rect) const
{
    QStyleHintReturnMask mask;
    QStyleOptionRubberBand option;
    option.initFrom(widget);
    option.rect = rect;
    option.opaque = false;
    option.shape = QRubberBand::Rectangle;

    QRegion tmp;
    tmp += rect.adjusted(-1, -1, 1, 1);
    if (widget->style()->styleHint(QStyle::SH_RubberBand_Mask, &option, widget, &mask))
        tmp &= mask.region;
    return tmp;
}

void QPsdView::Private::sceneChanged(QPsdScene *scene)
{
    for (const auto &conn: sceneConnections) {
        disconnect(conn);
    }
    sceneConnections.clear();

    if (scene) {
        sceneConnections = {
            QObject::connect(scene, &QPsdScene::modelChanged, q, &QPsdView::modelChanged),
            QObject::connect(scene, &QPsdScene::itemSelected, q, &QPsdView::itemSelected),
            QObject::connect(scene, &QPsdScene::itemsSelected, q, &QPsdView::itemsSelected),
            QObject::connect(scene, &QPsdScene::showCheckerChanged, q, &QPsdView::showCheckerChanged),
            QObject::connect(scene, &QPsdScene::selectionChanged, q, [this]() {
                const auto items = this->scene->selectedItems();
                if (items.size() == 0) {
                    rubberBandRect = QRect {};
                } else if (items.size() == 1) {
                    const auto item = items.at(0);
                    const auto rect = item->sceneBoundingRect().toRect();

                    const QPsdAbstractItem *psdItem = dynamic_cast<const QPsdAbstractItem *>(item);
                    if (psdItem) {
                        emit q->itemSelected(psdItem->modelIndex());
                    }
                    rubberBandRect = rect;
                } else {
                    // Multiple items selected
                    QModelIndexList indexes;
                    QRect combinedRect;
                    for (const auto *item : items) {
                        const QPsdAbstractItem *psdItem = dynamic_cast<const QPsdAbstractItem *>(item);
                        if (psdItem) {
                            indexes.append(psdItem->modelIndex());
                        }
                        combinedRect = combinedRect.united(item->sceneBoundingRect().toRect());
                    }
                    if (!indexes.isEmpty()) {
                        emit q->itemsSelected(indexes);
                    }
                    rubberBandRect = combinedRect;
                }
            }),
        };
    }
}

QPsdView::QPsdView(QWidget *parent)
    : QGraphicsView(parent)
    , d(new Private(this))
{
    connect(this, &QPsdView::sceneChanged, this, [this](QPsdScene *scene) {
        d->sceneChanged(scene);
    });

    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setPsdScene(new QPsdScene(parent));
}

QPsdView::~QPsdView() = default;

QPsdWidgetTreeItemModel *QPsdView::model() const
{
    return d->scene->model();
}

void QPsdView::setPsdScene(QPsdScene *scene)
{
    if (d->scene == scene) {
        return;
    }

    d->scene = scene;

    setScene(scene);

    emit sceneChanged(scene);
}

void QPsdView::setModel(QPsdWidgetTreeItemModel *model)
{
    d->scene->setModel(model);
}

bool QPsdView::showChecker() const
{
    return d->scene->showChecker();
}

void QPsdView::setShowChecker(bool showChecker)
{
    d->scene->setShowChecker(showChecker);
}

void QPsdView::reset()
{
    d->scene->reset();
}

void QPsdView::setItemVisible(quint32 id, bool visible)
{
    d->scene->setItemVisible(id, visible);
}

void QPsdView::selectItem(const QModelIndex &index)
{
    d->scene->selectItem(index);
    const auto selectedItems = d->scene->selectedItems();
    if (!selectedItems.isEmpty()) {
        const auto item = selectedItems.first();
        const auto rectF = item->sceneBoundingRect();
        d->rubberBandRect = rectF.toRect();
        if (!rectF.isEmpty()) {
            // Zoom out if the item doesn't fit in the current viewport.
            // We only shrink — never grow — so selecting a small item after
            // the user zoomed in keeps their zoom level.
            const qreal scale = transform().m11();
            const qreal vpW = viewport()->width() / scale;
            const qreal vpH = viewport()->height() / scale;
            if (rectF.width() > vpW || rectF.height() > vpH) {
                const qreal margin = 0.95; // leave a little breathing room
                const qreal fitScale = qMin(viewport()->width() / rectF.width(),
                                            viewport()->height() / rectF.height()) * margin;
                const qreal newScale = qBound(0.1, fitScale, 10.0);
                setTransform(QTransform::fromScale(newScale, newScale));
                emit scaleChanged(newScale);
                centerOn(rectF.center());
            } else {
                ensureVisible(item);
            }
        }
        viewport()->update();
    } else {
        d->rubberBandRect = QRect {};
        viewport()->update();
    }
}

void QPsdView::clearSelection()
{
    d->rubberBandRect = QRect {};
}

void QPsdView::contextMenuEvent(QContextMenuEvent *event)
{
    const QPointF scenePos = mapToScene(event->pos());
    const auto hitItems = d->scene->items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder);

    // Collect hit QPsdAbstractItems (front-to-back order)
    QList<const QPsdAbstractItem *> psdItems;
    for (auto *item : hitItems) {
        auto *psdItem = dynamic_cast<const QPsdAbstractItem *>(item);
        if (psdItem)
            psdItems.append(psdItem);
    }

    if (psdItems.isEmpty()) {
        QGraphicsView::contextMenuEvent(event);
        return;
    }

    // Build flat menu with breadcrumb path and layer preview icons
    QMenu menu(this);
    constexpr int iconSize = 24;

    for (const auto *psdItem : psdItems) {
        const QModelIndex index = psdItem->modelIndex();

        // Build breadcrumb path: "grandparent > parent > layer"
        QStringList pathParts;
        QModelIndex walk = index;
        while (walk.isValid()) {
            pathParts.prepend(walk.siblingAtColumn(1).data(Qt::DisplayRole).toString());
            walk = walk.parent();
        }
        const QString label = pathParts.join(u" > "_s);

        // Create preview icon from the layer image
        QIcon icon;
        const QImage layerImage = psdItem->abstractLayer()->image();
        if (!layerImage.isNull()) {
            QImage preview(iconSize, iconSize, QImage::Format_ARGB32_Premultiplied);
            preview.fill(Qt::transparent);
            QPainter p(&preview);
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            const QImage scaled = layerImage.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            p.drawImage((iconSize - scaled.width()) / 2, (iconSize - scaled.height()) / 2, scaled);
            p.end();
            icon = QIcon(QPixmap::fromImage(preview));
        }

        QAction *action = menu.addAction(icon, label);
        QPersistentModelIndex persistent(index);
        connect(action, &QAction::triggered, this, [this, persistent]() {
            selectItem(persistent);
        });
    }

    menu.exec(event->globalPos());
}

void QPsdView::paintEvent(QPaintEvent *event)
{
    QGraphicsView::paintEvent(event);

    if (!d->rubberBandRect.isEmpty()) {
        QPainter painter(viewport());
        painter.setWorldTransform(viewportTransform());

        QStyleOptionRubberBand option;
        option.initFrom(viewport());
        option.rect = d->rubberBandRect;
        option.shape = QRubberBand::Rectangle;

        QStyleHintReturnMask mask;
        if (viewport()->style()->styleHint(QStyle::SH_RubberBand_Mask, &option, viewport(), &mask)) {
            // painter clipping for masked rubberbands
            painter.setClipRegion(mask.region, Qt::IntersectClip);
        }

        viewport()->style()->drawControl(QStyle::CE_RubberBand, &option, &painter, viewport());
    }
}

void QPsdView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl+wheel = zoom
        const qreal currentScale = transform().m11();
        const qreal delta = event->angleDelta().y() / 120.0;  // Standard wheel step
        const qreal factor = std::pow(1.1, delta);  // 10% per step
        qreal newScale = currentScale * factor;

        // Clamp to 10% - 1000% range
        newScale = qBound(0.1, newScale, 10.0);

        setTransform(QTransform::fromScale(newScale, newScale));
        emit scaleChanged(newScale);
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

QT_END_NAMESPACE

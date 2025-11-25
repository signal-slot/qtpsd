// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdview.h"
#include "qpsdscene.h"
#include "qpsdabstractitem.h"

#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>

#include <QtWidgets/QStyleOption>
#include <QtWidgets/QRubberBand>

QT_BEGIN_NAMESPACE

class QPsdView::Private
{
public:
    Private();

public:
    QPsdScene *scene;
    QList<QMetaObject::Connection> sceneConnections;
    QRect rubberBandRect;
    QRegion rubberBandRegion(const QWidget *widget, const QRect &rect) const;
};

QPsdView::Private::Private()
    : scene(nullptr)
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

QPsdView::QPsdView(QWidget *parent)
    : QGraphicsView(parent)
    , d(new Private)
{
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
    if (d->scene != scene) {
        d->scene = scene;

        for (const auto &conn: d->sceneConnections) {
            disconnect(conn);
        }
        d->sceneConnections.clear();

        d->sceneConnections = {
            connect(d->scene, &QPsdScene::modelChanged, this, &QPsdView::modelChanged),
            connect(d->scene, &QPsdScene::itemSelected, this, &QPsdView::itemSelected),
            connect(d->scene, &QPsdScene::showCheckerChanged, this, &QPsdView::showCheckerChanged),
            connect(d->scene, &QPsdScene::selectionChanged, this, [&]() {
                const auto items = d->scene->selectedItems();
                if (items.size() == 0) {
                    d->rubberBandRect = QRect {};
                } else if (items.size() == 1) {
                    const auto item = items.at(0);
                    const auto rect = item->sceneBoundingRect().toRect();

                    const QPsdAbstractItem *psdItem = dynamic_cast<const QPsdAbstractItem *>(item);
                    if (psdItem) {
                        emit itemSelected(psdItem->modelIndex());
                    }
                    d->rubberBandRect = rect;
                }
            }),
        };

        setScene(scene);

        emit sceneChanged(scene);
    }
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

void QPsdView::clearSelection()
{
    d->rubberBandRect = QRect {};
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

QT_END_NAMESPACE

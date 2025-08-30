// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdview.h"
#include "qpsdscene.h"

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
    QPsdScene *scene;
    QList<QMetaObject::Connection> sceneConnections;
    QRubberBand *rubberBand;
};

QPsdView::Private::Private(QPsdView *parent)
    : q(parent)
    , scene(nullptr)
    , rubberBand(new QRubberBand(QRubberBand::Rectangle, q))
{
    rubberBand->hide();
}

QPsdView::QPsdView(QWidget *parent)
    : QGraphicsView(parent)
    , d(new Private(this))
{
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

void QPsdView::setShowChecker(bool show)
{
    d->scene->setShowChecker(show);
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
    d->rubberBand->hide();
}

QT_END_NAMESPACE

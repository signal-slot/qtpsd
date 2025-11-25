// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef QPSDABSTRACTITEM_H
#define QPSDABSTRACTITEM_H

#include <QtPsdGui/QPsdAbstractLayerItem>

#include <QtWidgets/QGraphicsItem>
#include <QtCore/QPersistentModelIndex>

QT_BEGIN_NAMESPACE

class QPainter;

class QPsdAbstractItem : public QGraphicsItem
{
public:
    QPsdAbstractItem(const QModelIndex &index, const QPsdAbstractLayerItem *layer, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QGraphicsItem *parent = nullptr);
    ~QPsdAbstractItem();

    quint32 id() const;
    QString name() const;
    const QPsdAbstractLayerItem *abstractLayer() const;
    QMap<quint32, QString> groupMap() const;
    QModelIndex modelIndex() const;

    QRectF boundingRect() const override;

protected:
    void setMask(QPainter *painter) const;

    template<class T>
    const T *layer() const { return reinterpret_cast<const T *>(abstractLayer()); }

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDABSTRACTITEM_H

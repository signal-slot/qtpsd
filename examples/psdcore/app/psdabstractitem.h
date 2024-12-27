// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PSDABSTRACTITEM_H
#define PSDABSTRACTITEM_H

#include <QtWidgets/QWidget>
#include <QtPsdGui/QPsdAbstractLayerItem>

class QPainter;

class PsdAbstractItem : public QWidget
{
    Q_OBJECT
public:
    PsdAbstractItem(const QPsdAbstractLayerItem *layer, const QPsdAbstractLayerItem *maskItem, QWidget *parent = nullptr);
    ~PsdAbstractItem();

    quint32 id() const;
    QString name() const;
    const QPsdAbstractLayerItem *abstractLayer() const;

protected:
    void setMask(QPainter *painter) const;

    template<class T>
    const T *layer() const { return reinterpret_cast<const T *>(abstractLayer()); }

private:
    class Private;
    QScopedPointer<Private> d;
};

#endif // PSDABSTRACTITEM_H

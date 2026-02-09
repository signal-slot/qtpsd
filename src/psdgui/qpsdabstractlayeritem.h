// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDABSTRACTLAYERITEM_H
#define QPSDABSTRACTLAYERITEM_H

#include <QtPsdGui/qpsdguiglobal.h>

#include <QtGui/QImage>
#include <QtGui/QPainterPath>

#include <QtPsdCore/qpsdlayerrecord.h>
#include <QtPsdCore/qpsdlinkedlayer.h>
#include <QtPsdCore/qpsdvectormasksetting.h>

QT_BEGIN_NAMESPACE

class QPsdFolderLayerItem;
class QGradient;
class QPsdBorder;
class QPsdPatternFill;

class Q_PSDGUI_EXPORT QPsdAbstractLayerItem
{
public:
    enum Type {
        Text,
        Shape,
        Image,
        Folder,
        Adjustment,
    };
    QPsdAbstractLayerItem(int width, int height);
    QPsdAbstractLayerItem(const QPsdLayerRecord &record);
    QPsdAbstractLayerItem();
    virtual ~QPsdAbstractLayerItem();
    virtual Type type() const = 0;

    QPsdLayerRecord record() const;

    quint32 id() const;
    QString name() const;
    QColor color() const;
    bool isVisible() const;
    qreal opacity() const;
    qreal fillOpacity() const;
    QRect rect() const;
    QGradient *gradient() const;
    qreal gradientOpacity() const;
    QCborMap dropShadow() const;
    QPsdBorder *border() const;
    QPsdPatternFill *patternFill() const;
    struct PathInfo {
        enum Type {
            None,
            Rectangle,
            RoundedRectangle,
            Path,
        };
        Type type = None;
        QRectF rect;
        qreal radius = 0;
        QPainterPath path;
    };
    PathInfo vectorMask() const;
    QImage image() const;
    QImage transparencyMask() const;
    QImage layerMask() const;
    QRect layerMaskRect() const;
    quint8 layerMaskDefaultColor() const;

    QPsdLinkedLayer::LinkedFile linkedFile() const;
    void setLinkedFile(const QPsdLinkedLayer::LinkedFile &linkedFile);

    QVariantList effects() const;

protected:
    QPsdAbstractLayerItem::PathInfo parseShape(const QPsdVectorMaskSetting &vms) const;

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDABSTRACTLAYERITEM_H

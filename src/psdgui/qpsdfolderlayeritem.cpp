// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdfolderlayeritem.h"

QT_BEGIN_NAMESPACE

class QPsdFolderLayerItem::Private
{
public:
    QList<const QPsdAbstractLayerItem *> children;
    bool opened = true;
    QRect artboardRect;
    QString artboardPresetName;
    QColor artboardBackground;
};

QPsdFolderLayerItem::QPsdFolderLayerItem(int width, int height)
    : QPsdAbstractLayerItem(width, height)
    , d(new Private)
{}

QPsdFolderLayerItem::QPsdFolderLayerItem(const QPsdLayerRecord &record, bool opened)
    : QPsdAbstractLayerItem(record)
    , d(new Private)
{
    d->opened = opened;
    const auto additionalLayerInformation = record.additionalLayerInformation();
    if (additionalLayerInformation.contains("artb")) {
        const auto artb = additionalLayerInformation.value("artb"_ba).value<QPsdDescriptor>().data();
//        const auto guideIndeces = artb.value("guideIndeces").toList();

        const auto artboardRect = artb.value("artboardRect").value<QPsdDescriptor>().data();
        const auto btom = artboardRect.value("Btom").toDouble();
        const auto rght = artboardRect.value("Rght").toDouble();
        const auto left = artboardRect.value("Left").toDouble();
        const auto top_ = artboardRect.value("Top ").toDouble();
        d->artboardRect = QRect(left, top_, rght - left, btom - top_);

        d->artboardPresetName = artb.value("artboardPresetName").toString();

        const auto artboardBackgroundType = artb.value("artboardBackgroundType").toInt();
        // https://community.adobe.com/t5/photoshop-ecosystem-discussions/combining-mulitple-photoshop-files-into-1-photoshop-file-with-artboards/td-p/12218200
        // 1 = White, 2 = Black, 3 = Transparent, 4 = Other
        switch (artboardBackgroundType) {
        case 1:
            d->artboardBackground = Qt::white;
            break;
        case 2:
            d->artboardBackground = Qt::black;
            break;
        case 3:
            d->artboardBackground = Qt::transparent;
            break;
        case 4: {
            const auto clr = artb.value("Clr ").value<QPsdDescriptor>().data();
            const auto rd__ = clr.value("Rd  ").toDouble();
            const auto grn_ = clr.value("Grn ").toDouble();
            const auto bl__ = clr.value("Bl  ").toDouble();
            d->artboardBackground = QColor(rd__, grn_, bl__);
            break; }
        }
        qDebug() << d->artboardBackground;
    }
}

QPsdFolderLayerItem::QPsdFolderLayerItem()
    : QPsdAbstractLayerItem()
    , d(new Private)
{
}

QPsdFolderLayerItem::~QPsdFolderLayerItem() = default;

QList<const QPsdAbstractLayerItem *> QPsdFolderLayerItem::children() const
{
    return d->children;
}

void QPsdFolderLayerItem::addChild(const QPsdAbstractLayerItem *child)
{
    d->children.append(child);
}

bool QPsdFolderLayerItem::isOpened() const
{
    return d->opened;
}

QRect QPsdFolderLayerItem::artboardRect() const
{
    return d->artboardRect;
}

QString QPsdFolderLayerItem::artboardPresetName() const
{
    return d->artboardPresetName;
}

QColor QPsdFolderLayerItem::artboardBackground() const
{
    return d->artboardBackground;
}

QT_END_NAMESPACE

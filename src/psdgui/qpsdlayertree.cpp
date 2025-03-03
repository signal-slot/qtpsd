// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdlayertree.h"
#include "qpsdfolderlayeritem.h"
#include "qpsdtextlayeritem.h"
#include "qpsdshapelayeritem.h"
#include "qpsdimagelayeritem.h"
#include "qpsdlinkedlayer.h"
#include "qpsdplacedlayer.h"
#include "qpsdplacedlayerdata.h"

#include <QtPsdCore/QPsdSectionDividerSetting>

QT_BEGIN_NAMESPACE

QPsdFolderLayerItem *QPsdLayerTree::fromParser(const QPsdParser &parser)
{
    const auto header = parser.fileHeader();
    QPsdFolderLayerItem *ret = new QPsdFolderLayerItem(header.width(), header.height());
    const auto colorModeData = parser.colorModeData();
    const auto imageResources = parser.imageResources();

    QList<int> groupIDs;
    for (const auto &block : imageResources.imageResourceBlocks()) {
        switch (block.id()) {
        case 1026: {
            const QByteArray groupData = block.data();
            const quint16 *p = reinterpret_cast<const quint16 *>(groupData.constData());
            for (int i = 0; i < groupData.size() / 2; i++) {
                const auto id = *p++;
                groupIDs.append(id);
            }
            break; }
        default:
            // qDebug() << block.id();
            break;
        }
    }
    const auto layerAndMaskInformation = parser.layerAndMaskInformation();
    const auto additionalLayerInformation = layerAndMaskInformation.additionalLayerInformation();

    QList<QPsdLinkedLayer::LinkedFile> linkedFiles;
    if (additionalLayerInformation.contains("lnk2")) {
        if (additionalLayerInformation.contains("lnk2")) {
            const auto lnk2 = additionalLayerInformation.value("lnk2"_ba).value<QPsdLinkedLayer>();
            linkedFiles = lnk2.files();
        }
    }

    const auto layers = layerAndMaskInformation.layerInfo();
    auto records = layers.records();
    std::reverse(records.begin(), records.end());
    const auto channelImageData = layers.channelImageData();

    QList<QPsdAbstractLayerItem *> toBeClippped;
    QPsdFolderLayerItem *parent = ret;

    QMultiMap<int, QPsdAbstractLayerItem *> groups;
    int i = records.count();
    for (auto record : records) {
        i--;
        auto imageData = channelImageData.at(i);
        imageData.setHeader(header);
        record.setImageData(imageData);
        const auto additionalLayerInformation = record.additionalLayerInformation();

        QPsdAbstractLayerItem *item = nullptr;

        qDebug() << QString::fromLocal8Bit(record.name()) << additionalLayerInformation.keys();

        // Layer structure
        if (additionalLayerInformation.contains("lsdk")) {
            const auto lsdk = additionalLayerInformation.value("lsdk"_ba).toInt();
            switch (lsdk) {
            case 1:
                item = new QPsdFolderLayerItem(record, true);
                break;
            case 2:
                item = new QPsdFolderLayerItem(record, false);
                break;
            case 3:
                break;
            }
        } else {
            const auto lsct = additionalLayerInformation.value("lsct"_ba).value<QPsdSectionDividerSetting>();
            switch (lsct.type()) {
            case QPsdSectionDividerSetting::AnyOtherTypeOfLayer: {
                if (additionalLayerInformation.contains("TySh")) {
                    item = new QPsdTextLayerItem(record);
                } else if (additionalLayerInformation.contains("vscg") || additionalLayerInformation.contains("SoCo")) {
                    item = new QPsdShapeLayerItem(record);
                } else {
                    item = new QPsdImageLayerItem(record);
                }
                switch (record.clipping()) {
                case QPsdLayerRecord::Clipping::NonBase:
                    toBeClippped.append(item);
                    break;
                case QPsdLayerRecord::Clipping::Base:
                    for (auto *clipped : toBeClippped) {
                        clipped->setMaskItem(item);
                    }
                    toBeClippped.clear();
                    break;
                }

                if (additionalLayerInformation.contains("SoLd")) {
                    const auto sold = additionalLayerInformation.value("SoLd"_ba).value<QPsdPlacedLayerData>();
                    const auto descriptor = sold.descriptor().data();
                    if (descriptor.contains("Idnt")) {
                        const auto uniqueId = descriptor.value("Idnt").toString().toLatin1();
                        for (const auto &file : linkedFiles) {
                            if (file.uniqueId == uniqueId) {
                                item->setLinkedFile(file);
                                break;
                            }
                        }
                    }
                } else if (additionalLayerInformation.contains("PlLd")) {
                    const auto plld = additionalLayerInformation.value("PlLd"_ba).value<QPsdPlacedLayer>();
                    for (const auto &file : linkedFiles) {
                        if (file.uniqueId == plld.uniqueId()) {
                            item->setLinkedFile(file);
                            break;
                        }
                    }
                }
                break; }
            case QPsdSectionDividerSetting::OpenFolder:
                item = new QPsdFolderLayerItem(record, true);
                break;
            case QPsdSectionDividerSetting::ClosedFolder:
                item = new QPsdFolderLayerItem(record, false);
                break;
            case QPsdSectionDividerSetting::BoundingSectionDivider:
                break;
            }

        }

        if (item) {
            if (i < groupIDs.size()) {
                const auto groupID = groupIDs.at(i);
                if (groupID > 0) {
                    groups.insert(groupID, item);
                }
            }
            item->setParent(parent);
            if (item->type() == QPsdAbstractLayerItem::Type::Folder) {
                parent = static_cast<QPsdFolderLayerItem *>(item);
            }
        } else {
            parent = parent->parent();
        }
    }

    for (const auto groupID : groups.keys()) {
        auto group = groups.values(groupID);
        for (auto *item : group) {
            item->setGroup(group);
        }
    }
    return ret;
}

QT_END_NAMESPACE

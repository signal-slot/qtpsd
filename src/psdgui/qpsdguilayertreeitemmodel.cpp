// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdabstractlayeritem.h"
#include "qpsdfolderlayeritem.h"
#include "qpsdimagelayeritem.h"
#include "qpsdshapelayeritem.h"
#include "qpsdtextlayeritem.h"
#include "qpsdguilayertreeitemmodel.h"

QT_BEGIN_NAMESPACE

class QPsdGuiLayerTreeItemModel::Private
{
public:
    Private(const QPsdGuiLayerTreeItemModel *model);
    ~Private();

    QPsdAbstractLayerItem *layerItemObject(const QPsdLayerRecord *layerRecord, QPsdLayerTreeItemModel::FolderType folderType);

    const QPsdGuiLayerTreeItemModel *q;
    
    QMap<const QPsdLayerRecord *, QPsdAbstractLayerItem *> mapLayerItemObjects;
    QList<QPsdLinkedLayer::LinkedFile> linkedFiles;
};

QPsdGuiLayerTreeItemModel::Private::Private(const QPsdGuiLayerTreeItemModel *model) : q(model)
{
}

QPsdGuiLayerTreeItemModel::Private::~Private()
{
    for (auto it = mapLayerItemObjects.begin(); it != mapLayerItemObjects.end(); ++it) {
        delete it.value();
    }
}

QPsdAbstractLayerItem *QPsdGuiLayerTreeItemModel::Private::layerItemObject(const QPsdLayerRecord *layerRecord,
                                                                           QPsdLayerTreeItemModel::FolderType folderType)
{
    if (!mapLayerItemObjects.contains(layerRecord)) {
        const auto additionalLayerInformation = layerRecord->additionalLayerInformation();

        QPsdAbstractLayerItem *item = nullptr;

        switch (folderType) {
        case QPsdLayerTreeItemModel::FolderType::OpenFolder:
            item = new QPsdFolderLayerItem(*layerRecord, true);
            break;
        case QPsdLayerTreeItemModel::FolderType::ClosedFolder:
            item = new QPsdFolderLayerItem(*layerRecord, false);
            break;
        default:
            if (additionalLayerInformation.contains("TySh")) {
                item = new QPsdTextLayerItem(*layerRecord);
            } else if (additionalLayerInformation.contains("vscg") || additionalLayerInformation.contains("SoCo")) {
                item = new QPsdShapeLayerItem(*layerRecord);
            } else {
                item = new QPsdImageLayerItem(*layerRecord);
            }

            //TODO clipping support

            if (additionalLayerInformation.contains("SoLd")) {
                const auto sold = additionalLayerInformation.value("SoLd").value<QPsdPlacedLayerData>();
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
                const auto plld = additionalLayerInformation.value("PlLd").value<QPsdPlacedLayer>();
                for (const auto &file : linkedFiles) {
                    if (file.uniqueId == plld.uniqueId()) {
                        item->setLinkedFile(file);
                        break;
                    }
                }
            }

            break;
        }

        if (item) {
            mapLayerItemObjects.insert(layerRecord, item);
        }
    }

    return mapLayerItemObjects.value(layerRecord);
}

QPsdGuiLayerTreeItemModel::QPsdGuiLayerTreeItemModel(QObject *parent)
    : QPsdLayerTreeItemModel{parent}, d{new Private(this)}
{
}

QPsdGuiLayerTreeItemModel::~QPsdGuiLayerTreeItemModel()
{
}

QHash<int, QByteArray> QPsdGuiLayerTreeItemModel::roleNames() const
{
    auto roles = QPsdLayerTreeItemModel::roleNames();
    roles.insert(Roles::LayerItemObjectRole, QByteArrayLiteral("LayerItemObject"));

    return roles;
}

QVariant QPsdGuiLayerTreeItemModel::data(const QModelIndex &index, int role) const
{
    switch (role) {
    case Roles::LayerItemObjectRole:
        return QVariant::fromValue(
            d->layerItemObject(
                QPsdLayerTreeItemModel::data(index, QPsdLayerTreeItemModel::Roles::LayerRecordObjectRole).value<const QPsdLayerRecord *>(),
                QPsdLayerTreeItemModel::data(index, QPsdLayerTreeItemModel::Roles::FolderTypeRole).value<QPsdLayerTreeItemModel::FolderType>()
                ));
    default:
        return QPsdLayerTreeItemModel::data(index, role);
    }
}

void QPsdGuiLayerTreeItemModel::fromParser(const QPsdParser &parser)
{
    QPsdLayerTreeItemModel::fromParser(parser);

    const auto layerAndMaskInformation = parser.layerAndMaskInformation();
    const auto additionalLayerInformation = layerAndMaskInformation.additionalLayerInformation();

    if (additionalLayerInformation.contains("lnk2")) {
        const auto lnk2 = additionalLayerInformation.value("lnk2").value<QPsdLinkedLayer>();
        d->linkedFiles = lnk2.files();
    }
}

const QPsdAbstractLayerItem *QPsdGuiLayerTreeItemModel::layerItem(const QModelIndex &index) const
{
    return d->layerItemObject(layerRecord(index), folderType(index));
}

QT_END_NAMESPACE

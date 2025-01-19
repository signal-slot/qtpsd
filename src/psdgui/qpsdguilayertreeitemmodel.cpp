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

    QPsdAbstractLayerItem *layerItemObject(const QPsdLayerRecord *layerRecord, enum QPsdLayerTreeItemModel::FolderType folderType);
    void sourceModelChanged(QAbstractItemModel *model);
    void parserReady(const QPsdParser &parser);

    const QPsdGuiLayerTreeItemModel *q;
    QMap<const QPsdLayerRecord *, QPsdAbstractLayerItem *> mapLayerItemObjects;

    QList<QPsdLinkedLayer::LinkedFile> linkedFiles;
    QMetaObject::Connection sourceModelConnection;
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
                                                                           enum QPsdLayerTreeItemModel::FolderType folderType)
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

void QPsdGuiLayerTreeItemModel::Private::sourceModelChanged(QAbstractItemModel *sourceModel)
{
    QObject::disconnect(sourceModelConnection);

    mapLayerItemObjects.clear();
    linkedFiles.clear();

    QPsdLayerTreeItemModel *model = dynamic_cast<QPsdLayerTreeItemModel *>(sourceModel);
    if (model) {
        sourceModelConnection = QObject::connect(model, &QPsdLayerTreeItemModel::parserReady, q, [=](const QPsdParser &parser) { parserReady(parser); });
    }
}

void QPsdGuiLayerTreeItemModel::Private::parserReady(const QPsdParser &parser)
{
    const auto layerAndMaskInformation = parser.layerAndMaskInformation();
    const auto additionalLayerInformation = layerAndMaskInformation.additionalLayerInformation();

    if (additionalLayerInformation.contains("lnk2")) {
        const auto lnk2 = additionalLayerInformation.value("lnk2").value<QPsdLinkedLayer>();
        linkedFiles = lnk2.files();
    }
}

QPsdGuiLayerTreeItemModel::QPsdGuiLayerTreeItemModel(QObject *parent)
    : QIdentityProxyModel{parent}, d{new Private(this)}
{
    QObject::connect(this, &QAbstractProxyModel::sourceModelChanged, this, [=](){ d->sourceModelChanged(sourceModel()); });
}

QPsdGuiLayerTreeItemModel::~QPsdGuiLayerTreeItemModel()
{
}

QHash<int, QByteArray> QPsdGuiLayerTreeItemModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();
    roles.insert(Roles::LayerIdRole, QByteArrayLiteral("LayerId"));
    roles.insert(Roles::NameRole, QByteArrayLiteral("Name"));
    roles.insert(Roles::LayerRecordObjectRole, QByteArrayLiteral("LayerRecordObject"));
    roles.insert(Roles::FolderTypeRole, QByteArrayLiteral("FolderType"));
    roles.insert(Roles::GroupIndexesRole, QByteArrayLiteral("GroupIndexes"));
    roles.insert(Roles::ClippingMaskIndexRole, QByteArrayLiteral("ClippingMaskIndex"));
    roles.insert(Roles::LayerItemObjectRole, QByteArrayLiteral("LayerItemObject"));

    return roles;
}

QVariant QPsdGuiLayerTreeItemModel::data(const QModelIndex &index, int role) const
{
    switch (role) {
    case Roles::GroupIndexesRole: {
        QList<QVariant> indexes = sourceModel()->data(mapToSource(index), role).toList();
        QList<QVariant> result;
        for (const auto &i : indexes) {
            QModelIndex sourceIndex = i.value<QPersistentModelIndex>();
            result.append(QVariant::fromValue(QPersistentModelIndex(mapFromSource(sourceIndex))));
        }
        return QVariant(result); }
    case Roles::ClippingMaskIndexRole: {
        QPersistentModelIndex maskIndex = sourceModel()->data(mapToSource(index), role).value<QPersistentModelIndex>();
        return QVariant::fromValue(mapFromSource(maskIndex)); }
    case Roles::LayerItemObjectRole:
        return QVariant::fromValue(
            d->layerItemObject(
                sourceModel()->data(mapToSource(index), QPsdLayerTreeItemModel::Roles::LayerRecordObjectRole).value<const QPsdLayerRecord *>(),
                sourceModel()->data(mapToSource(index), QPsdLayerTreeItemModel::Roles::FolderTypeRole).value<enum QPsdLayerTreeItemModel::FolderType>()
                ));
    default:
        return sourceModel()->data(mapToSource(index), role);
    }
}

void QPsdGuiLayerTreeItemModel::fromParser(const QPsdParser &parser)
{
    QPsdAbstractLayerTreeItemModel *parentModel = dynamic_cast<QPsdAbstractLayerTreeItemModel *>(sourceModel());
    if (parentModel == nullptr) {
        return;
    }

    parentModel->fromParser(parser);
}

QSize QPsdGuiLayerTreeItemModel::size() const
{
    QPsdAbstractLayerTreeItemModel *parentModel = dynamic_cast<QPsdAbstractLayerTreeItemModel *>(sourceModel());
    if (parentModel == nullptr) {
        return {};
    }

    return parentModel->size();
}

QT_END_NAMESPACE

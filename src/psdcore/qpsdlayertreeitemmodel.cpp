// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdlayertreeitemmodel.h"
#include "qpsdlayerrecord.h"
#include "qpsdparser.h"
#include "qpsdsectiondividersetting.h"

#include <QtCore/QFileInfo>
#include <QtCore/QVariant>

QT_BEGIN_NAMESPACE

class QPsdLayerTreeItemModel::Private
{
public:
    struct Node {
        qint32 recordIndex;
        quint32 layerId;
        qint32 parentNodeIndex;
        enum FolderType folderType;
        bool isCloseFolder;
    };

    struct IndexInfo {
        int row = -1;
        qint32 nodeIndex = -1;
    };

    Private(const ::QPsdLayerTreeItemModel *model);
    ~Private();

    bool isValidIndex(const QModelIndex &index) const;

    const ::QPsdLayerTreeItemModel *q;

    QPsdFileHeader fileHeader;
    QList<QPsdLayerRecord> layerRecords;
    QList<Node> treeNodeList;
    QList<int> groupIDs;
    QMultiMap<int, IndexInfo> groupsMap;
    QList<IndexInfo> clippingMasks;
};

QPsdLayerTreeItemModel::Private::Private(const ::QPsdLayerTreeItemModel *model) : q(model)
{
}

QPsdLayerTreeItemModel::Private::~Private()
{
}

bool QPsdLayerTreeItemModel::Private::isValidIndex(const QModelIndex &index) const
{
    return index.isValid() && index.model() == q;
}

QPsdLayerTreeItemModel::QPsdLayerTreeItemModel(QObject *parent)
    : QAbstractItemModel(parent), d(new Private(this))
{
}

QPsdLayerTreeItemModel::~QPsdLayerTreeItemModel()
{
}

QHash<int, QByteArray> QPsdLayerTreeItemModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();
    roles.insert(Roles::LayerIdRole, QByteArrayLiteral("LayerId"));
    roles.insert(Roles::NameRole, QByteArrayLiteral("Name"));
    roles.insert(Roles::LayerRecordObjectRole, QByteArrayLiteral("LayerRecordObject"));
    roles.insert(Roles::FolderTypeRole, QByteArrayLiteral("FolderType"));
    roles.insert(Roles::GroupIndexesRole, QByteArrayLiteral("GroupIndexes"));
    roles.insert(Roles::ClippingMaskIndexRole, QByteArrayLiteral("ClippingMaskIndex"));

    return roles;
}

QModelIndex QPsdLayerTreeItemModel::index(int row, int column, const QModelIndex &parent) const
{
    if (d->layerRecords.size() == 0 || d->treeNodeList.size() == 0) {
        return {};
    }

    qint32 parentNodeIndex = -1;
    if (parent.isValid()) {
        parentNodeIndex = parent.internalId();
    }

    int r = -1;
    int i = d->treeNodeList.size();
    for (auto it = d->treeNodeList.crbegin(); it != d->treeNodeList.crend(); ++it) {
        const auto node = *it;
        i--;
        if (node.parentNodeIndex == parentNodeIndex) {
            r++;
            if (r == row) {
                return createIndex(row, column, i);
            }
        }
    }

    return {};
}

QModelIndex QPsdLayerTreeItemModel::parent(const QModelIndex &index) const
{
    if (!d->isValidIndex(index)) {
        return {};
    }

    qint32 nodeIndex = index.internalId();
    if (nodeIndex < 0 || d->treeNodeList.size() <= nodeIndex) {
        return {};
    }

    const auto &node = d->treeNodeList.at(nodeIndex);
    qint32 parentNodeIndex = node.parentNodeIndex;

    if (parentNodeIndex < 0 || d->treeNodeList.size() <= parentNodeIndex) {
        return {};
    }
    const auto &parentNode = d->treeNodeList.at(parentNodeIndex);
    qint32 grandParentNodeIndex = parentNode.parentNodeIndex;

    int r = -1;
    int i = d->treeNodeList.size();
    for (auto it = d->treeNodeList.crbegin(); it != d->treeNodeList.crend(); ++it) {
        const auto node = *it;
        i--;
        if (node.parentNodeIndex == grandParentNodeIndex) {
            r++;
            if (i == parentNodeIndex) {
                return createIndex(r, 0, i);
            }
        }
    }

    return {};
}

int QPsdLayerTreeItemModel::rowCount(const QModelIndex &parent) const
{
    if (d->layerRecords.size() == 0 || d->treeNodeList.size() == 0) {
        return 0;
    }

    qint32 parentNodeIndex = parent.isValid() ? parent.internalId() : -1;    
    int count = 0;
    for (auto it = d->treeNodeList.crbegin(); it != d->treeNodeList.crend(); ++it) {
        const auto node = *it;

        if (node.parentNodeIndex == parentNodeIndex) {
            if (node.isCloseFolder) {
                return count;
            }

            count++;
        }
    }

    return count;
}

int QPsdLayerTreeItemModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 2;
}

QVariant QPsdLayerTreeItemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case Column::LayerIdColumn:
            return QString::number(layerId(index));
        case Column::NameColumn:
            return layerName(index);
        default:
            break;
        }
        break;
    case Qt::CheckStateRole:
        switch (index.column()) {
        case Column::FolderTypeColumn:
            switch (folderType(index)) {
            case FolderType::OpenFolder:
                return Qt::Checked;
            case FolderType::ClosedFolder:
                return Qt::Unchecked;
            default:
                break;
            }
            break;
        default:
            break;
        }
        break;
    case Roles::LayerIdRole:
        return QString::number(layerId(index));
    case Roles::NameRole:
        return layerName(index);
    case Roles::LayerRecordObjectRole:
        return QVariant::fromValue(layerRecord(index));
    case Roles::FolderTypeRole:
        return QVariant::fromValue(folderType(index));
    case Roles::GroupIndexesRole: {
        const auto list = groupIndexes(index);
        QList<QVariant> result;
        for (const auto &index : list) {
            result.append(QVariant::fromValue(index));
        }
        return QVariant(result); }
    case Roles::ClippingMaskIndexRole:
        return QVariant::fromValue(clippingMaskIndex(index));
    default:
        break;
    }

    return {};
}

void QPsdLayerTreeItemModel::fromParser(const QPsdParser &parser)
{
    beginResetModel();

    d->treeNodeList.clear();
    d->groupIDs.clear();
    d->groupsMap.clear();
    d->clippingMasks.clear();

    d->fileHeader = parser.fileHeader();
    const auto imageResources = parser.imageResources();
    
    for (const auto &block : imageResources.imageResourceBlocks()) {
        switch (block.id()) {
        case 1026: {
            const QByteArray groupData = block.data();
            const quint16 *p = reinterpret_cast<const quint16 *>(groupData.constData());
            for (int i = 0; i < groupData.size() / 2; i++) {
                const auto id = *p++;
                d->groupIDs.append(id);
            }
            break; }
        default:
            // qDebug() << block.id();
            break;
        }
    }
    const auto layerAndMaskInformation = parser.layerAndMaskInformation();
    const auto layers = layerAndMaskInformation.layerInfo();
    d->layerRecords = layers.records();
    const auto channelImageData = layers.channelImageData();
    
    qint32 parentNodeIndex = -1;
    QList<int> rowStack;
    int row = -1;
    qint32 i = d->layerRecords.size();
    std::for_each(d->layerRecords.rbegin(), d->layerRecords.rend(), [&](auto &record) {
        i--;
        auto imageData = channelImageData.at(i);
        imageData.setHeader(d->fileHeader);
        record.setImageData(imageData);
    
        const auto additionalLayerInformation = record.additionalLayerInformation();

        bool isCloseFolder = false;
        enum FolderType folderType = FolderType::NotFolder;

        // Layer structure
        if (additionalLayerInformation.contains("lsdk")) {
            const auto lsdk = additionalLayerInformation.value("lsdk").toInt();
            switch (lsdk) {
            case 1:
                folderType = FolderType::OpenFolder;
                break;
            case 2:
                folderType = FolderType::ClosedFolder;
                break;
            case 3:
                isCloseFolder = true;
                break;
            }
        } else {
            const auto lsct = additionalLayerInformation.value("lsct").template value<QPsdSectionDividerSetting>();
            switch (lsct.type()) {
            case QPsdSectionDividerSetting::OpenFolder:
                folderType = FolderType::OpenFolder;
                break;
            case QPsdSectionDividerSetting::ClosedFolder:
                folderType = FolderType::ClosedFolder;
                break;
            case QPsdSectionDividerSetting::BoundingSectionDivider:
                isCloseFolder = true;
                break;
            default:
                break;
            }
        }
        
        row++;
        Private::IndexInfo indexInfo;
        if (!isCloseFolder) {
            indexInfo = { row, i };
            
            if (i < d->groupIDs.size()) {
                const auto groupID = d->groupIDs.at(i);
                if (groupID > 0) {
                    d->groupsMap.insert(groupID, indexInfo);
                }
            }

            if (record.clipping() == QPsdLayerRecord::Clipping::Base) {
                while (d->clippingMasks.size() < d->treeNodeList.size()) {
                    d->clippingMasks.prepend(indexInfo);
                }
                d->clippingMasks.prepend({});
            }
        }
        
        if (isCloseFolder && !rowStack.isEmpty()) {
            row = rowStack.takeLast();
        } else if (folderType != FolderType::NotFolder) {
            rowStack.push_back(row);
            row = -1;
        }

        // Layer ID
        const auto lyid = additionalLayerInformation.value("lyid").template value<quint32>();

        d->treeNodeList.prepend(QPsdLayerTreeItemModel::Private::Node {
            i,
            lyid,
            parentNodeIndex,
            folderType,
            isCloseFolder,
        });
        
        if (folderType != FolderType::NotFolder) {
            parentNodeIndex = i;
        }

        if (isCloseFolder) {
            const QPsdLayerTreeItemModel::Private::Node node = d->treeNodeList.at(parentNodeIndex - i);
            parentNodeIndex = node.parentNodeIndex;
        }
    });

    while (d->clippingMasks.size() < d->treeNodeList.size()) {
        d->clippingMasks.prepend({});
    }

    endResetModel();
}

QSize QPsdLayerTreeItemModel::size() const
{
    return d->fileHeader.size();
}

qint32 QPsdLayerTreeItemModel::layerId(const QModelIndex &index) const
{
    int nodeIndex = index.internalId();
    const auto node = d->treeNodeList.at(nodeIndex);

    return node.layerId;
}

QString QPsdLayerTreeItemModel::layerName(const QModelIndex &index) const
{
    const auto *layerRecord = this->layerRecord(index);
    const auto additionalLayerInformation = layerRecord->additionalLayerInformation();

    // Layer name
    if (additionalLayerInformation.contains("luni")) {
        return additionalLayerInformation.value("luni").toString();
    } else {
        return QString::fromUtf8(layerRecord->name());
    }
}

const QPsdLayerRecord *QPsdLayerTreeItemModel::layerRecord(const QModelIndex &index) const
{
    int nodeIndex = index.internalId();

    return &(d->layerRecords.at(nodeIndex));
}

QPsdLayerTreeItemModel::FolderType QPsdLayerTreeItemModel::folderType(const QModelIndex &index) const
{
    int nodeIndex = index.internalId();
    const auto node = d->treeNodeList.at(nodeIndex);

    return node.folderType;
}

QList<QPersistentModelIndex> QPsdLayerTreeItemModel::groupIndexes(const QModelIndex &index) const
{
    int nodeIndex = index.internalId();
    const auto &list = d->groupsMap.values(d->groupIDs.at(nodeIndex));
    QList<QPersistentModelIndex> groupIndexes;
    for (const auto &info : list) {
        groupIndexes.append(createIndex(info.row, 0, info.nodeIndex));
    }
    return groupIndexes;
}

QPersistentModelIndex QPsdLayerTreeItemModel::clippingMaskIndex(const QModelIndex &index) const
{
    int nodeIndex = index.internalId();
    const auto &info = d->clippingMasks.at(nodeIndex);
    return createIndex(info.row, 0, info.nodeIndex);
}

QT_END_NAMESPACE

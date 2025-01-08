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
        QPersistentModelIndex modelIndex;
    };

    Private(const ::QPsdLayerTreeItemModel *model);
    ~Private();

    bool isValidIndex(const QModelIndex &index) const;

    const ::QPsdLayerTreeItemModel *q;

    QPsdFileHeader fileHeader;
    QList<QPsdLayerRecord> layerRecords;
    QList<Node> treeNodeList;
    QList<quint32> groupIDs; // Group ID for each layer index
    QMultiMap<quint32, QPersistentModelIndex> groupsMap; // Maps group IDs to member layer indexes
    QMap<QPersistentModelIndex, QPersistentModelIndex> clippingMaskMap; // Maps clipped layers to their base layers
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

    auto layerRecordObject = [&](int nodeIndex) {
        return &(d->layerRecords.at(nodeIndex));
    };

    auto layerName = [&](int nodeIndex) {
        const auto *layerRecord = layerRecordObject(nodeIndex);
        const auto additionalLayerInformation = layerRecord->additionalLayerInformation();

        // Layer name
        if (additionalLayerInformation.contains("luni")) {
            return additionalLayerInformation.value("luni").toString();
        } else {
            return QString::fromUtf8(layerRecord->name());
        }
    };

    int nodeIndex = index.internalId();
    const auto node = d->treeNodeList.at(nodeIndex);
    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case Column::LayerId:
            return QString::number(node.layerId);
        case Column::Name:
            return layerName(nodeIndex);
        default:
            break;
        }
        break;
    case Qt::CheckStateRole:
        switch (index.column()) {
        case Column::FolderType:
            switch (node.folderType) {
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
        return QString::number(node.layerId);
    case Roles::NameRole:
        return layerName(nodeIndex);
    case Roles::LayerRecordObjectRole:
        return QVariant::fromValue(layerRecordObject(nodeIndex));
    case Roles::FolderTypeRole:
        return QVariant::fromValue(node.folderType);
    case Roles::GroupIndexesRole: {
        QVariantList result;
        if (index.isValid()) {
            // Get the layer ID for this node
            const auto &node = d->treeNodeList.at(nodeIndex);
            const auto layerID = node.layerId;
            qDebug() << "\nGroupIndexesRole - Node:" << nodeIndex << "LayerID:" << layerID;

            // First check direct layer ID relationships
            auto layerMembers = d->groupsMap.values(layerID);
            qDebug() << "Found" << layerMembers.size() << "direct layer relationships";
            for (const auto &member : layerMembers) {
                if (member.isValid() && member != index) {
                    result.append(QVariant::fromValue(member));
                    qDebug() << "Added direct layer relationship:" << member.internalId();
                }
            }

            // Then check group ID relationships
            if (nodeIndex < d->groupIDs.size()) {
                const auto groupID = d->groupIDs.at(nodeIndex);
                if (groupID > 0) {
                    qDebug() << "Layer" << layerID << "belongs to group:" << groupID;
                    auto groupMembers = d->groupsMap.values(groupID);
                    for (const auto &member : groupMembers) {
                        if (member.isValid() && member != index && !result.contains(QVariant::fromValue(member))) {
                            result.append(QVariant::fromValue(member));
                            qDebug() << "Added group member:" << member.internalId();
                        }
                    }
                }
            }

            // Finally, check if we're part of any other layer's relationships
            for (auto it = d->groupsMap.begin(); it != d->groupsMap.end(); ++it) {
                if (it.value() == index) {
                    // We found a relationship where this layer is the target
                    auto sourceMembers = d->groupsMap.values(it.key());
                    for (const auto &member : sourceMembers) {
                        if (member.isValid() && member != index && !result.contains(QVariant::fromValue(member))) {
                            result.append(QVariant::fromValue(member));
                            qDebug() << "Added reverse relationship:" << member.internalId();
                        }
                    }
                }
            }
        }
        qDebug() << "Returning" << result.size() << "group indexes for node" << nodeIndex;
        return result;
    }
    case Roles::ClippingMaskIndexRole: {
        QModelIndex result;
        if (index.isValid()) {
            QPersistentModelIndex currentIndex(index);
            auto it = d->clippingMaskMap.find(currentIndex);
            if (it != d->clippingMaskMap.end() && it.value().isValid()) {
                qDebug() << "Found clipping mask base layer for" << nodeIndex << "at" << it.value().row();
                result = it.value();
            }
        }
        return QVariant::fromValue(result);
    }
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
    d->clippingMaskMap.clear();

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
    int row = -1;
    qint32 i = d->layerRecords.size();
    Private::Node currentNode;
    std::for_each(d->layerRecords.rbegin(), d->layerRecords.rend(), [this, d, &parentNodeIndex, &row, &i, &channelImageData, &currentNode](auto &record) {
        currentNode.recordIndex = i;
        currentNode.layerId = record.layerId();
        currentNode.parentNodeIndex = parentNodeIndex;
        currentNode.folderType = FolderType::NotFolder;
        currentNode.isCloseFolder = false;
        i--;
        auto imageData = channelImageData.at(i);
        imageData.setHeader(d->fileHeader);
        record.setImageData(imageData);
    
        const auto additionalLayerInformation = record.additionalLayerInformation();
        const auto layerId = additionalLayerInformation.value("lyid").template value<quint32>();

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
            const auto lsct = additionalLayerInformation.value("lsct");
            if (lsct.isValid()) {
                // Section divider settings can be either a direct value or a list
                if (lsct.typeId() == QMetaType::QVariantList) {
                    // Format: [type, key, subtype]
                    const auto list = lsct.toList();
                    if (!list.isEmpty()) {
                        bool ok = false;
                        int type = list[0].toInt(&ok);
                        if (ok) {
                            qDebug() << "Layer" << i << "section divider type:" << type;
                            // Map the type to QPsdSectionDividerSetting::Type
                            switch (static_cast<QPsdSectionDividerSetting::Type>(type)) {
                            case QPsdSectionDividerSetting::OpenFolder:
                                folderType = FolderType::OpenFolder;
                                break;
                            case QPsdSectionDividerSetting::ClosedFolder:
                                folderType = FolderType::ClosedFolder;
                                break;
                            case QPsdSectionDividerSetting::BoundingSectionDivider:
                                isCloseFolder = true;
                                break;
                            case QPsdSectionDividerSetting::AnyOtherTypeOfLayer:
                                // Non-folder layer, no special handling needed
                                break;
                            default:
                                qDebug() << "Unknown section divider type:" << type;
                                break;
                            }
                        }
                    }
                } else if (lsct.canConvert<QPsdSectionDividerSetting>()) {
                    // Try to handle as direct QPsdSectionDividerSetting
                    const auto dividerSetting = lsct.value<QPsdSectionDividerSetting>();
                    qDebug() << "Layer" << i << "direct section divider type:" << dividerSetting.type();
                    switch (dividerSetting.type()) {
                    case QPsdSectionDividerSetting::OpenFolder:
                        folderType = FolderType::OpenFolder;
                        break;
                    case QPsdSectionDividerSetting::ClosedFolder:
                        folderType = FolderType::ClosedFolder;
                        break;
                    case QPsdSectionDividerSetting::BoundingSectionDivider:
                        isCloseFolder = true;
                        break;
                    case QPsdSectionDividerSetting::AnyOtherTypeOfLayer:
                        // Non-folder layer, no special handling needed
                        break;
                    default:
                        qDebug() << "Unknown direct section divider type:" << dividerSetting.type();
                        break;
                    }
                } else {
                    qDebug() << "Layer" << i << "has unsupported section divider format";
                }
            }
        }
        
        row++;
        QModelIndex modelIndex;
        if (!isCloseFolder) {
            modelIndex = createIndex(row, 0, i);

            // Create and add node to tree first
            currentNode.recordIndex = i;
            currentNode.layerId = layerId;
            currentNode.parentNodeIndex = parentNodeIndex;
            currentNode.folderType = folderType;
            currentNode.isCloseFolder = isCloseFolder;
            currentNode.modelIndex = modelIndex;
            
            // Store the node first so we can reference it
            const int nodeIndex = d->treeNodeList.size();
            d->treeNodeList.append(currentNode);
            
            // Now we can use the node's layerId
            QPersistentModelIndex persistentIndex(modelIndex);
            
            // Ensure we have a valid layer ID
            if (layerId == 0 && nodeIndex < d->groupIDs.size()) {
                // If no explicit layer ID, use the group ID
                currentNode.layerId = d->groupIDs.at(nodeIndex);
                d->treeNodeList[nodeIndex] = currentNode;
            }
            const auto layerID = currentNode.layerId;

            qDebug() << "\nProcessing layer" << i << "with ID" << layerID;

            // Get section divider information first
            const auto additionalInfo = record.additionalLayerInformation();
            const auto lsct = additionalInfo.value("lsct");
            
            // Debug lsct data
            if (lsct.isValid()) {
                qDebug() << "Layer" << i << "lsct data:" << lsct;
                if (lsct.typeId() == QMetaType::QVariantList) {
                    qDebug() << "lsct list contents:" << lsct.toList();
                }
            }

            // Process group relationships
            qDebug() << "\nProcessing group relationships for layer" << i << "(ID:" << layerId << ")";

            // First, handle folder structure
            if (folderType != FolderType::NotFolder) {
                qDebug() << "Layer" << i << "is a folder";
                // This layer is a folder, store its index for its children
                d->groupsMap.insert(layerId, persistentIndex);
            }

            // Find the parent folder for this layer
            if (parentNodeIndex >= 0 && parentNodeIndex < d->treeNodeList.size()) {
                qDebug() << "Layer" << i << "has parent" << parentNodeIndex;
                
                // First establish relationship with parent
                const auto &parentNode = d->treeNodeList.at(parentNodeIndex);
                if (parentNode.folderType != FolderType::NotFolder) {
                    QPersistentModelIndex parentIndex(parentNode.modelIndex);
                    if (parentIndex.isValid()) {
                        d->groupsMap.insert(layerID, parentIndex);
                        d->groupsMap.insert(parentNode.layerId, persistentIndex);
                        qDebug() << "Added parent-child relationship between" << i << "and" << parentNodeIndex;
                    }
                }
                
                // Then find all siblings under this parent
                // Only establish sibling relationships if this is a group
                if (folderType != FolderType::NotFolder) {
                    for (const auto &otherNode : d->treeNodeList) {
                        if (otherNode.parentNodeIndex == parentNodeIndex && !otherNode.isCloseFolder) {
                            QPersistentModelIndex siblingIndex(otherNode.modelIndex);
                            if (siblingIndex.isValid() && siblingIndex != persistentIndex) {
                                // Add bidirectional relationships between siblings in the same group
                                d->groupsMap.insert(layerID, siblingIndex);
                                d->groupsMap.insert(otherNode.layerId, persistentIndex);
                                qDebug() << "Added sibling relationship between" << i << "and" << otherNode.recordIndex;
                            }
                        }
                    }
                }
            }

            // Always store self-reference
            d->groupsMap.insert(layerId, persistentIndex);

            // Debug output
            qDebug() << "\nFinal group relationships for layer" << i << ":";
            auto layerRelationships = d->groupsMap.values(layerID);
            for (const auto &rel : layerRelationships) {
                if (rel.isValid()) {
                    qDebug() << "  - Related to layer:" << rel.internalId();
                }
            }

            // Handle section divider information
            if (lsct.isValid()) {
                // Check for section divider settings
                quint32 groupID = 0;
                if (lsct.typeId() == QMetaType::QVariantList) {
                    const auto list = lsct.toList();
                    if (!list.isEmpty()) {
                        bool ok = false;
                        // Get the group ID from the list if available (usually in position 3)
                        if (list.size() > 3) {
                            groupID = list[3].toUInt(&ok);
                            if (!ok) {
                                qDebug() << "Failed to convert group ID to uint:" << list[3];
                                groupID = 0;
                            }
                        }
                        qDebug() << "Layer" << i << "section divider list:" << list;
                    }
                } else if (lsct.template canConvert<QPsdSectionDividerSetting>()) {
                    const auto dividerSetting = lsct.template value<QPsdSectionDividerSetting>();
                    // Try to get group ID from the setting if available
                    if (dividerSetting.type() > QPsdSectionDividerSetting::AnyOtherTypeOfLayer) {
                        groupID = dividerSetting.subType() == QPsdSectionDividerSetting::SceneGroup ? 1 : 0;
                        qDebug() << "Layer" << i << "direct section divider subtype:" << dividerSetting.subType();
                    }
                }
                
                if (groupID > 0) {
                            qDebug() << "Layer" << i << "is part of group" << groupID << "from section divider";
                            
                            // Store both the layer's own ID and group ID relationships
                            d->groupsMap.insert(groupID, persistentIndex);
                            d->groupsMap.insert(layerID, persistentIndex);

                            // Find and establish relationships with existing group members
                            auto groupMembers = d->groupsMap.values(groupID);
                            for (const auto &member : groupMembers) {
                                if (member.isValid() && member != persistentIndex) {
                                    const auto &memberNode = d->treeNodeList.at(member.internalId());
                                    // Add bidirectional relationships
                                    d->groupsMap.insert(memberNode.layerId, persistentIndex);
                                    d->groupsMap.insert(layerID, member);
                                    qDebug() << "Added bidirectional relationship between" << i << "and" << member.internalId();
                                }
                            }

                            // Debug group membership
                            auto updatedMembers = d->groupsMap.values(groupID);
                            qDebug() << "Group" << groupID << "now has" << updatedMembers.size() << "members";
                        }
                    } else {
                        // Try to determine group type from section divider settings
                        int dividerType = 0;
                        if (lsct.typeId() == QMetaType::QVariantList) {
                            const auto list = lsct.toList();
                            if (!list.isEmpty()) {
                                bool ok = false;
                                dividerType = list[0].toInt(&ok);
                                if (!ok) {
                                    qDebug() << "Failed to convert divider type to int:" << list[0];
                                }
                            }
                        } else if (lsct.template canConvert<QPsdSectionDividerSetting>()) {
                            const auto dividerSetting = lsct.template value<QPsdSectionDividerSetting>();
                            dividerType = static_cast<int>(dividerSetting.type());
                        }

                        if (dividerType > 0) { // Layer is part of a group but no explicit group ID
                            qDebug() << "Layer" << i << "is a group with type" << dividerType;
                        
                            // Store both the layer's own ID and group ID relationships
                            d->groupsMap.insert(layerID, persistentIndex);
                            
                            // Look for any existing members of this group
                            for (int j = i + 1; j < d->layerRecords.size(); ++j) {
                                const auto &memberRecord = d->layerRecords.at(j);
                                const auto memberInfo = memberRecord.additionalLayerInformation();
                                const auto memberLsct = memberInfo.value("lsct");
                                
                                qDebug() << "Checking layer" << j << "for membership in group" << layerID;
                                
                                if (memberLsct.isValid()) {
                                    QList<QVariant> memberData = memberLsct.toList();
                                    if (!memberData.isEmpty()) {
                                        bool memberOk = false;
                                        const auto memberType = memberData.first().toUInt(&memberOk);
                                        if (!memberOk) {
                                            qDebug() << "Failed to convert member type to uint:" << memberData.first();
                                            continue;
                                        }
                                        
                                        // Check if this is a valid section divider type
                                        if (memberType != QPsdSectionDividerSetting::OpenFolder &&
                                            memberType != QPsdSectionDividerSetting::ClosedFolder &&
                                            memberType != QPsdSectionDividerSetting::BoundingSectionDivider) {
                                            qDebug() << "Invalid section divider type:" << memberType;
                                            continue;
                                        }
                                        
                                        if (memberData.size() > 1) {
                                            bool groupOk = false;
                                            quint32 memberGroupID = memberData.at(1).toUInt(&groupOk);
                                            if (!groupOk) {
                                                qDebug() << "Failed to convert group ID to uint:" << memberData.at(1);
                                                continue;
                                            }
                                            qDebug() << "Layer" << j << "has group ID" << memberGroupID;
                                            
                                            if (memberGroupID == layerID) {
                                                // This layer is a member of our group
                                                for (int k = 0; k < d->treeNodeList.size(); ++k) {
                                                    const auto &memberNode = d->treeNodeList.at(k);
                                                    if (memberNode.recordIndex == j) {
                                                        QPersistentModelIndex memberIndex(memberNode.modelIndex);
                                                        if (memberIndex.isValid()) {
                                                            // Add bidirectional relationships
                                                            d->groupsMap.insert(layerID, memberIndex);
                                                            d->groupsMap.insert(memberNode.layerId, persistentIndex);
                                                            
                                                            // If this is a folder, also establish parent-child relationship
                                                            if (memberNode.folderType != FolderType::NotFolder) {
                                                                d->groupsMap.insert(memberNode.layerId, persistentIndex);
                                                                d->groupsMap.insert(layerID, memberIndex);
                                                            }
                                                            
                                                            qDebug() << "Added bidirectional relationship between" << i << "and" << j;
                                                        }
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

            // Handle clipping masks
            QPersistentModelIndex currentIndex(modelIndex);
            if (record.clipping() == QPsdLayerRecord::Clipping::Base) {
                qDebug() << "\nLayer" << i << "is a base layer";
                // Register this as a base layer by mapping it to itself
                d->clippingMaskMap.insert(currentIndex, currentIndex);
                
                // Also store in group map for relationship tracking
                d->groupsMap.insert(currentNode.layerId, currentIndex);
            } else if (record.clipping() == QPsdLayerRecord::Clipping::NonBase) {
                qDebug() << "\nLayer" << i << "is a clipped layer";
                
                // Find the base layer by looking at previous layers in the same group
                QPersistentModelIndex baseIndex;
                const auto parentId = currentNode.parentNodeIndex;
                
                // Look for base layers in reverse order (closest first)
                // Start from current layer and work backwards
                int searchStart = i - 1;  // Start from previous layer
                while (searchStart >= 0) {
                    // Ensure we're within bounds of our data structures
                    if (searchStart >= d->treeNodeList.size() || searchStart >= d->layerRecords.size()) {
                        break;
                    }
                    
                    const auto &baseNode = d->treeNodeList.at(searchStart);
                    // Only consider layers in the same group
                    if (baseNode.parentNodeIndex == parentId && !baseNode.isCloseFolder) {
                        const auto &baseRecord = d->layerRecords.at(baseNode.recordIndex);
                        if (baseRecord.clipping() == QPsdLayerRecord::Clipping::Base) {
                            // Found a valid base layer
                            baseIndex = QPersistentModelIndex(baseNode.modelIndex);
                            // Set up clipping mask relationship
                            d->clippingMaskMap.insert(currentIndex, baseIndex);
                            setData(currentIndex, QVariant::fromValue(baseIndex), QPsdLayerTreeItemModel::Roles::ClippingMaskIndexRole);
                            qDebug() << "Found base layer" << baseNode.recordIndex << "for clipped layer" << i;
                            break;
                        }
                    }
                    searchStart--;
                }
                
                if (baseIndex.isValid()) {
                    // Store the clipping mask relationship
                    d->clippingMaskMap.insert(currentIndex, baseIndex);
                    qDebug() << "Layer" << i << "is clipped to base layer" << baseIndex.internalId();
                    
                    // Also store the relationship in the group map
                    const auto &baseNode = d->treeNodeList.at(baseIndex.internalId());
                    d->groupsMap.insert(currentNode.layerId, baseIndex);
                    d->groupsMap.insert(baseNode.layerId, currentIndex);
                    qDebug() << "Added bidirectional group relationship for clipping mask between" << i << "and" << baseIndex.internalId();
                } else {
                    qWarning() << "No valid base layer found for clipped layer" << i;
                }
            }
        }
        // Update parent node index for folder structure
        if (folderType != FolderType::NotFolder) {
            parentNodeIndex = i;
        }

        if (isCloseFolder) {
            if (parentNodeIndex >= 0 && parentNodeIndex < d->treeNodeList.size()) {
                const auto &parentNode = d->treeNodeList.at(parentNodeIndex);
                if (parentNode.isCloseFolder) {
                    parentNodeIndex = parentNode.parentNodeIndex;
                }
            }
        }
    });

    // No additional group relationship processing needed - relationships are established during layer processing

    endResetModel();
}

QSize QPsdLayerTreeItemModel::size() const
{
    return d->fileHeader.size();
}

QT_END_NAMESPACE

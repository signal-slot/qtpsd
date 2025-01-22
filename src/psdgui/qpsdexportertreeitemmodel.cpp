// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdexportertreeitemmodel.h"

#include <QtPsdGui/QPsdFolderLayerItem>

QT_BEGIN_NAMESPACE

class QPsdExporterTreeItemModel::Private
{
public:
    Private(const QPsdExporterTreeItemModel *model);
    ~Private();

    bool isValidIndex(const QModelIndex &index) const;

    const QPsdExporterTreeItemModel *q;
    QMetaObject::Connection sourceModelConnection;

    QString psdFileName;
    QMap<QString, QPsdAbstractLayerItem::ExportHint> layerHints;
    QMap<QString, QVariantMap> exportHints;
};

QPsdExporterTreeItemModel::Private::Private(const ::QPsdExporterTreeItemModel *model) : q(model)
{
}

QPsdExporterTreeItemModel::Private::~Private()
{
}

bool QPsdExporterTreeItemModel::Private::isValidIndex(const QModelIndex &index) const
{
    return index.isValid() && index.model() == q;
}

QPsdExporterTreeItemModel::QPsdExporterTreeItemModel(QObject *parent)
    : QIdentityProxyModel(parent), d(new Private(this))
{
}

QPsdExporterTreeItemModel::~QPsdExporterTreeItemModel()
{
}

QHash<int, QByteArray> QPsdExporterTreeItemModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();
    roles.insert(Roles::LayerIdRole, QByteArrayLiteral("LayerId"));
    roles.insert(Roles::NameRole, QByteArrayLiteral("Name"));
    roles.insert(Roles::LayerRecordObjectRole, QByteArrayLiteral("LayerRecordObject"));
    roles.insert(Roles::FolderTypeRole, QByteArrayLiteral("FolderType"));
    roles.insert(Roles::GroupIndexesRole, QByteArrayLiteral("GroupIndexes"));
    roles.insert(Roles::ClippingMaskIndexRole, QByteArrayLiteral("ClippingMaskIndex"));
    roles.insert(Roles::LayerItemObjectRole, QByteArrayLiteral("LayerItemObject"));
    roles.insert(Roles::LayerHintRole, QByteArrayLiteral("LayerHint"));

    return roles;
}

QVariant QPsdExporterTreeItemModel::data(const QModelIndex &index, int role) const
{
    if (!d->isValidIndex(index))
        return QVariant();

    switch (role) {
    case Roles::LayerHintRole:
        return QVariant::fromValue(layerHint(index));
    default:
        return sourceModel()->data(mapToSource(index), role);
    }
}

void QPsdExporterTreeItemModel::fromParser(const QPsdParser &parser,
    const QMap<QString, QPsdAbstractLayerItem::ExportHint> layerHints,
    const QMap<QString, QVariantMap> exportHints)
{
    d->layerHints = layerHints;
    d->exportHints = exportHints;

    fromParser(parser);
}

void QPsdExporterTreeItemModel::fromParser(const QPsdParser &parser)
{
    QPsdGuiAbstractLayerTreeItemModel *parentModel = dynamic_cast<QPsdGuiAbstractLayerTreeItemModel *>(sourceModel());
    if (parentModel == nullptr) {
        return;
    }

    parentModel->fromParser(parser);
}

QSize QPsdExporterTreeItemModel::size() const
{
    QPsdGuiAbstractLayerTreeItemModel *parentModel = dynamic_cast<QPsdGuiAbstractLayerTreeItemModel *>(sourceModel());
    if (parentModel == nullptr) {
        return {};
    }

    return parentModel->size();
}

QVariantMap QPsdExporterTreeItemModel::exportHint(const QString &exporterKey) const
{
    return d->exportHints.value(exporterKey);
}

void QPsdExporterTreeItemModel::setExportHint(const QString &exporterKey, const QVariantMap exportHint)
{
    d->exportHints.insert(exporterKey, exportHint);
}

qint32 QPsdExporterTreeItemModel::layerId(const QModelIndex &index) const
{
    QPsdGuiAbstractLayerTreeItemModel *parentModel = dynamic_cast<QPsdGuiAbstractLayerTreeItemModel *>(sourceModel());
    if (parentModel == nullptr) {
        return {};
    }

    return parentModel->layerId(mapToSource(index));
}

QString QPsdExporterTreeItemModel::layerName(const QModelIndex &index) const
{
    QPsdGuiAbstractLayerTreeItemModel *parentModel = dynamic_cast<QPsdGuiAbstractLayerTreeItemModel *>(sourceModel());
    if (parentModel == nullptr) {
        return {};
    }

    return parentModel->layerName(mapToSource(index));
}

const QPsdLayerRecord *QPsdExporterTreeItemModel::layerRecord(const QModelIndex &index) const
{
    QPsdGuiAbstractLayerTreeItemModel *parentModel = dynamic_cast<QPsdGuiAbstractLayerTreeItemModel *>(sourceModel());
    if (parentModel == nullptr) {
        return {};
    }

    return parentModel->layerRecord(mapToSource(index));
}

enum QPsdAbstractLayerTreeItemModel::FolderType QPsdExporterTreeItemModel::folderType(const QModelIndex &index) const
{
    QPsdGuiAbstractLayerTreeItemModel *parentModel = dynamic_cast<QPsdGuiAbstractLayerTreeItemModel *>(sourceModel());
    if (parentModel == nullptr) {
        return {};
    }

    return parentModel->folderType(mapToSource(index));
}

QList<QPersistentModelIndex> QPsdExporterTreeItemModel::groupIndexes(const QModelIndex &index) const
{
    QPsdGuiAbstractLayerTreeItemModel *parentModel = dynamic_cast<QPsdGuiAbstractLayerTreeItemModel *>(sourceModel());
    if (parentModel == nullptr) {
        return {};
    }

    return parentModel->groupIndexes(mapToSource(index));
}

QPersistentModelIndex QPsdExporterTreeItemModel::clippingMaskIndex(const QModelIndex &index) const
{
    QPsdGuiAbstractLayerTreeItemModel *parentModel = dynamic_cast<QPsdGuiAbstractLayerTreeItemModel *>(sourceModel());
    if (parentModel == nullptr) {
        return {};
    }

    return parentModel->clippingMaskIndex(mapToSource(index));
}

const QPsdAbstractLayerItem *QPsdExporterTreeItemModel::layerItem(const QModelIndex &index) const
{
    QPsdGuiAbstractLayerTreeItemModel *parentModel = dynamic_cast<QPsdGuiAbstractLayerTreeItemModel *>(sourceModel());
    if (parentModel == nullptr) {
        return {};
    }

    return parentModel->layerItem(mapToSource(index));
}

QPsdAbstractLayerItem::ExportHint QPsdExporterTreeItemModel::layerHint(const QModelIndex &index) const
{
    QPsdGuiAbstractLayerTreeItemModel *parentModel = dynamic_cast<QPsdGuiAbstractLayerTreeItemModel *>(sourceModel());
    if (parentModel == nullptr) {
        return {};
    }

    return d->layerHints.value(QString::number(layerId(index)));
}

void QPsdExporterTreeItemModel::setLayerHint(const QModelIndex &index, const QPsdAbstractLayerItem::ExportHint layerHint)
{
    d->layerHints.insert(QString::number(layerId(index)), layerHint);
}

QMap<QString, QPsdAbstractLayerItem::ExportHint> QPsdExporterTreeItemModel::layerHints() const
{
    return d->layerHints;
}

QMap<QString, QVariantMap> QPsdExporterTreeItemModel::exportHints() const
{
    return d->exportHints;
}

QT_END_NAMESPACE

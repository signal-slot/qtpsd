// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef QPSDEXPORTERTREEITEMMODEL_H
#define QPSDEXPORTERTREEITEMMODEL_H

#include <QtPsdExporter/qpsdexporterglobal.h>
#include <QtPsdGui/QPsdGuiLayerTreeItemModel>

#include <QtCore/QIdentityProxyModel>

QT_BEGIN_NAMESPACE

class Q_PSDEXPORTER_EXPORT QPsdExporterTreeItemModel : public QIdentityProxyModel
{
    Q_OBJECT

public:
    enum Roles {
        LayerIdRole = QPsdLayerTreeItemModel::Roles::LayerIdRole,
        NameRole = QPsdLayerTreeItemModel::Roles::NameRole,
        RectRole = QPsdLayerTreeItemModel::Roles::RectRole,
        FolderTypeRole = QPsdLayerTreeItemModel::Roles::FolderTypeRole,
        GroupIndexesRole = QPsdLayerTreeItemModel::Roles::GroupIndexesRole,
        ClippingMaskIndexRole = QPsdLayerTreeItemModel::Roles::ClippingMaskIndexRole,
        LayerItemObjectRole = QPsdGuiLayerTreeItemModel::Roles::LayerItemObjectRole,
    };

    explicit QPsdExporterTreeItemModel(QObject *parent = nullptr);
    ~QPsdExporterTreeItemModel() override;

    QPsdGuiLayerTreeItemModel *guiLayerTreeItemModel() const;

    QHash<int, QByteArray> roleNames() const override;

    QVariantMap exportHint(const QString& exporterKey) const;
    void updateExportHint(const QString &key, const QVariantMap &hint);

    QPsdAbstractLayerItem::ExportHint layerHint(const QModelIndex &index) const;
    void setLayerHint(const QModelIndex &index, const QPsdAbstractLayerItem::ExportHint exportHint);

    QSize size() const;

    bool isVisible(const QModelIndex &index) const;
    void setVisible(const QModelIndex &index, bool visible);

    const QPsdAbstractLayerItem *layerItem(const QModelIndex &index) const;
    qint32 layerId(const QModelIndex &index) const;
    QString layerName(const QModelIndex &index) const;
    QRect rect(const QModelIndex &index) const;
    QList<QPersistentModelIndex> groupIndexes(const QModelIndex &index) const;

    QFileInfo fileInfo() const;
    QString fileName() const;
    QString errorMessage() const;

public slots:
    void load(const QString &fileName);
    void save();

private slots:
    void setErrorMessage(const QString &errorMessage);

signals:
    void fileInfoChanged(const QFileInfo &fileInfo);
    void errorOccurred(const QString &errorMessage);

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDEXPORTERTREEITEMMODEL_H

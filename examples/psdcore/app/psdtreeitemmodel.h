// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef PSDTREEITEMMODEL_H
#define PSDTREEITEMMODEL_H

#include <QtPsdGui/QPsdLayerTree>
#include <QtPsdGui/QPsdGuiLayerTreeItemModel>

#include <QIdentityProxyModel>
#include <QFileInfo>

class PsdTreeItemModel : public QIdentityProxyModel
{
    Q_OBJECT

    Q_PROPERTY(QFileInfo fileInfo READ fileInfo NOTIFY fileInfoChanged)

public:
    enum Roles {
        LayerIdRole = QPsdGuiLayerTreeItemModel::LayerIdRole,
        NameRole = QPsdGuiLayerTreeItemModel::NameRole,
        LayerRecordObjectRole = QPsdGuiLayerTreeItemModel::LayerRecordObjectRole,
        FolderTypeRole = QPsdGuiLayerTreeItemModel::FolderTypeRole,
        GroupIndexesRole = QPsdGuiLayerTreeItemModel::GroupIndexesRole,
        ClippingMaskIndexRole = QPsdGuiLayerTreeItemModel::ClippingMaskIndexRole,
        LayerItemObjectRole = QPsdGuiLayerTreeItemModel::Roles::LayerItemObjectRole,
        VisibleRole,
        ExportIdRole,
    };
    enum Column {
        Name = 0,
        Visible,
        Export
    };

    explicit PsdTreeItemModel(QObject *parent = nullptr);
    ~PsdTreeItemModel() override;

    // Header:
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    QFileInfo fileInfo() const;
    QString fileName() const;
    QString errorMessage() const;
    QSize size() const;

    const QPsdFolderLayerItem *layerTree() const;
    QVariantMap exportHint(const QString& exporterKey) const;
    void updateExportHint(const QString &key, const QVariantMap &hint);

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

#endif // PSDTREEITEMMODEL_H

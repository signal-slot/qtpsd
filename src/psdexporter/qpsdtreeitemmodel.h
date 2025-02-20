// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef QPSDTREEITEMMODEL_H
#define QPSDTREEITEMMODEL_H

#include <QtPsdExporter/qpsdexporterglobal.h>
#include <QtPsdGui/QPsdGuiLayerTreeItemModel>

#include <QFileInfo>

QT_BEGIN_NAMESPACE

class Q_PSDEXPORTER_EXPORT QPsdTreeItemModel : public QPsdGuiLayerTreeItemModel
{
    Q_OBJECT

    Q_PROPERTY(QFileInfo fileInfo READ fileInfo NOTIFY fileInfoChanged)

public:
    enum Roles {
        VisibleRole = QPsdGuiLayerTreeItemModel::LayerItemObjectRole + 1,
        ExportIdRole,
    };
    enum Column {
        Name = 0,
        Visible,
        Export
    };

    explicit QPsdTreeItemModel(QObject *parent = nullptr);
    ~QPsdTreeItemModel() override;

    // Header:
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    bool isVisible(const QModelIndex &index) const;
    QString exportId(const QModelIndex &index) const;

    QFileInfo fileInfo() const;
    QString fileName() const;
    QString errorMessage() const;

    QT_DEPRECATED
    const QPsdFolderLayerItem *layerTree() const;
    QVariantMap exportHint(const QString& exporterKey) const;
    void updateExportHint(const QString &key, const QVariantMap &hint);

    QPsdAbstractLayerItem::ExportHint layerHint(const QModelIndex &index) const;
    void setLayerHint(const QModelIndex &index, const QPsdAbstractLayerItem::ExportHint exportHint);

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

#endif // QPSDTREEITEMMODEL_H

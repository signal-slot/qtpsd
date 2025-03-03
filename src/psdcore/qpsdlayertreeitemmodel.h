// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDLAYERTREEITEMMODEL_H
#define QPSDLAYERTREEITEMMODEL_H

#include <QtPsdCore/qpsdparser.h>

#include <QAbstractItemModel>
#include <QFileInfo>
#include <QtCore/QByteArray>

// Define the _ba suffix for QByteArray literals
inline QByteArray operator"" _ba(const char *str, size_t size)
{
    return QByteArray(str, static_cast<int>(size));
}

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdLayerTreeItemModel : public QAbstractItemModel
{
    Q_OBJECT

    Q_PROPERTY(QFileInfo fileInfo READ fileInfo NOTIFY fileInfoChanged)

public:
    enum Roles {
        LayerIdRole = Qt::UserRole + 1,
        NameRole,
        RectRole,
        LayerRecordObjectRole,
        FolderTypeRole,
        GroupIndexesRole,
        ClippingMaskIndexRole,
    };
    enum Column {
        LayerIdColumn = 0,
        NameColumn,
        FolderTypeColumn,
    };
    enum class FolderType {
        NotFolder = 0,
        OpenFolder,
        ClosedFolder,
    };

    explicit QPsdLayerTreeItemModel(QObject *parent = nullptr);
    virtual ~QPsdLayerTreeItemModel();

    QHash<int, QByteArray> roleNames() const override;

    // Basic functionality:
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    virtual void fromParser(const QPsdParser &parser);
    QSize size() const;

    qint32 layerId(const QModelIndex &index) const;
    QString layerName(const QModelIndex &index) const;
    QRect rect(const QModelIndex &index) const;
    const QPsdLayerRecord *layerRecord(const QModelIndex &index) const;
    FolderType folderType(const QModelIndex &index) const;
    QList<QPersistentModelIndex> groupIndexes(const QModelIndex &index) const;
    QPersistentModelIndex clippingMaskIndex(const QModelIndex &index) const;

    QFileInfo fileInfo() const;
    QString fileName() const;
    QString errorMessage() const;

public slots:
    void load(const QString &fileName);

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

#endif // QPSDLAYERTREEITEMMODEL_H

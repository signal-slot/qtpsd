#ifndef PSDTREEITEMMODEL_H
#define PSDTREEITEMMODEL_H

#include <QtPsdExporter/QPsdExporterTreeItemModel>
#include <QtPsdGui/QPsdGuiLayerTreeItemModel>
#include <QtPsdWidget/QPsdWidgetTreeItemModel>

#include <QtCore/QFileInfo>

class PsdTreeItemModel : public QPsdExporterTreeItemModel
{
    Q_OBJECT

public:
    enum Roles {
        ExportIdRole = QPsdGuiLayerTreeItemModel::LayerItemObjectRole + 10,
        PlaceholderTextRole,
    };
    enum Column {
        Name = 0,
        Visible
    };

    explicit PsdTreeItemModel(QObject *parent = nullptr);
    ~PsdTreeItemModel() override;

    QPsdWidgetTreeItemModel *widgetModel() const;

    // Header:
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    QString exportId(const QModelIndex &index) const;
    bool isVisible(const QModelIndex &index) const;
    void setVisible(const QModelIndex &index, bool visible);

private:
    class Private;
    QScopedPointer<Private> d;
};

#endif // PSDTREEITEMMODEL_H

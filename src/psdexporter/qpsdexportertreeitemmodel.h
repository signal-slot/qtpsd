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

    Q_PROPERTY(QFileInfo fileInfo READ fileInfo NOTIFY fileInfoChanged)

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

    struct ExportHint {
        enum Type {
            Embed,
            Merge,
            Component,
            Native,
            Skip,
            None,
        };
        enum NativeComponent {
            Container,
            TouchArea,
            Button,
            Button_Highlighted,
        };

        QString id;
        Type type = Embed;
        QString componentName;
        NativeComponent baseElement = Container;
        bool visible = true;
        QSet<QString> properties;

        bool isDefaultValue() const {
            return id.isEmpty() && type == Embed && componentName.isEmpty() && baseElement == Container;
        }

        static NativeComponent nativeName2Code(const QString &name) {

#define IF(x) if (name == u###x##_s) \
            return x; \
            else
            IF(Container)
            IF(TouchArea)
            IF(Button)
            IF(Button_Highlighted)
#undef IF
            {
                qWarning() << name << "is not a valid NativeComponent";
            }
            return Container;
        }
        static QString nativeCode2Name(NativeComponent code) {
            auto parentheses = [](const QString &s) {
                QString ret = s;
                if (ret.count('_'_L1) == 1) {
                    const auto pos = ret.indexOf('_'_L1);
                    ret.replace(pos, 1, '('_L1);
                    ret.append(')'_L1);
                }
                return ret;
            };
            switch (code) {
#define CASE(x) case x: return parentheses(u###x##_s)
            CASE(Container);
            CASE(TouchArea);
            CASE(Button);
            CASE(Button_Highlighted);
#undef CASE
            default:
                qWarning() << code << "is not a valid NativeComponent";
                break;
            }
            return QString();
        }
    };

    explicit QPsdExporterTreeItemModel(QObject *parent = nullptr);
    ~QPsdExporterTreeItemModel() override;

    void setSourceModel(QAbstractItemModel *sourceModel) override;
    QPsdGuiLayerTreeItemModel *guiLayerTreeItemModel() const;

    QHash<int, QByteArray> roleNames() const override;

    QVariantMap exportHint(const QString& exporterKey) const;
    void updateExportHint(const QString &key, const QVariantMap &hint);

    ExportHint layerHint(const QModelIndex &index) const;
    void setLayerHint(const QModelIndex &index, const ExportHint exportHint);

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

// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdexportertreeitemmodel.h"

#include <QtPsdCore/QPsdParser>
#include <QtPsdGui/QPsdFolderLayerItem>
#include <QtPsdGui/QPsdGuiLayerTreeItemModel>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtGui/QBrush>
#include <QtGui/QIcon>

class QPsdExporterTreeItemModel::Private
{
public:
    Private(const ::QPsdExporterTreeItemModel *model);
    ~Private();

    QFileInfo setDefaultHintFile(const QString &psdFileName);
    void loadHintFile();
    QJsonDocument loadHint(const QString &hintFileName);

    bool isValidIndex(const QModelIndex &index) const;

    const ::QPsdExporterTreeItemModel *q;
    QList<QMetaObject::Connection> sourceConnections;

    QString hintFileName;
    QFileInfo hintFileInfo;
    QString errorMessage;

    QMap<QString, ExportHint> layerHints;
    QMap<QString, QVariantMap> exportHints;
};

#define HINTFILE_MAGIC_KEY "qtpsdparser.hint"_L1
#define HINTFILE_MAGIC_VERSION 1
#define HINTFILE_LAYER_HINTS_KEY "layers"_L1
#define HINTFILE_EXPORT_HINTS_KEY "exports"_L1

QPsdExporterTreeItemModel::Private::Private(const ::QPsdExporterTreeItemModel *model) : q(model)
{
}

QPsdExporterTreeItemModel::Private::~Private()
{
}

QFileInfo QPsdExporterTreeItemModel::Private::setDefaultHintFile(const QString &psdFileName)
{
    QFileInfo fileInfo(psdFileName);
    QString hintFileName = u"%1.%2"_s.arg(
        fileInfo.suffix().toLower() == "psd"_L1 ? fileInfo.completeBaseName() : fileInfo.fileName(), "psd_"_L1);

    this->hintFileInfo = QFileInfo(fileInfo.dir(), hintFileName);
    this->hintFileName = this->hintFileInfo.filePath();

    return hintFileInfo;
}

void QPsdExporterTreeItemModel::Private::loadHintFile()
{
    layerHints.clear();
    exportHints.clear();

    QFileInfo hintFileInfo(hintFileName);
    if (hintFileInfo.exists()) {
        QJsonDocument hintDoc = loadHint(hintFileInfo.absoluteFilePath());
        QJsonObject root = hintDoc.object();
        QJsonObject layerHintsJson = root.value(HINTFILE_LAYER_HINTS_KEY).toObject();
        for (const auto &idstr: layerHintsJson.keys()) {
            QVariantMap settings = layerHintsJson.value(idstr).toObject().toVariantMap();
            QStringList properties = settings.value("properties"_L1).toStringList();
            ExportHint exportHint {
                settings.value("id"_L1).toString(),
                static_cast<ExportHint::Type>(settings.value("type"_L1).toInt()),
                settings.value("name"_L1).toString(),
                static_cast<ExportHint::NativeComponent>(settings.value("native"_L1).toInt()),
                settings.value("visible"_L1).toBool(),
                QSet<QString>(properties.begin(), properties.end()),
            };
            layerHints.insert(idstr, exportHint);
        }

        QJsonObject exportHintsJson = root.value(HINTFILE_EXPORT_HINTS_KEY).toObject();
        for (const auto &exporterKey : exportHintsJson.keys()) {
            QVariantMap map = exportHintsJson.value(exporterKey).toObject().toVariantMap();
            exportHints.insert(exporterKey, map);
        }
    }
}

QJsonDocument QPsdExporterTreeItemModel::Private::loadHint(const QString &hintFileName)
{
    QFile file(hintFileName);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray buf = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(buf);
        QJsonObject root = doc.object();
        if (root.value(HINTFILE_MAGIC_KEY) == HINTFILE_MAGIC_VERSION) {
            return doc;
        }
    }

    return {};
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

void QPsdExporterTreeItemModel::setSourceModel(QAbstractItemModel *source)
{
    QIdentityProxyModel::setSourceModel(source);

    beginResetModel();
    for (const auto &conn: d->sourceConnections) {
        disconnect(conn);
    }
    d->sourceConnections.clear();

    QPsdLayerTreeItemModel *model = dynamic_cast<QPsdLayerTreeItemModel *>(source);
    d->sourceConnections = QList<QMetaObject::Connection> {
        connect(source, &QAbstractItemModel::modelReset, this, [this]() {
            beginResetModel();
            d->loadHintFile();
            endResetModel();
        }),
        connect(model, &QPsdLayerTreeItemModel::fileInfoChanged, this, [this](const QFileInfo &fileInfo) {
            emit fileInfoChanged(fileInfo);
        }),
        connect(model, &QPsdLayerTreeItemModel::errorOccurred, this, [this](const QString &errorMessage) {
            setErrorMessage(errorMessage);
        }),
    };

    endResetModel();
}

QPsdGuiLayerTreeItemModel *QPsdExporterTreeItemModel::guiLayerTreeItemModel() const
{
    return dynamic_cast<QPsdGuiLayerTreeItemModel *>(sourceModel());
}

QHash<int, QByteArray> QPsdExporterTreeItemModel::roleNames() const
{
    auto roles = QIdentityProxyModel::roleNames();
    roles.insert(Roles::LayerIdRole, QByteArrayLiteral("LayerId"));
    roles.insert(Roles::NameRole, QByteArrayLiteral("Name"));
    roles.insert(Roles::RectRole, QByteArrayLiteral("Rect"));
    roles.insert(Roles::FolderTypeRole, QByteArrayLiteral("FolderType"));
    roles.insert(Roles::GroupIndexesRole, QByteArrayLiteral("GroupIndexes"));
    roles.insert(Roles::ClippingMaskIndexRole, QByteArrayLiteral("ClippingMaskIndex"));
    roles.insert(Roles::LayerItemObjectRole, QByteArrayLiteral("LayerItemObject"));

    return roles;
}

QVariantMap QPsdExporterTreeItemModel::exportHint(const QString& exporterKey) const
{
    return d->exportHints.value(exporterKey);
}

void QPsdExporterTreeItemModel::updateExportHint(const QString &exporterKey, const QVariantMap &hint)
{
    d->exportHints.insert(exporterKey, hint);
}

QPsdExporterTreeItemModel::ExportHint QPsdExporterTreeItemModel::layerHint(const QModelIndex &index) const
{
    const QPsdAbstractLayerItem *item = layerItem(index);
    const QString idstr = QString::number(item->id());

    return d->layerHints.value(idstr);
}

void QPsdExporterTreeItemModel::setLayerHint(const QModelIndex &index, const ExportHint exportHint)
{
    const QPsdAbstractLayerItem *item = layerItem(index);
    const QString idstr = QString::number(item->id());

    d->layerHints.insert(idstr, exportHint);

    emit dataChanged(index, index);
}

QSize QPsdExporterTreeItemModel::size() const
{
    const auto *model = dynamic_cast<const QPsdLayerTreeItemModel *>(sourceModel());
    if (model) {
        return model->size();
    } else {
        return {};
    }
}

const QPsdAbstractLayerItem *QPsdExporterTreeItemModel::layerItem(const QModelIndex &index) const
{
    const auto *model = dynamic_cast<const QPsdGuiLayerTreeItemModel *>(sourceModel());
    if (model) {
        return model->layerItem(mapToSource(index));
    } else {
        return nullptr;
    }
}

qint32 QPsdExporterTreeItemModel::layerId(const QModelIndex &index) const
{
    const auto *model = dynamic_cast<const QPsdLayerTreeItemModel *>(sourceModel());
    if (model) {
        return model->layerId(mapToSource(index));
    } else {
        return -1;
    }
}

QString QPsdExporterTreeItemModel::layerName(const QModelIndex &index) const
{
    const auto *model = dynamic_cast<const QPsdLayerTreeItemModel *>(sourceModel());
    if (model) {
        return model->layerName(mapToSource(index));
    } else {
        return {};
    }
}

QRect QPsdExporterTreeItemModel::rect(const QModelIndex &index) const
{
    const auto *model = dynamic_cast<const QPsdLayerTreeItemModel *>(sourceModel());
    if (model) {
        return model->rect(mapToSource(index));
    } else {
        return {};
    }
}

QList<QPersistentModelIndex> QPsdExporterTreeItemModel::groupIndexes(const QModelIndex &index) const
{
    const auto *model = dynamic_cast<const QPsdLayerTreeItemModel *>(sourceModel());
    if (model) {
        const auto sourceGroupIndexes = model->groupIndexes(index);

        QList<QPersistentModelIndex> res;
        for (const auto &i : sourceGroupIndexes) {
            res.append(mapFromSource(i));
        }
        return res;
    } else {
        return {};
    }
}

QFileInfo QPsdExporterTreeItemModel::fileInfo() const
{
    auto *model = dynamic_cast<QPsdLayerTreeItemModel *>(sourceModel());
    if (model) {
        return model->fileInfo();
    } else {
        return {};
    }
}

QString QPsdExporterTreeItemModel::fileName() const
{
    auto *model = dynamic_cast<QPsdLayerTreeItemModel *>(sourceModel());
    if (model) {
        return model->fileName();
    } else {
        return {};
    }
}

QString QPsdExporterTreeItemModel::errorMessage() const
{
    return d->errorMessage;
}

void QPsdExporterTreeItemModel::load(const QString &fileName)
{
    d->setDefaultHintFile(fileName);
    auto *model = dynamic_cast<QPsdLayerTreeItemModel *>(sourceModel());
    model->load(fileName);
}

void QPsdExporterTreeItemModel::save()
{
    QJsonDocument doc;
    QJsonObject layerHints;
    std::function<void(const QModelIndex &)> traverse = [&](const QModelIndex &index) {
        if (index.isValid()) {
            const auto layer = layerItem(index);
            const auto lyid = layer->id();
            const auto idstr = QString::number(lyid);

            const auto exportHint = layerHint(index);
            if (!exportHint.isDefaultValue()) {
                QStringList propList = exportHint.properties.values();
                std::sort(propList.begin(), propList.end(), std::less<QString>());
                QJsonObject object;
                if (!exportHint.id.isEmpty()) {
                    object.insert("id"_L1, exportHint.id);
                }
                object.insert("type"_L1, static_cast<int>(exportHint.type));
                if (!exportHint.componentName.isEmpty()) {
                    object.insert("name"_L1, exportHint.componentName);
                }
                object.insert("native"_L1, static_cast<int>(exportHint.baseElement));
                object.insert("visible"_L1, exportHint.visible);
                if (!propList.isEmpty())
                    object.insert("properties"_L1, QJsonArray::fromStringList(propList));
                layerHints.insert(idstr, object);
            }
        }
        for (int i = 0; i < rowCount(index); i++) {
            traverse(this->index(i, 0, index));
        }
    };
    traverse({});

    QJsonObject exportHints;
    for (const auto &exporterKey : d->exportHints.keys()) {
        exportHints.insert(exporterKey, QJsonObject::fromVariantMap(d->exportHints.value(exporterKey)));
    }

    QJsonObject root;
    root.insert(HINTFILE_MAGIC_KEY, HINTFILE_MAGIC_VERSION);
    root.insert(HINTFILE_LAYER_HINTS_KEY, layerHints);
    root.insert(HINTFILE_EXPORT_HINTS_KEY, exportHints);

    doc.setObject(root);

    QFile file(d->hintFileInfo.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(doc.toJson());
    file.close();
}

void QPsdExporterTreeItemModel::setErrorMessage(const QString &errorMessage)
{
    if (d->errorMessage == errorMessage) return;
    d->errorMessage = errorMessage;
    emit errorOccurred(errorMessage);
}

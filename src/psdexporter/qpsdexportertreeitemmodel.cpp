// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdexportertreeitemmodel.h"

#include <QtPsdCore/QPsdParser>
#include <QtPsdGui/QPsdFolderLayerItem>
#include <QtPsdGui/QPsdFontMapper>
#include <QtPsdGui/QPsdGuiLayerTreeItemModel>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
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

    // Fallback members for non-PSD data sources
    QSize size;
    QFileInfo fallbackFileInfo;
    QString fallbackFileName;
};

#define HINTFILE_MAGIC_KEY "qtpsdparser.hint"_L1
#define HINTFILE_MAGIC_VERSION 1
#define HINTFILE_LAYER_HINTS_KEY "layers"_L1
#define HINTFILE_EXPORT_HINTS_KEY "exports"_L1
#define HINTFILE_FONT_MAPPING_KEY "fontMapping"_L1

QJsonObject QPsdExporterTreeItemModel::ExportHint::toJson() const
{
    QJsonObject object;
    if (!id.isEmpty())
        object.insert("id"_L1, id);
    object.insert("type"_L1, static_cast<int>(type));
    if (!componentName.isEmpty())
        object.insert("componentName"_L1, componentName);
    object.insert("native"_L1, static_cast<int>(baseElement));
    object.insert("visible"_L1, visible);
    if (interactive)
        object.insert("interactive"_L1, true);
    if (!properties.isEmpty()) {
        QStringList propList = properties.values();
        std::sort(propList.begin(), propList.end(), std::less<QString>());
        object.insert("properties"_L1, QJsonArray::fromStringList(propList));
    }
    if (!textSource.isEmpty())
        object.insert("textSource"_L1, textSource);
    return object;
}

QPsdExporterTreeItemModel::ExportHint QPsdExporterTreeItemModel::ExportHint::fromJson(const QJsonObject &obj)
{
    QStringList propList = obj.value("properties"_L1).toVariant().toStringList();
    int rawType = obj.value("type"_L1).toInt();

    // Migration: old None(5) → Embed(0)
    if (rawType == 5)
        rawType = 0;

    int rawNative = obj.value("native"_L1).toInt();
    bool interactive = obj.value("interactive"_L1).toBool(false);

    // Migration: old TouchArea(1) → interactive=true, baseElement=Container
    if (rawNative == 1) { // old TouchArea enum value
        if (rawType == Embed && !interactive)
            interactive = true;
        rawNative = Container;
    }
    auto nativeComponent = static_cast<NativeComponent>(rawNative);

    // Backward compat: read "componentName" with "name" fallback
    QString componentName = obj.value("componentName"_L1).toString();
    if (componentName.isEmpty())
        componentName = obj.value("name"_L1).toString();

    return ExportHint {
        obj.value("id"_L1).toString(),
        static_cast<Type>(rawType),
        componentName,
        nativeComponent,
        obj.value("visible"_L1).toBool(true),
        interactive,
        QSet<QString>(propList.begin(), propList.end()),
        obj.value("textSource"_L1).toString(),
    };
}

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

    // Get the PSD file path for font mapping context
    const QString psdPath = q->fileName();

    QFileInfo hintFileInfo(hintFileName);
    if (hintFileInfo.exists()) {
        QJsonDocument hintDoc = loadHint(hintFileInfo.absoluteFilePath());
        QJsonObject root = hintDoc.object();
        QJsonObject layerHintsJson = root.value(HINTFILE_LAYER_HINTS_KEY).toObject();
        for (const auto &idstr: layerHintsJson.keys()) {
            layerHints.insert(idstr, ExportHint::fromJson(layerHintsJson.value(idstr).toObject()));
        }

        QJsonObject exportHintsJson = root.value(HINTFILE_EXPORT_HINTS_KEY).toObject();
        for (const auto &exporterKey : exportHintsJson.keys()) {
            QVariantMap map = exportHintsJson.value(exporterKey).toObject().toVariantMap();
            exportHints.insert(exporterKey, map);
        }

        // Load font mappings from hint file
        if (root.contains(HINTFILE_FONT_MAPPING_KEY) && !psdPath.isEmpty()) {
            QJsonObject fontMappingJson = root.value(HINTFILE_FONT_MAPPING_KEY).toObject();
            QPsdFontMapper::instance()->loadFromHint(psdPath, fontMappingJson);
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
    if (model) {
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
    } else if (source) {
        // Non-PSD source model: connect modelReset only
        d->sourceConnections = QList<QMetaObject::Connection> {
            connect(source, &QAbstractItemModel::modelReset, this, [this]() {
                beginResetModel();
                if (!d->hintFileName.isEmpty())
                    d->loadHintFile();
                endResetModel();
            }),
        };
    }

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
    if (!item)
        return {};
    const QString idstr = QString::number(item->id());

    return d->layerHints.value(idstr);
}

void QPsdExporterTreeItemModel::setLayerHint(const QModelIndex &index, const ExportHint exportHint)
{
    const QPsdAbstractLayerItem *item = layerItem(index);
    if (!item)
        return;
    const QString idstr = QString::number(item->id());

    d->layerHints.insert(idstr, exportHint);

    emit dataChanged(index, index);
}

QSize QPsdExporterTreeItemModel::size() const
{
    const auto *model = dynamic_cast<const QPsdLayerTreeItemModel *>(sourceModel());
    if (model) {
        return model->size();
    }
    return d->size;
}

void QPsdExporterTreeItemModel::setSize(const QSize &size)
{
    d->size = size;
}

const QPsdAbstractLayerItem *QPsdExporterTreeItemModel::layerItem(const QModelIndex &index) const
{
    const auto *model = dynamic_cast<const QPsdGuiLayerTreeItemModel *>(sourceModel());
    if (model)
        return model->layerItem(mapToSource(index));
    // Fallback: retrieve from data role
    return data(index, LayerItemObjectRole).value<QPsdAbstractLayerItem *>();
}

qint32 QPsdExporterTreeItemModel::layerId(const QModelIndex &index) const
{
    const auto *model = dynamic_cast<const QPsdLayerTreeItemModel *>(sourceModel());
    if (model)
        return model->layerId(mapToSource(index));
    // Fallback: retrieve from data role
    return data(index, LayerIdRole).value<qint32>();
}

QString QPsdExporterTreeItemModel::layerName(const QModelIndex &index) const
{
    const auto *model = dynamic_cast<const QPsdLayerTreeItemModel *>(sourceModel());
    if (model)
        return model->layerName(mapToSource(index));
    // Fallback: retrieve from data role
    return data(index, NameRole).toString();
}

QRect QPsdExporterTreeItemModel::rect(const QModelIndex &index) const
{
    const auto *model = dynamic_cast<const QPsdLayerTreeItemModel *>(sourceModel());
    if (model)
        return model->rect(mapToSource(index));
    // Fallback: retrieve from data role
    return data(index, RectRole).toRect();
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
    }
    // Fallback: retrieve from data role
    return data(index, GroupIndexesRole).value<QList<QPersistentModelIndex>>();
}

QFileInfo QPsdExporterTreeItemModel::fileInfo() const
{
    auto *model = dynamic_cast<QPsdLayerTreeItemModel *>(sourceModel());
    if (model) {
        const auto fi = model->fileInfo();
        if (!fi.filePath().isEmpty())
            return fi;
    }
    return d->fallbackFileInfo;
}

void QPsdExporterTreeItemModel::setFileInfo(const QFileInfo &fileInfo)
{
    d->fallbackFileInfo = fileInfo;
    emit fileInfoChanged(fileInfo);
}

QString QPsdExporterTreeItemModel::fileName() const
{
    auto *model = dynamic_cast<QPsdLayerTreeItemModel *>(sourceModel());
    if (model) {
        const auto fn = model->fileName();
        if (!fn.isEmpty())
            return fn;
    }
    return d->fallbackFileName;
}

void QPsdExporterTreeItemModel::setFileName(const QString &fileName)
{
    d->fallbackFileName = fileName;
}

QString QPsdExporterTreeItemModel::errorMessage() const
{
    return d->errorMessage;
}

void QPsdExporterTreeItemModel::load(const QString &fileName)
{
    d->setDefaultHintFile(fileName);
    auto *model = dynamic_cast<QPsdLayerTreeItemModel *>(sourceModel());
    if (model)
        model->load(fileName);
}

static QJsonObject serializeLayerHints(QPsdExporterTreeItemModel *self)
{
    QJsonObject layerHintsJson;
    std::function<void(const QModelIndex &)> traverse = [&](const QModelIndex &index) {
        if (index.isValid()) {
            const auto layer = self->layerItem(index);
            if (!layer)
                return;
            const auto idstr = QString::number(layer->id());
            const auto hint = self->layerHint(index);
            if (!hint.isDefaultValue())
                layerHintsJson.insert(idstr, hint.toJson());
        }
        for (int i = 0; i < self->rowCount(index); i++) {
            traverse(self->index(i, 0, index));
        }
    };
    traverse({});
    return layerHintsJson;
}

void QPsdExporterTreeItemModel::save()
{
    QJsonObject layerHintsJson = serializeLayerHints(this);

    QJsonObject exportHintsJson;
    for (const auto &exporterKey : d->exportHints.keys()) {
        exportHintsJson.insert(exporterKey, QJsonObject::fromVariantMap(d->exportHints.value(exporterKey)));
    }

    // Get font mappings for this PSD file
    const QString psdPath = fileName();
    QJsonObject fontMappingJson = QPsdFontMapper::instance()->toHint(psdPath);

    QJsonObject root;
    root.insert(HINTFILE_MAGIC_KEY, HINTFILE_MAGIC_VERSION);
    root.insert(HINTFILE_LAYER_HINTS_KEY, layerHintsJson);
    root.insert(HINTFILE_EXPORT_HINTS_KEY, exportHintsJson);
    if (!fontMappingJson.isEmpty()) {
        root.insert(HINTFILE_FONT_MAPPING_KEY, fontMappingJson);
    }

    QJsonDocument doc;
    doc.setObject(root);

    QFile file(d->hintFileInfo.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(doc.toJson());
    file.close();
}

void QPsdExporterTreeItemModel::loadHints(const QString &hintFilePath)
{
    d->hintFileName = hintFilePath;
    d->hintFileInfo = QFileInfo(hintFilePath);
    d->loadHintFile();
}

void QPsdExporterTreeItemModel::saveHints(const QString &hintFilePath)
{
    QJsonObject layerHintsJson = serializeLayerHints(this);

    QJsonObject exportHintsJson;
    for (const auto &exporterKey : d->exportHints.keys()) {
        exportHintsJson.insert(exporterKey, QJsonObject::fromVariantMap(d->exportHints.value(exporterKey)));
    }

    QJsonObject root;
    root.insert(HINTFILE_MAGIC_KEY, HINTFILE_MAGIC_VERSION);
    root.insert(HINTFILE_LAYER_HINTS_KEY, layerHintsJson);
    root.insert(HINTFILE_EXPORT_HINTS_KEY, exportHintsJson);

    QJsonDocument doc;
    doc.setObject(root);

    QFile file(hintFilePath);
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

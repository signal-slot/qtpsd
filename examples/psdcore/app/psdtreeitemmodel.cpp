// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "psdtreeitemmodel.h"

#include <QtPsdCore/QPsdParser>
#include <QtPsdGui/QPsdFolderLayerItem>
#include <QtPsdGui/QPsdGuiLayerTreeItemModel>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtGui/QBrush>
#include <QtGui/QIcon>

class PsdTreeItemModel::Private
{
public:
    Private(const ::PsdTreeItemModel *model);
    ~Private();

    QFileInfo hintFileInfo(const QString &psdFileName) const;
    QJsonDocument loadHint(const QString &hintFileName);

    bool isValidIndex(const QModelIndex &index) const;

    const QPsdAbstractLayerItem *layerItem(const QModelIndex &index) const;
    const QPsdAbstractLayerItem::ExportHint layerHint(const QModelIndex &index) const;
    void setLayerHint(const QModelIndex &index, QPsdAbstractLayerItem::ExportHint exportHint);

    QVariantMap exportHint(const QString& exporterKey) const;
    void setExportHint(const QString& exporterKey, const QVariantMap &exportHint);
    QMap<QString, QVariantMap> exportHints() const;

    bool isVisible(const QModelIndex &index);
    void setVisible(const QModelIndex &index, bool visible);

    const ::PsdTreeItemModel *q;
    QString fileName;
    QFileInfo fileInfo;
    QString errorMessage;

    QPsdLayerTreeItemModel layerTreeModel;
    QPsdGuiLayerTreeItemModel guiLayerTreeModel;
    QPsdExporterTreeItemModel exporterTreeModel;
    QPsdFolderLayerItem *root = nullptr;

    QMap<QString, bool> visibleMap;
};

#define HINTFILE_MAGIC_KEY "qtpsdparser.hint"_L1
#define HINTFILE_MAGIC_VERSION 1
#define HINTFILE_LAYER_HINTS_KEY "layers"_L1
#define HINTFILE_EXPORT_HINTS_KEY "exports"_L1

PsdTreeItemModel::Private::Private(const ::PsdTreeItemModel *model) : q(model)
{
}

PsdTreeItemModel::Private::~Private()
{
}

QFileInfo PsdTreeItemModel::Private::hintFileInfo(const QString &psdFileName) const
{
    QFileInfo fileInfo(psdFileName);
    QString hintFileName = u"%1.%2"_s.arg(
        fileInfo.suffix().toLower() == "psd" ? fileInfo.completeBaseName() : fileInfo.fileName(), "psd_");

    return QFileInfo(fileInfo.dir(), hintFileName);
}

QJsonDocument PsdTreeItemModel::Private::loadHint(const QString &hintFileName)
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

const QPsdAbstractLayerItem *PsdTreeItemModel::Private::layerItem(const QModelIndex &index) const
{
    return exporterTreeModel.layerItem(index);
}

const QPsdAbstractLayerItem::ExportHint PsdTreeItemModel::Private::layerHint(const QModelIndex &index) const
{
    return exporterTreeModel.layerHint(index);
}

void PsdTreeItemModel::Private::setLayerHint(const QModelIndex &index, QPsdAbstractLayerItem::ExportHint exportHint)
{
    exporterTreeModel.setLayerHint(index, exportHint);
}

QVariantMap PsdTreeItemModel::Private::exportHint(const QString& exporterKey) const
{
    return exporterTreeModel.exportHint(exporterKey);
}

void PsdTreeItemModel::Private::setExportHint(const QString& exporterKey, const QVariantMap &exportHint)
{
    return exporterTreeModel.setExportHint(exporterKey, exportHint);
}

QMap<QString, QVariantMap> PsdTreeItemModel::Private::exportHints() const
{
    return exporterTreeModel.exportHints();
}

bool PsdTreeItemModel::Private::isValidIndex(const QModelIndex &index) const
{
    return index.isValid() && index.model() == q;
}

bool PsdTreeItemModel::Private::isVisible(const QModelIndex &index)
{
    if (!isValidIndex(index)) {
        return false;
    }

    const auto *item = layerItem(index);
    if (item == nullptr) {
        return false;
    }
    QString idstr = QString::number(item->id());

    if (!visibleMap.contains(idstr)) {
        visibleMap.insert(idstr, item->isVisible());
    }

    return visibleMap.value(idstr);
}

void PsdTreeItemModel::Private::setVisible(const QModelIndex &index, bool visible)
{
    if (!isValidIndex(index)) {
        return;
    }

    const auto *item = layerItem(index);
    if (item == nullptr) {
        return;
    }
    QString idstr = QString::number(item->id());

    visibleMap.insert(idstr, visible);
}

PsdTreeItemModel::PsdTreeItemModel(QObject *parent)
    : QIdentityProxyModel(parent), d(new Private(this))
{
}

PsdTreeItemModel::~PsdTreeItemModel()
{
}

QVariant PsdTreeItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    switch (role) {
    case Qt::DisplayRole:
        switch (section) {
        case Column::Name:
            return "Layers"_L1;
        case Column::Visible:
            return "Visible"_L1;
        case Column::Export:
            return "Export"_L1;
        }
        break;
    case Qt::TextAlignmentRole:
        return Qt::AlignCenter;
    default:
        break;
    }

    return QAbstractItemModel::headerData(section, orientation, role);
}

QHash<int, QByteArray> PsdTreeItemModel::roleNames() const
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
    roles.insert(Roles::VisibleRole, QByteArrayLiteral("Visible"));
    roles.insert(Roles::ExportIdRole, QByteArrayLiteral("ExportId"));

    return roles;
}

int PsdTreeItemModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 3;
}

QVariant PsdTreeItemModel::data(const QModelIndex &index, int role) const
{
    if (!d->isValidIndex(index))
        return QVariant();

    const QPsdAbstractLayerItem *item = d->layerItem(index);
    const QPsdAbstractLayerItem::ExportHint exportHint = d->layerHint(index);
    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case Column::Name:
            return item->name();
        case Column::Export:
            return exportHint.id;
        default:
            break;
        }
        break;
    case Qt::EditRole:
        switch (index.column()) {
        case Column::Export:
            return exportHint.id;
        default:
            break;
        }
        break;
    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case Column::Visible:
            return Qt::AlignHCenter;
        default:
            break;
        }
        break;
    case Qt::CheckStateRole:
        switch (index.column()) {
        case Column::Visible:
            return d->isVisible(index) ? Qt::Checked : Qt::Unchecked;
        default:
            break;
        }
        break;
    case Qt::BackgroundRole: {
        QColor color = item->color();
        int h, s, l, a;
        color.getHsl(&h, &s, &l, &a);
        color.setHsl(h, 128, 128, 128 * a / 255);
        return QBrush(color); }
    case Qt::DecorationRole:
        switch (index.column()) {
        case Column::Name:
            return QIcon(QPixmap::fromImage(item->image()));
        default:
            break;
        }
        break;
    case Roles::NameRole:
        return item->name();
    case Roles::LayerIdRole:
        return item->id();
    case Roles::VisibleRole:
        return d->isVisible(index);
    case Roles::ExportIdRole:
        return exportHint.id;
     case Roles::GroupIndexesRole: {
        QList<QVariant> indexes = sourceModel()->data(mapToSource(index), role).toList();
        QList<QVariant> result;
        for (const auto &i : indexes) {
            QModelIndex index = i.value<QPersistentModelIndex>();
            result.append(QVariant::fromValue(QPersistentModelIndex(mapFromSource(index))));
        }
        return QVariant(result); }
    case Roles::ClippingMaskIndexRole: {
        QPersistentModelIndex maskIndex = sourceModel()->data(mapToSource(index), role).value<QPersistentModelIndex>();
        return QVariant::fromValue(mapFromSource(maskIndex)); }
    case Roles::LayerItemObjectRole:
        return QVariant::fromValue(item);
    default:
        return sourceModel()->data(mapToSource(index), role);
    }

    return {};
}

bool PsdTreeItemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    switch (index.column()) {
    case Column::Visible:
        if (role == Qt::CheckStateRole) {
            d->setVisible(index, value.toBool());

            emit dataChanged(index, index);
            return true;
        }
        break;
    case Column::Export: {
        const QPsdAbstractLayerItem *item = d->layerItem(index);
        QPsdAbstractLayerItem::ExportHint exportHint = d->layerHint(index);
        exportHint.id = value.toString();
        d->setLayerHint(index, exportHint);

        emit dataChanged(index, index);
        return true; }
    default:
        break;
    }

    return false;
}

Qt::ItemFlags PsdTreeItemModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags(index);
    switch (index.column()) {
    case Column::Visible:
        return flags | Qt::ItemIsUserCheckable;
    case Column::Export:
        return flags | Qt::ItemIsEditable;
    default:
        break;
    }

    return flags;
}

QFileInfo PsdTreeItemModel::fileInfo() const
{
    return d->fileInfo;
}

QString PsdTreeItemModel::fileName() const
{
    return d->fileName;
}

QString PsdTreeItemModel::errorMessage() const
{
    return d->errorMessage;
}

QSize PsdTreeItemModel::size() const
{
    return d->guiLayerTreeModel.size();
}

const QPsdFolderLayerItem *PsdTreeItemModel::layerTree() const
{
    return d->root;
}

QVariantMap PsdTreeItemModel::exportHint(const QString& exporterKey) const
{
    return d->exportHint(exporterKey);
}

void PsdTreeItemModel::updateExportHint(const QString &exporterKey, const QVariantMap &hint)
{
    d->setExportHint(exporterKey, hint);
}

void PsdTreeItemModel::load(const QString &fileName)
{
    d->fileInfo = QFileInfo(fileName);
    d->fileName = fileName;
    if (!d->fileInfo.exists()) {
        setErrorMessage(tr("File not found"));
        return;
    }
    emit fileInfoChanged(d->fileInfo);

    QPsdParser parser;
    parser.load(fileName);

    QMap<QString, QPsdAbstractLayerItem::ExportHint> layerHints;
    QMap<QString, QVariantMap> exportHints;

    QFileInfo hintFileInfo = d->hintFileInfo(fileName);
    if (hintFileInfo.exists()) {
        QJsonDocument hintDoc = d->loadHint(hintFileInfo.absoluteFilePath());
        QJsonObject root = hintDoc.object();
        QJsonObject layerHintsJson = root.value(HINTFILE_LAYER_HINTS_KEY).toObject();
        for (const auto &idstr: layerHintsJson.keys()) {
            QVariantMap settings = layerHintsJson.value(idstr).toObject().toVariantMap();
            QStringList properties = settings.value("properties").toStringList();
            QPsdAbstractLayerItem::ExportHint exportHint {
                settings.value("id").toString(),
                static_cast<QPsdAbstractLayerItem::ExportHint::Type>(settings.value("type").toInt()),
                settings.value("name").toString(),
                static_cast<QPsdAbstractLayerItem::ExportHint::NativeComponent>(settings.value("native").toInt()),
                settings.value("visible").toBool(),
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

    d->guiLayerTreeModel.setSourceModel(&d->layerTreeModel);
    d->exporterTreeModel.setSourceModel(&d->guiLayerTreeModel);
    d->exporterTreeModel.fromParser(parser, layerHints, exportHints);
    setSourceModel(&d->guiLayerTreeModel);

    const auto header = parser.fileHeader();
    if (!header.errorString().isEmpty()) {
        setErrorMessage(header.errorString());
        return;
    }

    d->root = QPsdLayerTree::fromParser(parser);

    std::function<void(const QModelIndex &parent)> traverseTree;
    traverseTree = [&](const QModelIndex &parent) {
        for (int row = 0; row < rowCount(parent); row++) {
            const QModelIndex childIndex = index(row, 0, parent);
            const QPsdAbstractLayerItem *item = d->layerItem(childIndex);
            QPsdAbstractLayerItem::ExportHint exportHint = d->layerHint(childIndex);
            exportHint.visible = item->isVisible();
            d->setLayerHint(childIndex, exportHint);
            item->setExportHint(exportHint);

            if (hasChildren(childIndex)) {
                traverseTree(childIndex);
            }
        }
    };
    traverseTree(QModelIndex());
}

void PsdTreeItemModel::save()
{
    QFileInfo hintFileInfo = d->hintFileInfo(fileName());

    QJsonDocument doc;
    QJsonObject layerHintsJson;
    std::function<void(const QModelIndex &)> traverse = [&](const QModelIndex &index) {
        if (index.isValid()) {
            const auto layer = data(index, PsdTreeItemModel::Roles::LayerItemObjectRole).value<const QPsdAbstractLayerItem *>();
            const auto lyid = layer->id();
            const auto idstr = QString::number(lyid);

            const auto exportHint = layer->exportHint();
            if (!exportHint.isDefaultValue()) {
                QStringList propList = exportHint.properties.values();
                std::sort(propList.begin(), propList.end(), std::less<QString>());
                QJsonObject object;
                if (!exportHint.id.isEmpty()) {
                    object.insert("id", exportHint.id);
                }
                object.insert("type", static_cast<int>(exportHint.type));
                if (!exportHint.componentName.isEmpty()) {
                    object.insert("name", exportHint.componentName);
                }
                object.insert("native", static_cast<int>(exportHint.baseElement));
                object.insert("visible", exportHint.visible);
                if (!propList.isEmpty())
                    object.insert("properties", QJsonArray::fromStringList(propList));
                layerHintsJson.insert(idstr, object);
            }
        }
        for (int i = 0; i < rowCount(index); i++) {
            traverse(this->index(i, 0, index));
        }
    };
    traverse({});

    QJsonObject exportHintsJson;
    const auto exportHints = d->exportHints();
    for (const auto &exporterKey : exportHints.keys()) {
        exportHintsJson.insert(exporterKey, QJsonObject::fromVariantMap(exportHints.value(exporterKey)));
    }

    QJsonObject root;
    root.insert(HINTFILE_MAGIC_KEY, HINTFILE_MAGIC_VERSION);
    root.insert(HINTFILE_LAYER_HINTS_KEY, layerHintsJson);
    root.insert(HINTFILE_EXPORT_HINTS_KEY, exportHintsJson);

    doc.setObject(root);

    QFile file(hintFileInfo.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(doc.toJson());
    file.close();
}

void PsdTreeItemModel::setErrorMessage(const QString &errorMessage)
{
    if (d->errorMessage == errorMessage) return;
    d->errorMessage = errorMessage;
    emit errorOccurred(errorMessage);
}

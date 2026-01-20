#include "psdtreeitemmodel.h"

#include <QtPsdGui/QPsdAbstractLayerItem>
#include <QtPsdGui/QPsdFolderLayerItem>
#include <QtPsdWidget/QPsdWidgetTreeItemModel>

#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtGui/QBrush>
#include <QtGui/QIcon>
#include <QtGui/QPixmap>

class PsdTreeItemModel::Private
{
public:
    Private(const ::PsdTreeItemModel *model);
    ~Private();

    bool isValidIndex(const QModelIndex &index) const;
    bool isSkipped(const QModelIndex &index) const;

    const ::PsdTreeItemModel *q;
};

PsdTreeItemModel::Private::Private(const ::PsdTreeItemModel *model) : q(model)
{
}

PsdTreeItemModel::Private::~Private()
{
}

bool PsdTreeItemModel::Private::isValidIndex(const QModelIndex &index) const
{
    return index.isValid() && index.model() == q;
}

bool PsdTreeItemModel::Private::isSkipped(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;

    const auto hint = q->layerHint(index);
    if (hint.type == QPsdExporterTreeItemModel::ExportHint::Skip)
        return true;

    return isSkipped(index.parent());
}

PsdTreeItemModel::PsdTreeItemModel(QObject *parent)
    : QPsdExporterTreeItemModel(parent), d(new Private(this))
{
}

PsdTreeItemModel::~PsdTreeItemModel()
{
}

QPsdWidgetTreeItemModel *PsdTreeItemModel::widgetModel() const
{
    return dynamic_cast<QPsdWidgetTreeItemModel *>(sourceModel());
}

QVariant PsdTreeItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    switch (role) {
    case Qt::DisplayRole:
        switch (section) {
        case Column::Name:
            return "Layers"_L1;
        case Column::Visible:
            return u"👁"_s;
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
    auto roles = QPsdExporterTreeItemModel::roleNames();
    roles.insert(Roles::ExportIdRole, QByteArrayLiteral("ExportId"));

    return roles;
}

int PsdTreeItemModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 2;
}

QVariant PsdTreeItemModel::data(const QModelIndex &index, int role) const
{
    if (!d->isValidIndex(index))
        return QVariant();

    const QPsdAbstractLayerItem *item = layerItem(index);
    const QPsdExporterTreeItemModel::ExportHint exportHint = layerHint(index);
    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case Column::Name:
            // Show custom name if set, otherwise original name
            return exportHint.id.isEmpty() ? item->name() : exportHint.id;
        default:
            break;
        }
        break;
    case Qt::EditRole:
        switch (index.column()) {
        case Column::Name:
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
            return isVisible(index) ? Qt::Checked : Qt::Unchecked;
        default:
            break;
        }
        break;
    case Qt::ForegroundRole:
        if (d->isSkipped(index))
            return QBrush(Qt::gray);
        // Show red if name is edited
        if (index.column() == Column::Name && !exportHint.id.isEmpty())
            return QBrush(Qt::red);
        break;
    case Roles::PlaceholderTextRole:
        switch (index.column()) {
        case Column::Name:
            return item->name();
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
    case Roles::ExportIdRole:
        return exportHint.id;
    default:
        return QPsdExporterTreeItemModel::data(index, role);
    }

    return {};
}

bool PsdTreeItemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    switch (index.column()) {
    case Column::Name:
        if (role == Qt::EditRole) {
            QPsdExporterTreeItemModel::ExportHint exportHint = layerHint(index);
            const QString newId = value.toString();
            if (exportHint.id == newId)
                return false;
            exportHint.id = newId;
            setLayerHint(index, exportHint);
            emit dataChanged(index, index);
            return true;
        }
        break;
    case Column::Visible:
        if (role == Qt::CheckStateRole) {
            setVisible(index, value.toBool());

            emit dataChanged(index, index);
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

Qt::ItemFlags PsdTreeItemModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags(index);
    switch (index.column()) {
    case Column::Name:
        return flags | Qt::ItemIsEditable;
    case Column::Visible:
        return flags | Qt::ItemIsUserCheckable;
    default:
        break;
    }

    return flags;
}

QString PsdTreeItemModel::exportId(const QModelIndex &index) const
{
    QPsdExporterTreeItemModel::ExportHint exportHint = layerHint(index);

    return exportHint.id;
}

bool PsdTreeItemModel::isVisible(const QModelIndex &index) const
{
    const auto *model = dynamic_cast<const QPsdWidgetTreeItemModel *>(sourceModel());
    if (model) {
        return model->isVisible(mapToSource(index));
    } else {
        return {};
    }
}

void PsdTreeItemModel::setVisible(const QModelIndex &index, bool visible)
{
    auto *model = dynamic_cast<QPsdWidgetTreeItemModel *>(sourceModel());
    if (model) {
        model->setVisible(mapToSource(index), visible);
    }
}

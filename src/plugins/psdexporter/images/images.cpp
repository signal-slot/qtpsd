// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/qpsdexporterplugin.h>
#include <QtGui/QImage>
#include <QtCore/QDir>

QT_BEGIN_NAMESPACE

class QPsdExporterImagePlugin : public QPsdExporterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdExporterFactoryInterface" FILE "images.json")
public:
    int priority() const override { return 100; }
    QIcon icon() const override {
        auto mimeType = mimeDatabase.mimeTypeForName("image/png");
        return QIcon::fromTheme(mimeType.iconName());
    }
    QString name() const override {
        return tr("&Images");
    }
    ExportType exportType() const override { return QPsdExporterPlugin::Directory; }

    bool exportTo(const PsdTreeItemModel *model, const QString &to, const QVariantMap &hint) const override;
};

bool QPsdExporterImagePlugin::exportTo(const PsdTreeItemModel *model, const QString &to, const QVariantMap &hint) const
{
    const auto imageScaling = hint.value("imageScaling", false).toBool();
    std::function<void(const QModelIndex &, QDir *)> traverseTree;
    traverseTree = [&](const QModelIndex &index, QDir *directory) {
        bool isFolder = false;
        QString folderName;
        if (index.isValid()) {
            const auto *item = model->layerItem(index);
            switch (item->type()) {
            case QPsdAbstractLayerItem::Folder: {
                const auto *folder = dynamic_cast<const QPsdFolderLayerItem *>(item);
                folderName = folder->name();
                directory->mkdir(folderName);
                directory->cd(folderName);
                isFolder = true;
                break; }
            case QPsdAbstractLayerItem::Image: {
                const auto *imageItem = dynamic_cast<const QPsdImageLayerItem *>(item);
                QImage image = imageItem->linkedImage();
                QString name = imageItem->linkedFile().name;
                if (image.isNull()) {
                    image = item->image();
                    name = item->name() + ".png"_L1;
                }
                if (imageScaling)
                    image = image.scaled(item->rect().size(), Qt::KeepAspectRatio);
                image.save(directory->filePath(name));
                break; }
            default:
                break;
            }
        }

        for (int i = 0; i < model->rowCount(index); i++) {
            traverseTree(model->index(i, 0, index), directory);
        }

        if (isFolder) {
            bool isEmpty = directory->isEmpty();
            directory->cdUp();
            if (isEmpty)
                directory->rmdir(folderName);
        }
    };

    QDir dir(to);
    traverseTree(QModelIndex{}, &dir);

    return true;
}

QT_END_NAMESPACE

#include "images.moc"

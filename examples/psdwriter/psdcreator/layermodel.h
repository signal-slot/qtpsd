// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef LAYERMODEL_H
#define LAYERMODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtGui/QImage>
#include <QtPsdCore/qpsdblend.h>
#include <QtPsdGui/qpsdtextlayeritem.h>

struct Layer {
    enum Type { ImageLayer, TextLayer, FolderLayer };
    Type type = ImageLayer;
    QString name;
    QImage image;           // pixel data at original size (not full canvas)
    QRect imageRect;        // position/size within the canvas
    bool hasAlpha = true;   // false for PSD "Background" layers (no transparency mask)
    quint8 opacity = 255;
    quint8 originalFlags = 0x00;
    QPsdBlend::Mode blendMode = QPsdBlend::Normal;
    bool visible = true;
    // Text-specific
    QList<QPsdTextLayerItem::Run> textRuns;
    QPoint textPosition;    // top-left of text content within the canvas
    QPointF textOrigin;     // baseline anchor point (absolute canvas coords)
    // Original PSD data for round-trip (kept when loaded from PSD)
    QVariant originalTySh;
    // Tree
    QList<Layer> children;  // only for FolderLayer
    // Internal: parent pointer (non-owning, for tree navigation)
    Layer *parentLayer = nullptr;
};

class LayerModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum Roles {
        OpacityRole = Qt::UserRole + 1,
        BlendModeRole,
        ThumbnailRole,
        LayerTypeRole,
    };

    explicit LayerModel(QObject *parent = nullptr);

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Layer management
    QModelIndex addLayer(const QString &name, const QModelIndex &parentIndex = QModelIndex());
    QModelIndex addTextLayer(const QString &name, const QModelIndex &parentIndex = QModelIndex());
    QModelIndex addFolder(const QString &name, const QModelIndex &parentIndex = QModelIndex());
    void removeLayer(const QModelIndex &index);

    // Accessors
    Layer *layerForIndex(const QModelIndex &index) const;
    void setLayerImage(const QModelIndex &index, const QImage &image);
    void setLayerOpacity(const QModelIndex &index, quint8 opacity);
    void setLayerBlendMode(const QModelIndex &index, QPsdBlend::Mode mode);
    void setLayerVisible(const QModelIndex &index, bool visible);

    QSize canvasSize() const;
    void setCanvasSize(const QSize &size);
    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &color);

    QImage flattenedImage() const;

    void clear();
    QModelIndex insertLayerAt(const QModelIndex &parentIndex, int row, const Layer &layer);

    // Flat count of all leaf layers (for compatibility)
    int totalLayerCount() const;

signals:
    void layerChanged(const QModelIndex &index);

private:
    QModelIndex addLayerInternal(const Layer &layer, const QModelIndex &parentIndex);
    QList<Layer> &childList(const QModelIndex &parentIndex);
    const QList<Layer> &childList(const QModelIndex &parentIndex) const;
    void fixupParentPointers(QList<Layer> &layers, Layer *parent);
    void compositeRecursive(QImage &result, const QList<Layer> &layers) const;
    int countLeaves(const QList<Layer> &layers) const;

    QList<Layer> m_rootLayers;
    QSize m_canvasSize = QSize(800, 600);
    QColor m_backgroundColor = Qt::white;
};

#endif // LAYERMODEL_H

// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "layermodel.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtPsdGui/qpsdguiglobal.h>

using namespace Qt::StringLiterals;

LayerModel::LayerModel(QObject *parent)
    : QAbstractItemModel(parent)
{}

QModelIndex LayerModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0)
        return {};
    const auto &children = childList(parent);
    if (row < 0 || row >= children.size())
        return {};
    return createIndex(row, 0, const_cast<Layer *>(&children.at(row)));
}

QModelIndex LayerModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return {};
    auto *layer = static_cast<Layer *>(child.internalPointer());
    if (!layer || !layer->parentLayer)
        return {};
    // Find the parent's row in its own parent's child list
    Layer *parentLayer = layer->parentLayer;
    Layer *grandparent = parentLayer->parentLayer;
    const auto &siblings = grandparent ? grandparent->children : m_rootLayers;
    for (int i = 0; i < siblings.size(); ++i) {
        if (&siblings.at(i) == parentLayer)
            return createIndex(i, 0, parentLayer);
    }
    return {};
}

int LayerModel::rowCount(const QModelIndex &parent) const
{
    return childList(parent).size();
}

int LayerModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant LayerModel::data(const QModelIndex &index, int role) const
{
    auto *layer = layerForIndex(index);
    if (!layer)
        return {};

    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return layer->name;
    case Qt::CheckStateRole:
        return layer->visible ? Qt::Checked : Qt::Unchecked;
    case Qt::DecorationRole: {
        const int thumbSize = 32;
        if (layer->type == Layer::FolderLayer) {
            // Simple folder icon placeholder
            QImage icon(thumbSize, thumbSize, QImage::Format_ARGB32);
            icon.fill(Qt::transparent);
            QPainter p(&icon);
            p.setPen(Qt::darkYellow);
            p.setBrush(QColor(255, 200, 50, 100));
            p.drawRect(2, 6, thumbSize - 5, thumbSize - 9);
            p.drawRect(2, 2, 14, 8);
            p.end();
            return icon;
        }
        if (layer->type == Layer::TextLayer) {
            QImage icon(thumbSize, thumbSize, QImage::Format_ARGB32);
            icon.fill(Qt::transparent);
            QPainter p(&icon);
            p.setPen(Qt::black);
            QFont f;
            f.setPixelSize(24);
            f.setBold(true);
            p.setFont(f);
            p.drawText(QRect(0, 0, thumbSize, thumbSize), Qt::AlignCenter, u"T"_s);
            p.end();
            return icon;
        }
        return layer->image.scaled(thumbSize, thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    case OpacityRole:
        return layer->opacity;
    case BlendModeRole:
        return QVariant::fromValue(static_cast<int>(layer->blendMode));
    case LayerTypeRole:
        return static_cast<int>(layer->type);
    default:
        return {};
    }
}

bool LayerModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    auto *layer = layerForIndex(index);
    if (!layer)
        return false;

    switch (role) {
    case Qt::EditRole:
        layer->name = value.toString();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    case Qt::CheckStateRole:
        layer->visible = (value.toInt() == Qt::Checked);
        emit dataChanged(index, index, {Qt::CheckStateRole});
        emit layerChanged(index);
        return true;
    case OpacityRole:
        layer->opacity = static_cast<quint8>(value.toUInt());
        emit dataChanged(index, index, {OpacityRole});
        emit layerChanged(index);
        return true;
    case BlendModeRole:
        layer->blendMode = static_cast<QPsdBlend::Mode>(value.toInt());
        emit dataChanged(index, index, {BlendModeRole});
        emit layerChanged(index);
        return true;
    default:
        return false;
    }
}

Qt::ItemFlags LayerModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable
           | Qt::ItemIsUserCheckable;
}

QModelIndex LayerModel::addLayer(const QString &name, const QModelIndex &parentIndex)
{
    Layer layer;
    layer.type = Layer::ImageLayer;
    layer.name = name;
    layer.image = QImage(m_canvasSize, QImage::Format_ARGB32);
    layer.image.fill(Qt::transparent);
    return addLayerInternal(layer, parentIndex);
}

QModelIndex LayerModel::addTextLayer(const QString &name, const QModelIndex &parentIndex)
{
    Layer layer;
    layer.type = Layer::TextLayer;
    layer.name = name;
    layer.image = QImage(m_canvasSize, QImage::Format_ARGB32);
    layer.image.fill(Qt::transparent);
    return addLayerInternal(layer, parentIndex);
}

QModelIndex LayerModel::addFolder(const QString &name, const QModelIndex &parentIndex)
{
    Layer layer;
    layer.type = Layer::FolderLayer;
    layer.name = name;
    // Folders don't need a full-size image
    return addLayerInternal(layer, parentIndex);
}

void LayerModel::removeLayer(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    const QModelIndex par = index.parent();
    auto &children = childList(par);
    const int row = index.row();
    if (row < 0 || row >= children.size())
        return;
    beginRemoveRows(par, row, row);
    children.removeAt(row);
    fixupParentPointers(children, children.isEmpty() ? nullptr : children.first().parentLayer);
    endRemoveRows();
}

Layer *LayerModel::layerForIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return nullptr;
    return static_cast<Layer *>(index.internalPointer());
}

void LayerModel::setLayerImage(const QModelIndex &index, const QImage &image)
{
    auto *layer = layerForIndex(index);
    if (!layer)
        return;
    layer->image = image;
    emit dataChanged(index, index, {Qt::DecorationRole});
    emit layerChanged(index);
}

void LayerModel::setLayerOpacity(const QModelIndex &index, quint8 opacity)
{
    setData(index, opacity, OpacityRole);
}

void LayerModel::setLayerBlendMode(const QModelIndex &index, QPsdBlend::Mode mode)
{
    setData(index, static_cast<int>(mode), BlendModeRole);
}

void LayerModel::setLayerVisible(const QModelIndex &index, bool visible)
{
    setData(index, visible ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
}

QSize LayerModel::canvasSize() const
{
    return m_canvasSize;
}

void LayerModel::setCanvasSize(const QSize &size)
{
    m_canvasSize = size;
}

QColor LayerModel::backgroundColor() const
{
    return m_backgroundColor;
}

void LayerModel::setBackgroundColor(const QColor &color)
{
    m_backgroundColor = color;
}

QImage LayerModel::flattenedImage() const
{
    QImage result(m_canvasSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(m_backgroundColor);
    // Layers are stored top-first; iterate bottom-to-top for compositing
    compositeRecursive(result, m_rootLayers);
    return result;
}

void LayerModel::clear()
{
    beginResetModel();
    m_rootLayers.clear();
    endResetModel();
}

QModelIndex LayerModel::insertLayerAt(const QModelIndex &parentIndex, int row, const Layer &layer)
{
    auto &children = childList(parentIndex);
    if (row < 0)
        row = 0;
    if (row > children.size())
        row = children.size();
    beginInsertRows(parentIndex, row, row);
    children.insert(row, layer);
    Layer *parentPtr = parentIndex.isValid() ? layerForIndex(parentIndex) : nullptr;
    fixupParentPointers(children, parentPtr);
    endInsertRows();
    return index(row, 0, parentIndex);
}

int LayerModel::totalLayerCount() const
{
    return countLeaves(m_rootLayers);
}

QModelIndex LayerModel::addLayerInternal(const Layer &layer, const QModelIndex &parentIndex)
{
    // Insert at top (row 0)
    return insertLayerAt(parentIndex, 0, layer);
}

QList<Layer> &LayerModel::childList(const QModelIndex &parentIndex)
{
    if (!parentIndex.isValid())
        return m_rootLayers;
    auto *layer = layerForIndex(parentIndex);
    return layer->children;
}

const QList<Layer> &LayerModel::childList(const QModelIndex &parentIndex) const
{
    if (!parentIndex.isValid())
        return m_rootLayers;
    auto *layer = layerForIndex(parentIndex);
    return layer->children;
}

void LayerModel::fixupParentPointers(QList<Layer> &layers, Layer *parent)
{
    for (int i = 0; i < layers.size(); ++i) {
        layers[i].parentLayer = parent;
        fixupParentPointers(layers[i].children, &layers[i]);
    }
}

void LayerModel::compositeRecursive(QImage &result, const QList<Layer> &layers) const
{
    // Layers are stored top-first; iterate bottom-to-top
    for (int i = layers.size() - 1; i >= 0; --i) {
        const auto &layer = layers.at(i);
        if (!layer.visible)
            continue;

        if (layer.type == Layer::FolderLayer) {
            // Composite children recursively
            compositeRecursive(result, layer.children);
        } else if (layer.type == Layer::TextLayer) {
            // Render text runs into a temporary image for compositing
            QImage textImage(result.size(), QImage::Format_ARGB32);
            textImage.fill(Qt::transparent);
            QPainter tp(&textImage);
            tp.setRenderHint(QPainter::Antialiasing);
            QPoint pos = layer.textPosition;
            for (const auto &run : layer.textRuns) {
                QFont font = run.font;
                const qreal dpiScale = QGuiApplication::primaryScreen()->logicalDotsPerInchY() / 72.0;
                font.setPointSizeF(font.pointSizeF() / dpiScale);
                font.setStyleStrategy(QFont::PreferTypoLineMetrics);
                tp.setFont(font);
                tp.setPen(run.color);
                const QFontMetrics fm(font);
                tp.drawText(pos.x(), pos.y() + fm.ascent(), run.text);
                pos.setY(pos.y() + fm.height());
            }
            tp.end();

            const qreal opacity = layer.opacity / 255.0;
            if (QtPsdGui::isCustomBlendMode(layer.blendMode)) {
                QtPsdGui::customBlend(result, textImage, layer.blendMode, opacity);
            } else {
                QPainter p(&result);
                p.setCompositionMode(QtPsdGui::compositionMode(layer.blendMode));
                p.setOpacity(opacity);
                p.drawImage(0, 0, textImage);
                p.end();
            }
        } else {
            if (layer.image.isNull())
                continue;
            const qreal opacity = layer.opacity / 255.0;
            if (QtPsdGui::isCustomBlendMode(layer.blendMode)) {
                QtPsdGui::customBlend(result, layer.image, layer.blendMode, opacity);
            } else {
                QPainter p(&result);
                p.setCompositionMode(QtPsdGui::compositionMode(layer.blendMode));
                p.setOpacity(opacity);
                p.drawImage(0, 0, layer.image);
                p.end();
            }
        }
    }
}

int LayerModel::countLeaves(const QList<Layer> &layers) const
{
    int count = 0;
    for (const auto &layer : layers) {
        if (layer.type == Layer::FolderLayer)
            count += countLeaves(layer.children);
        else
            ++count;
    }
    return count;
}

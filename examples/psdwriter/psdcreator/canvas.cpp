// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "canvas.h"
#include "layermodel.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QTextDocument>
#include <QtGui/QTextCursor>
#include <QtGui/QTextCharFormat>
#include <QtWidgets/QGraphicsPathItem>
#include <QtWidgets/QGraphicsPixmapItem>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsTextItem>

Canvas::Canvas(LayerModel *model, QWidget *parent)
    : QGraphicsView(parent)
    , m_model(model)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void Canvas::setActiveLayer(const QModelIndex &index)
{
    m_activeLayer = index;
}

QModelIndex Canvas::activeLayer() const
{
    return m_activeLayer;
}

void Canvas::setTool(Tool tool)
{
    m_tool = tool;
    switch (m_tool) {
    case BrushTool:
        setCursor(Qt::CrossCursor);
        break;
    case EraserTool:
        setCursor(Qt::CrossCursor);
        break;
    case FillTool:
        setCursor(Qt::PointingHandCursor);
        break;
    }
}

Canvas::Tool Canvas::tool() const
{
    return m_tool;
}

void Canvas::setColor(const QColor &color)
{
    m_color = color;
}

QColor Canvas::color() const
{
    return m_color;
}

void Canvas::setBrushSize(int size)
{
    m_brushSize = qBound(1, size, 100);
}

int Canvas::brushSize() const
{
    return m_brushSize;
}

void Canvas::updateLayers()
{
    refreshPixmaps();
}

void Canvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_activeLayer.isValid()) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    auto *layer = m_model->layerForIndex(m_activeLayer);
    if (!layer || layer->type != Layer::ImageLayer) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    const QPointF scenePos = mapToScene(event->pos());

    if (m_tool == FillTool) {
        fillActiveLayer();
        emit modified();
        return;
    }

    m_drawing = true;
    m_lastPoint = scenePos;
    drawStroke(scenePos, scenePos);
    refreshPixmaps();
    emit modified();
}

void Canvas::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_drawing) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    const QPointF scenePos = mapToScene(event->pos());
    drawStroke(m_lastPoint, scenePos);
    m_lastPoint = scenePos;
    refreshPixmaps();
    emit modified();
}

void Canvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        m_drawing = false;
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void Canvas::drawBackground(QPainter *painter, const QRectF &rect)
{
    // Checkerboard transparency pattern
    const int gridSize = 8;
    const QColor light(220, 220, 220);
    const QColor dark(180, 180, 180);

    const int left = static_cast<int>(rect.left()) - (static_cast<int>(rect.left()) % gridSize);
    const int top = static_cast<int>(rect.top()) - (static_cast<int>(rect.top()) % gridSize);

    for (int y = top; y < rect.bottom(); y += gridSize) {
        for (int x = left; x < rect.right(); x += gridSize) {
            const bool even = ((x / gridSize) + (y / gridSize)) % 2 == 0;
            painter->fillRect(x, y, gridSize, gridSize, even ? light : dark);
        }
    }

    // Draw canvas background
    const QSize canvasSize = m_model->canvasSize();
    painter->fillRect(0, 0, canvasSize.width(), canvasSize.height(), m_model->backgroundColor());
}

void Canvas::drawStroke(const QPointF &from, const QPointF &to)
{
    if (!m_activeLayer.isValid())
        return;

    auto *layer = m_model->layerForIndex(m_activeLayer);
    if (!layer || layer->type != Layer::ImageLayer)
        return;

    QImage image = layer->image;
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen pen;
    pen.setWidth(m_brushSize);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);

    if (m_tool == EraserTool) {
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        pen.setColor(Qt::transparent);
    } else {
        pen.setColor(m_color);
    }

    painter.setPen(pen);
    painter.drawLine(from, to);
    painter.end();

    m_model->setLayerImage(m_activeLayer, image);
}

void Canvas::fillActiveLayer()
{
    if (!m_activeLayer.isValid())
        return;

    auto *layer = m_model->layerForIndex(m_activeLayer);
    if (!layer || layer->type != Layer::ImageLayer)
        return;

    QImage image = layer->image;
    image.fill(m_color);
    m_model->setLayerImage(m_activeLayer, image);
    refreshPixmaps();
}

void Canvas::refreshPixmaps()
{
    // Remove old items
    for (auto *item : m_layerItems)
        m_scene->removeItem(item);
    qDeleteAll(m_layerItems);
    m_layerItems.clear();

    const QSize canvasSize = m_model->canvasSize();
    m_scene->setSceneRect(0, 0, canvasSize.width(), canvasSize.height());

    int zOrder = 0;
    addLayerItems(QModelIndex(), zOrder);
}

QGraphicsTextItem *Canvas::createTextItem(const Layer &layer)
{
    auto *textItem = new QGraphicsTextItem;
    auto *doc = textItem->document();
    doc->setDocumentMargin(0);
    QTextCursor cursor(doc);

    for (int i = 0; i < layer.textRuns.size(); ++i) {
        const auto &run = layer.textRuns.at(i);
        if (i > 0 && !run.text.startsWith(u'\n'))
            cursor.insertText(u"\n"_s);
        QTextCharFormat fmt;
        QFont font = run.font;
        // PSD font sizes assume 72 PPI; Qt converts points using screen logical DPI
        const qreal dpiScale = QGuiApplication::primaryScreen()->logicalDotsPerInchY() / 72.0;
        font.setPointSizeF(font.pointSizeF() / dpiScale);
        font.setStyleStrategy(QFont::PreferTypoLineMetrics);
        fmt.setFont(font);
        fmt.setForeground(run.color);
        cursor.insertText(run.text, fmt);
    }

    // Match QPsdTextItem baseline positioning:
    // QPsdTextItem draws baseline at (textOrigin.y - rect.top) from item top
    // QGraphicsTextItem places baseline at ascent from item top
    // Offset = difference between the two
    if (!layer.textRuns.isEmpty() && !layer.textOrigin.isNull()) {
        QFont font = layer.textRuns.first().font;
        const qreal dpiScale = QGuiApplication::primaryScreen()->logicalDotsPerInchY() / 72.0;
        font.setPointSizeF(font.pointSizeF() / dpiScale);
        font.setStyleStrategy(QFont::PreferTypoLineMetrics);
        const QFontMetricsF fm(font);
        const qreal psdBaseline = layer.textOrigin.y() - layer.textPosition.y();
        const qreal docBaseline = fm.ascent();
        textItem->setPos(layer.textPosition.x(),
                         layer.textPosition.y() + (psdBaseline - docBaseline));
    } else {
        textItem->setPos(layer.textPosition);
    }
    return textItem;
}

void Canvas::addLayerItems(const QModelIndex &parent, int &zOrder)
{
    // Layers stored top-first; render bottom-to-top (increasing z)
    const int count = m_model->rowCount(parent);
    for (int i = count - 1; i >= 0; --i) {
        const auto idx = m_model->index(i, 0, parent);
        const auto *layer = m_model->layerForIndex(idx);
        if (!layer)
            continue;

        if (!layer->visible)
            continue;

        if (layer->type == Layer::FolderLayer) {
            addLayerItems(idx, zOrder);
        } else if (layer->type == Layer::TextLayer) {
            auto *item = createTextItem(*layer);
            item->setZValue(++zOrder);
            item->setOpacity(layer->opacity / 255.0);
            m_scene->addItem(item);
            m_layerItems.append(item);
        } else if (layer->type == Layer::ShapeLayer) {
            auto *pathItem = new QGraphicsPathItem(layer->shapePath);
            pathItem->setBrush(layer->shapeFillColor.isValid()
                               ? QBrush(layer->shapeFillColor) : Qt::NoBrush);
            if (layer->shapeStrokeWidth > 0 && layer->shapeStrokeColor.isValid())
                pathItem->setPen(QPen(layer->shapeStrokeColor, layer->shapeStrokeWidth));
            else
                pathItem->setPen(Qt::NoPen);
            pathItem->setZValue(++zOrder);
            pathItem->setOpacity(layer->opacity / 255.0);
            m_scene->addItem(pathItem);
            m_layerItems.append(pathItem);
        } else {
            // Image layer
            auto *item = m_scene->addPixmap(QPixmap::fromImage(layer->image));
            item->setZValue(++zOrder);
            item->setOpacity(layer->opacity / 255.0);
            m_layerItems.append(item);
        }
    }
}

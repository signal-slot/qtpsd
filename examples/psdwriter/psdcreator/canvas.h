// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef CANVAS_H
#define CANVAS_H

#include <QtCore/QPersistentModelIndex>
#include <QtWidgets/QGraphicsView>

class QGraphicsTextItem;
class LayerModel;

class Canvas : public QGraphicsView
{
    Q_OBJECT
public:
    enum Tool {
        BrushTool,
        EraserTool,
        FillTool,
    };

    explicit Canvas(LayerModel *model, QWidget *parent = nullptr);

    void setActiveLayer(const QModelIndex &index);
    QModelIndex activeLayer() const;

    void setTool(Tool tool);
    Tool tool() const;

    void setColor(const QColor &color);
    QColor color() const;

    void setBrushSize(int size);
    int brushSize() const;

    void updateLayers();

signals:
    void modified();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    void drawStroke(const QPointF &from, const QPointF &to);
    void fillActiveLayer();
    void refreshPixmaps();
    void addLayerItems(const QModelIndex &parent, int &zOrder);
    QGraphicsTextItem *createTextItem(const struct Layer &layer);

    LayerModel *m_model = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QList<QGraphicsItem *> m_layerItems;
    QPersistentModelIndex m_activeLayer;
    Tool m_tool = BrushTool;
    QColor m_color = Qt::black;
    int m_brushSize = 5;
    bool m_drawing = false;
    QPointF m_lastPoint;
};

#endif // CANVAS_H

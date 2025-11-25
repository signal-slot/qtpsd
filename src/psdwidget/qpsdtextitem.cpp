// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdtextitem.h"

#include <QtGui/QPainter>
#include <QtGui/QWindow>

QT_BEGIN_NAMESPACE

QPsdTextItem::QPsdTextItem(const QModelIndex &index, const QPsdTextLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QGraphicsItem *parent)
    : QPsdAbstractItem(index, psdData, maskItem, group, parent)
{}

void QPsdTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const auto *layer = this->layer<QPsdTextLayerItem>();
    QRect rect;

    if (layer->textType() != QPsdTextLayerItem::TextType::ParagraphText) {
        rect = layer->fontAdjustedBounds().toRect();
    } else {
        rect = layer->rect();
    }

    const auto runs = layer->runs();
    struct Chunk {
        QFont font;
        QColor color;
        QString text;
        int alignment;
        QSizeF size;
    };

    int flag;
    if (layer->textType() == QPsdTextLayerItem::TextType::ParagraphText) {
        flag = Qt::TextWrapAnywhere;
    } else {
        flag = Qt::AlignHCenter;
    }

    QList<QList<Chunk>> lines;
    QList<Chunk> currentLine;
    for (const auto &run : runs) {
        bool isFirst = true;

        for (const QString &line : run.text.split("\n"_L1)) {
            if (isFirst) {
                isFirst = false;
            } else {
                lines.append(currentLine);
                currentLine.clear();
            }
            Chunk chunk;
            chunk.font = run.font;
            chunk.font.setStyleStrategy(QFont::PreferTypoLineMetrics);
            chunk.color = run.color;
            chunk.text = line;
            chunk.alignment = run.alignment | flag;
            painter->setFont(chunk.font);
            QFontMetrics fontMetrics(chunk.font);
            auto bRect = painter->boundingRect(rect, chunk.alignment, line);
            chunk.size = bRect.size();
            // adjust size, for boundingRect is too small?
            if (chunk.font.pointSizeF() * 1.5 > chunk.size.height()) {
                chunk.size.setHeight(chunk.font.pointSizeF() * 1.5);
            }
            currentLine.append(chunk);
        }
    }
    if (!currentLine.isEmpty()) {
        lines.append(currentLine);
    }

    qreal contentHeight = 0;
    for (const auto &line : lines) {
        qreal maxHeight = 0;
        for (const auto &chunk : line) {
            const auto size = chunk.size;
            if (size.height() > maxHeight)
                maxHeight = size.height();
        }
        contentHeight += maxHeight;
    }

    rect.setHeight(contentHeight);

    qreal y = 0;
    for (const auto &line : lines) {
        QSizeF size;
        for (const auto &chunk : line) {
            const auto w = chunk.size.width();
            const auto h = chunk.size.height();
            if (size.isEmpty()) {
                size = QSize(w, h);
            } else {
                size.setWidth(size.width() + w);
                if (h > size.height())
                    size.setHeight(h);
            }
        }
        // Calculate x position based on text alignment instead of forcing center alignment
        qreal x = 0;

        // Use the original layer bounds to determine proper positioning
        const auto layerBounds = layer->bounds();
        const auto widgetGeom = rect;

        // Calculate the offset from widget origin to layer bounds origin
        qreal layerOffsetX = layerBounds.x() - widgetGeom.x();

        // Extract horizontal alignment from the first chunk
        Qt::Alignment horizontalAlignment = Qt::AlignLeft; // Default to left alignment
        if (!line.isEmpty()) {
            horizontalAlignment = static_cast<Qt::Alignment>(line.first().alignment & Qt::AlignHorizontal_Mask);
        }

        // Position text based on the PSD horizontal alignment
        switch (horizontalAlignment) {
        case Qt::AlignLeft:
            // For left alignment, position at the layer bounds left edge
            x = layerOffsetX;
            break;
        case Qt::AlignRight:
            // For right alignment, position so text ends at layer bounds right edge
            x = layerOffsetX + layerBounds.width() - size.width();
            break;
        case Qt::AlignHCenter:
            // For center alignment, center within layer bounds
            x = layerOffsetX + (layerBounds.width() - size.width()) / 2;
            break;
        case Qt::AlignJustify:
            // For justify, treat as left alignment for now
            x = layerOffsetX;
            break;
        default:
            // Default to left alignment
            x = layerOffsetX;
            break;
        }

        // Ensure x is not negative (fallback to left alignment)
        if (x < 0) {
            x = 0;
        }

        for (const auto &chunk : line) {
            painter->setFont(chunk.font);
            painter->setPen(chunk.color);
            // qDebug() << chunk.text << chunk.size << chunk.alignment;
            painter->drawText(x, y, chunk.size.width(), chunk.size.height(), chunk.alignment, chunk.text);
            x += chunk.size.width();
        }
        y += size.height();
    }
}

QT_END_NAMESPACE

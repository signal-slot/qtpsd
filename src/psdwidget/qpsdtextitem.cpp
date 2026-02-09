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

    const auto compositionMode = groupCompositionMode() != QPainter::CompositionMode_SourceOver
        ? groupCompositionMode()
        : QtPsdGui::compositionMode(layer->record().blendMode());
    painter->setCompositionMode(compositionMode);
    // Apply both opacity and fill opacity for text content
    painter->setOpacity(layer->opacity() * layer->fillOpacity());

    QRect rect;

    if (layer->textType() != QPsdTextLayerItem::TextType::ParagraphText) {
        // Use PSD bounds directly instead of recalculated fontAdjustedBounds
        // fontAdjustedBounds depends on Qt font metrics which vary by font
        rect = layer->bounds().toRect();
    } else {
        rect = layer->rect();
    }

    // Calculate offset from item position (layer->rect()) to actual text bounds
    // The item is positioned at layer->rect().topLeft(), but text should be at bounds()
    const QPointF boundsOffset = layer->bounds().topLeft() - QPointF(layer->rect().topLeft());
    qreal baseY = boundsOffset.y();
    qreal baseX = boundsOffset.x();

    // For point text, use baseline-based positioning from textOrigin
    // textOrigin() returns the baseline anchor position (transform tx, ty)
    const bool isPointText = (layer->textType() == QPsdTextLayerItem::TextType::PointText);
    qreal baselineY = 0;
    qreal pointTextX = 0;
    if (isPointText) {
        // For point text, use the transform origin as the anchor point
        pointTextX = layer->textOrigin().x() - layer->rect().left();
        baselineY = layer->textOrigin().y() - layer->rect().top();
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
            // Scale font for Qt rendering (PSD stores original size, Qt needs DPI adjustment)
            chunk.font.setPointSizeF(run.font.pointSizeF() / 1.5);
            chunk.font.setStyleStrategy(QFont::PreferTypoLineMetrics);
            chunk.color = run.color;
            chunk.text = line;
            chunk.alignment = run.alignment | flag;
            painter->setFont(chunk.font);
            QFontMetrics fontMetrics(chunk.font);
            auto bRect = painter->boundingRect(rect, chunk.alignment, line);
            chunk.size = bRect.size();
            // adjust size, for boundingRect is too small?
            if (run.font.pointSizeF() > chunk.size.height()) {
                chunk.size.setHeight(run.font.pointSizeF());
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

    qreal y = baseY;  // Start from the bounds offset
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

        // Calculate x position based on text alignment
        qreal x;

        // Extract horizontal alignment from the first chunk
        Qt::Alignment horizontalAlignment = Qt::AlignLeft; // Default to left alignment
        if (!line.isEmpty()) {
            horizontalAlignment = static_cast<Qt::Alignment>(line.first().alignment & Qt::AlignHorizontal_Mask);
        }

        if (isPointText) {
            // For point text, textOrigin is the anchor point
            // Alignment determines where the text is positioned relative to this anchor
            switch (horizontalAlignment) {
            case Qt::AlignLeft:
                // Text starts at anchor
                x = pointTextX;
                break;
            case Qt::AlignRight:
                // Text ends at anchor
                x = pointTextX - size.width();
                break;
            case Qt::AlignHCenter:
                // Text is centered on anchor
                x = pointTextX - size.width() / 2;
                break;
            default:
                x = pointTextX;
                break;
            }
        } else {
            // For paragraph text, use bounds-based positioning
            x = baseX;
            const auto layerBounds = layer->bounds();

            switch (horizontalAlignment) {
            case Qt::AlignLeft:
                // For left alignment, already at baseX
                break;
            case Qt::AlignRight:
                // For right alignment, position so text ends at layer bounds right edge
                x = baseX + layerBounds.width() - size.width();
                break;
            case Qt::AlignHCenter:
                // For center alignment, center within layer bounds
                x = baseX + (layerBounds.width() - size.width()) / 2;
                break;
            case Qt::AlignJustify:
                // For justify, treat as left alignment for now
                break;
            default:
                break;
            }
        }

        for (const auto &chunk : line) {
            painter->setFont(chunk.font);
            painter->setPen(chunk.color);
            if (isPointText) {
                // For point text, draw at the baseline position directly
                // drawText(QPointF, QString) uses the point as baseline-left
                painter->drawText(QPointF(x, baselineY), chunk.text);
                x += painter->fontMetrics().horizontalAdvance(chunk.text);
            } else {
                painter->drawText(x, y, chunk.size.width(), chunk.size.height(), chunk.alignment, chunk.text);
                x += chunk.size.width();
            }
        }
        if (isPointText) {
            baselineY += size.height();
        }
        y += size.height();
    }
}

QT_END_NAMESPACE

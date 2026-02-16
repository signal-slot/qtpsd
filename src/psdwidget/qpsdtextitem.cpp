// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdtextitem.h"

#include <QtGui/QPainter>
#include <QtGui/QWindow>

QT_BEGIN_NAMESPACE

QPsdTextItem::QPsdTextItem(const QModelIndex &index, const QPsdTextLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QGraphicsItem *parent)
    : QPsdAbstractItem(index, psdData, maskItem, group, parent)
{}

QRectF QPsdTextItem::boundingRect() const
{
    const auto *layer = this->layer<QPsdTextLayerItem>();
    QRectF base = QPsdAbstractItem::boundingRect();
    if (layer->textType() == QPsdTextLayerItem::TextType::ParagraphText
        && !layer->textFrame().isEmpty()) {
        const QRectF frameLocal(
            layer->textFrame().left() - layer->rect().left(),
            layer->textFrame().top() - layer->rect().top(),
            layer->textFrame().width(),
            layer->textFrame().height());
        return base.united(frameLocal);
    }
    return base;
}

void QPsdTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const auto *layer = this->layer<QPsdTextLayerItem>();

    // Determine the effective blend mode
    const auto blendMode = groupCompositionMode() != QPainter::CompositionMode_SourceOver
        ? QPsdBlend::Mode::Normal
        : layer->record().blendMode();

    // For custom blend modes, redirect rendering to a temp image
    QImage tempImage;
    QPainter tempPainter;
    QPainter *p = painter;
    const bool useCustomBlend = QtPsdGui::isCustomBlendMode(blendMode);

    if (useCustomBlend) {
        // Get the bounding rect in device coordinates to create temp image
        const QRectF br = boundingRect();
        const QTransform xf = painter->combinedTransform();
        const QRect deviceRect = xf.mapRect(br).toAlignedRect();
        if (!deviceRect.isEmpty()) {
            tempImage = QImage(deviceRect.size(), QImage::Format_ARGB32_Premultiplied);
            tempImage.fill(Qt::transparent);
            tempPainter.begin(&tempImage);
            tempPainter.setRenderHints(painter->renderHints());
            // Translate so drawing at local coordinates maps to temp image
            tempPainter.translate(-br.topLeft());
            p = &tempPainter;
        }
    }

    if (!useCustomBlend) {
        const auto compositionMode = groupCompositionMode() != QPainter::CompositionMode_SourceOver
            ? groupCompositionMode()
            : QtPsdGui::compositionMode(layer->record().blendMode());
        painter->setCompositionMode(compositionMode);
        // Apply both opacity and fill opacity for text content
        painter->setOpacity(layer->opacity() * layer->fillOpacity());
    }

    const bool isPointText = (layer->textType() == QPsdTextLayerItem::TextType::PointText);
    const bool hasParagraphFrame = !isPointText && !layer->textFrame().isEmpty();

    const auto runs = layer->runs();
    struct Chunk {
        QFont font;
        QColor color;
        QString text;
        Qt::Alignment alignment;
        qreal width;
    };

    // Split runs into lines by newlines
    QList<QList<Chunk>> lines;
    QList<Chunk> currentLine;
    for (const auto &run : runs) {
        bool isFirst = true;
        for (const QString &part : run.text.split("\n"_L1)) {
            if (isFirst) {
                isFirst = false;
            } else {
                lines.append(currentLine);
                currentLine.clear();
            }
            if (part.isEmpty())
                continue;
            Chunk chunk;
            chunk.font = run.font;
            chunk.font.setPointSizeF(run.font.pointSizeF() / 1.5);
            chunk.font.setStyleStrategy(QFont::PreferTypoLineMetrics);
            chunk.color = run.color;
            chunk.text = part;
            chunk.alignment = run.alignment;
            p->setFont(chunk.font);
            chunk.width = p->fontMetrics().horizontalAdvance(part);
            currentLine.append(chunk);
        }
    }
    if (!currentLine.isEmpty()) {
        lines.append(currentLine);
    }

    // Calculate text metrics for positioning
    QFont primaryFont;
    for (const auto &line : lines) {
        if (!line.isEmpty()) {
            primaryFont = line.first().font;
            break;
        }
    }
    QFontMetrics fm(primaryFont);

    if (isPointText) {
        // Point text: textOrigin is the baseline anchor
        const qreal anchorX = layer->textOrigin().x() - layer->rect().left();
        qreal baselineY = layer->textOrigin().y() - layer->rect().top();

        for (const auto &line : lines) {
            if (line.isEmpty()) {
                baselineY += fm.height();
                continue;
            }

            // Calculate total line width
            qreal lineWidth = 0;
            for (const auto &chunk : line)
                lineWidth += chunk.width;

            // Position based on alignment
            const auto hAlign = static_cast<Qt::Alignment>(line.first().alignment & Qt::AlignHorizontal_Mask);
            qreal x = anchorX;
            if (hAlign == Qt::AlignHCenter)
                x = anchorX - lineWidth / 2;
            else if (hAlign == Qt::AlignRight)
                x = anchorX - lineWidth;

            for (const auto &chunk : line) {
                p->setFont(chunk.font);
                p->setPen(chunk.color);
                p->drawText(QPointF(x, baselineY), chunk.text);
                x += p->fontMetrics().horizontalAdvance(chunk.text);
            }
            baselineY += fm.height();
        }
    } else {
        // Paragraph text: draw line by line with proper Y advancement
        qreal drawLeft, drawTop, drawWidth;
        if (hasParagraphFrame) {
            drawLeft = layer->textFrame().left() - layer->rect().left();
            drawTop = layer->textFrame().top() - layer->rect().top();
            drawWidth = layer->textFrame().width();
            // Adjust for cap-height alignment
            const qreal capHeightOffset = fm.ascent() - fm.capHeight();
            drawTop -= capHeightOffset;
        } else {
            // Use bounds offset for positioning (matches old behavior)
            const QPointF boundsOffset = layer->bounds().topLeft() - QPointF(layer->rect().topLeft());
            drawLeft = boundsOffset.x();
            drawTop = boundsOffset.y();
            drawWidth = layer->bounds().isEmpty() ? layer->rect().width() : layer->bounds().width();
        }

        qreal currentY = drawTop;
        for (const auto &line : lines) {
            if (line.isEmpty()) {
                currentY += fm.height();
                continue;
            }

            // Concatenate all chunks in the line (for wrapping to work correctly)
            // and draw with the first chunk's style
            // TODO: support mixed-style lines with wrapping
            QString lineText;
            for (const auto &chunk : line)
                lineText += chunk.text;

            const auto hAlign = static_cast<Qt::Alignment>(line.first().alignment & Qt::AlignHorizontal_Mask);
            const QRectF lineRect(drawLeft, currentY, drawWidth, fm.height() * 100);

            p->setFont(line.first().font);
            p->setPen(line.first().color);
            QRectF boundingRect;
            p->drawText(lineRect, Qt::TextWordWrap | hAlign | Qt::AlignTop, lineText, &boundingRect);
            currentY += boundingRect.height();
        }
    }

    // Blend temp image back to backbuffer using custom blend mode
    if (useCustomBlend && !tempImage.isNull()) {
        tempPainter.end();
        const qreal opacity = layer->opacity() * layer->fillOpacity();
        QImage *backbuffer = dynamic_cast<QImage *>(painter->device());
        if (backbuffer) {
            const QTransform xf = painter->combinedTransform();
            const QRectF br = boundingRect();
            const QRect deviceRect = xf.mapRect(br).toAlignedRect();
            const QRect clipped = deviceRect.intersected(backbuffer->rect());
            if (!clipped.isEmpty()) {
                QImage destRegion = backbuffer->copy(clipped).convertToFormat(QImage::Format_ARGB32_Premultiplied);
                QImage srcRegion = tempImage.copy(
                    clipped.x() - deviceRect.x(),
                    clipped.y() - deviceRect.y(),
                    clipped.width(), clipped.height()
                ).convertToFormat(QImage::Format_ARGB32_Premultiplied);
                QtPsdGui::customBlend(destRegion, srcRegion, blendMode, opacity);
                painter->save();
                painter->resetTransform();
                painter->setCompositionMode(QPainter::CompositionMode_Source);
                painter->setOpacity(1.0);
                painter->drawImage(clipped.topLeft(), destRegion);
                painter->restore();
            }
        }
    }
}

QT_END_NAMESPACE

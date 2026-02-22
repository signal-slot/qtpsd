// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdtextitem.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
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
        qreal lineHeight = -1;
        qreal baselineShift = 0;
        qreal horizontalScale = 1.0;
        qreal verticalScale = 1.0;
        int fontCaps = 0;
        bool fauxItalic = false;
        qreal spaceBefore = 0;
        qreal spaceAfter = 0;
        qreal firstLineIndent = 0;
        qreal startIndent = 0;
        qreal endIndent = 0;
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
            const qreal dpiScale = QGuiApplication::primaryScreen()->logicalDotsPerInchY() / 72.0;
            chunk.font.setPointSizeF(run.font.pointSizeF() / dpiScale);
            chunk.font.setStyleStrategy(QFont::PreferTypoLineMetrics);

            // Apply faux styles to font
            if (run.fauxBold)
                chunk.font.setWeight(QFont::Bold);
            // Faux italic: use shear transform like Photoshop, not font italic
            // (Photoshop shears regular glyphs; Qt italic loads different glyphs)
            chunk.fauxItalic = run.fauxItalic;
            if (run.underline)
                chunk.font.setUnderline(true);
            if (run.strikethrough)
                chunk.font.setStrikeOut(true);

            chunk.color = run.color;
            chunk.text = part;
            chunk.alignment = run.alignment;

            // Apply allCaps text transform
            if (run.fontCaps == 2)
                chunk.text = chunk.text.toUpper();

            if (run.lineHeight > 0)
                chunk.lineHeight = run.lineHeight / dpiScale;

            // Copy scaled properties (already in scaled points, divide by dpiScale)
            chunk.baselineShift = run.baselineShift / dpiScale;
            chunk.horizontalScale = run.horizontalScale;
            chunk.verticalScale = run.verticalScale;
            chunk.fontCaps = run.fontCaps;
            chunk.spaceBefore = run.spaceBefore / dpiScale;
            chunk.spaceAfter = run.spaceAfter / dpiScale;
            chunk.firstLineIndent = run.firstLineIndent / dpiScale;
            chunk.startIndent = run.startIndent / dpiScale;
            chunk.endIndent = run.endIndent / dpiScale;

            p->setFont(chunk.font);
            chunk.width = p->fontMetrics().horizontalAdvance(chunk.text) * chunk.horizontalScale;
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

        bool firstLine = true;
        for (const auto &line : lines) {
            const qreal lineAdvance = (!line.isEmpty() && line.first().lineHeight > 0)
                ? line.first().lineHeight : fm.height();

            // Add spaceBefore at paragraph boundaries (each line after a newline split)
            if (!firstLine && !line.isEmpty()) {
                baselineY += line.first().spaceBefore;
            }

            if (line.isEmpty()) {
                baselineY += lineAdvance;
                firstLine = false;
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
                const qreal adjustedY = baselineY - chunk.baselineShift;

                // Photoshop faux italic: shear regular glyphs by ~12°
                constexpr qreal fauxItalicShear = -0.2126; // tan(12°)
                const bool needsTransform = chunk.fauxItalic || chunk.horizontalScale != 1.0;

                if (chunk.fontCaps == 1) {
                    // Small caps: draw character by character
                    qreal cx = x;
                    for (const QChar &ch : chunk.text) {
                        QFont f = chunk.font;
                        const QString drawChar = QString(ch.toUpper());
                        if (ch.isLower())
                            f.setPointSizeF(f.pointSizeF() * 0.7);
                        p->setFont(f);
                        if (needsTransform) {
                            p->save();
                            p->translate(cx, adjustedY);
                            if (chunk.fauxItalic)
                                p->shear(fauxItalicShear, 0);
                            if (chunk.horizontalScale != 1.0)
                                p->scale(chunk.horizontalScale, 1.0);
                            p->drawText(QPointF(0, 0), drawChar);
                            p->restore();
                        } else {
                            p->drawText(QPointF(cx, adjustedY), drawChar);
                        }
                        cx += QFontMetricsF(f).horizontalAdvance(drawChar) * chunk.horizontalScale;
                    }
                    x = cx;
                } else if (needsTransform) {
                    p->save();
                    p->translate(x, adjustedY);
                    if (chunk.fauxItalic)
                        p->shear(fauxItalicShear, 0);
                    if (chunk.horizontalScale != 1.0)
                        p->scale(chunk.horizontalScale, 1.0);
                    p->drawText(QPointF(0, 0), chunk.text);
                    p->restore();
                    x += chunk.width;
                } else {
                    p->drawText(QPointF(x, adjustedY), chunk.text);
                    x += chunk.width;
                }
            }
            baselineY += lineAdvance;

            // Add spaceAfter at the end of each line
            if (!line.isEmpty())
                baselineY += line.first().spaceAfter;

            firstLine = false;
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
        bool isFirstLineOfParagraph = true;
        for (int i = 0; i < lines.size(); ++i) {
            const auto &line = lines.at(i);
            const qreal lineAdvance = (!line.isEmpty() && line.first().lineHeight > 0)
                ? line.first().lineHeight : fm.height();
            if (line.isEmpty()) {
                currentY += lineAdvance;
                isFirstLineOfParagraph = true;
                continue;
            }

            const auto &firstChunk = line.first();

            // Add spaceBefore at paragraph boundaries
            if (isFirstLineOfParagraph)
                currentY += firstChunk.spaceBefore;

            // Calculate indent-adjusted rect
            qreal indentLeft = firstChunk.startIndent;
            qreal indentRight = firstChunk.endIndent;
            if (isFirstLineOfParagraph)
                indentLeft += firstChunk.firstLineIndent;
            const QRectF lineRect(drawLeft + indentLeft, currentY,
                                  drawWidth - indentLeft - indentRight, fm.height() * 100);

            // Concatenate all chunks in the line (for wrapping to work correctly)
            // and draw with the first chunk's style
            // TODO: support mixed-style lines with wrapping
            QString lineText;
            for (const auto &chunk : line)
                lineText += chunk.text;

            // Apply allCaps for paragraph text as well (already applied in chunk creation)
            const auto hAlign = static_cast<Qt::Alignment>(firstChunk.alignment & Qt::AlignHorizontal_Mask);

            p->setFont(firstChunk.font);
            p->setPen(firstChunk.color);
            QRectF boundingRect;
            if (firstChunk.fauxItalic) {
                constexpr qreal fauxItalicShear = -0.2126;
                p->save();
                p->shear(fauxItalicShear, 0);
                p->drawText(lineRect, Qt::TextWordWrap | hAlign | Qt::AlignTop, lineText, &boundingRect);
                p->restore();
            } else {
                p->drawText(lineRect, Qt::TextWordWrap | hAlign | Qt::AlignTop, lineText, &boundingRect);
            }
            currentY += boundingRect.height();

            // Add spaceAfter at end of paragraph
            currentY += firstChunk.spaceAfter;

            // Next line after a newline boundary is a new paragraph
            isFirstLineOfParagraph = true;
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

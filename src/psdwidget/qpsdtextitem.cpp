// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdtextitem.h"
#include "qpsdblur_p.h"

#include <QtCore/QCborMap>
#include <QtGui/QGuiApplication>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtMath>

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

    // Drop shadow support: redirect drawing to temp image if shadow present
    const auto shadowMap = layer->dropShadow();
    const bool hasShadow = !shadowMap.isEmpty() && !useCustomBlend;
    QImage shadowTextImage;
    QPainter shadowTextPainter;
    if (hasShadow) {
        const QRectF br = boundingRect();
        shadowTextImage = QImage(br.size().toSize(), QImage::Format_ARGB32);
        shadowTextImage.fill(Qt::transparent);
        shadowTextPainter.begin(&shadowTextImage);
        shadowTextPainter.setRenderHints(p->renderHints());
        p = &shadowTextPainter;
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
            chunk.color = run.color;
            chunk.text = part;
            chunk.alignment = run.alignment;
            if (run.lineHeight > 0)
                chunk.lineHeight = run.lineHeight / dpiScale;
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
            const qreal lineAdvance = (!line.isEmpty() && line.first().lineHeight > 0)
                ? line.first().lineHeight : fm.height();
            if (line.isEmpty()) {
                baselineY += lineAdvance;
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
            baselineY += lineAdvance;
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
            const qreal lineAdvance = (!line.isEmpty() && line.first().lineHeight > 0)
                ? line.first().lineHeight : fm.height();
            if (line.isEmpty()) {
                currentY += lineAdvance;
                continue;
            }

            // Check if the line fits without wrapping
            qreal totalLineWidth = 0;
            for (const auto &chunk : line)
                totalLineWidth += chunk.width;

            const auto hAlign = static_cast<Qt::Alignment>(line.first().alignment & Qt::AlignHorizontal_Mask);

            if (line.size() > 1 && totalLineWidth <= drawWidth) {
                // Mixed-style line that fits: draw each chunk with its own style
                qreal x = drawLeft;
                if (hAlign == Qt::AlignHCenter)
                    x = drawLeft + (drawWidth - totalLineWidth) / 2;
                else if (hAlign == Qt::AlignRight)
                    x = drawLeft + drawWidth - totalLineWidth;

                for (const auto &chunk : line) {
                    p->setFont(chunk.font);
                    p->setPen(chunk.color);
                    p->drawText(QPointF(x, currentY + QFontMetrics(chunk.font).ascent()), chunk.text);
                    x += p->fontMetrics().horizontalAdvance(chunk.text);
                }
                currentY += lineAdvance;
            } else {
                // Single-style line or needs wrapping: draw as one block
                QString lineText;
                for (const auto &chunk : line)
                    lineText += chunk.text;

                const QRectF lineRect(drawLeft, currentY, drawWidth, fm.height() * 100);
                p->setFont(line.first().font);
                p->setPen(line.first().color);
                QRectF boundingRect;
                p->drawText(lineRect, Qt::TextWordWrap | hAlign | Qt::AlignTop, lineText, &boundingRect);
                currentY += boundingRect.height();
            }
        }
    }

    // Draw drop shadow and text from temp image
    if (hasShadow && !shadowTextImage.isNull()) {
        shadowTextPainter.end();

        const QColor shadowColor(shadowMap.value(QLatin1String("color")).toString());
        const qreal shadowOpacity = shadowMap.value(QLatin1String("opacity")).toDouble(1.0);
        const qreal angle = shadowMap.value(QLatin1String("angle")).toDouble(0);
        const qreal distance = shadowMap.value(QLatin1String("distance")).toDouble(0);
        const qreal blur = shadowMap.value(QLatin1String("size")).toDouble(0);

        const qreal angleRad = qDegreesToRadians(angle);
        const QPointF offset(qCos(angleRad) * distance, qSin(angleRad) * distance);

        // Create shadow image from text alpha
        // Fill ALL pixels with shadow color RGB, keeping their alpha
        // This prevents black-fringe artifacts when blur spreads alpha
        // into previously-transparent (black RGB) pixels
        QImage shadowImage = shadowTextImage;
        for (int y = 0; y < shadowImage.height(); ++y) {
            QRgb *scanLine = reinterpret_cast<QRgb *>(shadowImage.scanLine(y));
            for (int x = 0; x < shadowImage.width(); ++x) {
                scanLine[x] = qRgba(shadowColor.red(), shadowColor.green(), shadowColor.blue(),
                                     qAlpha(scanLine[x]));
            }
        }

        // Apply blur (3-pass box blur approximates gaussian)
        const QColor transparentShadow(shadowColor.red(), shadowColor.green(), shadowColor.blue(), 0);
        if (blur > 0) {
            const int blurRadius = qMax(1, static_cast<int>(blur));
            const int margin = blurRadius * 2;
            QImage expanded(shadowImage.width() + margin * 2,
                            shadowImage.height() + margin * 2,
                            QImage::Format_ARGB32);
            expanded.fill(transparentShadow);
            QPainter ep(&expanded);
            ep.drawImage(margin, margin, shadowImage);
            ep.end();
            boxBlurAlpha(expanded, blurRadius);
            boxBlurAlpha(expanded, blurRadius);
            boxBlurAlpha(expanded, blurRadius);
            painter->save();
            painter->setOpacity(shadowOpacity);
            painter->drawImage(offset.toPoint() - QPoint(margin, margin), expanded);
            painter->restore();
        } else {
            painter->save();
            painter->setOpacity(shadowOpacity);
            painter->drawImage(offset.toPoint(), shadowImage);
            painter->restore();
        }

        // Draw the text on top of the shadow
        painter->drawImage(0, 0, shadowTextImage);
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

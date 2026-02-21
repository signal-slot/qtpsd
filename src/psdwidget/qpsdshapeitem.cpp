// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdshapeitem.h"
#include "qpsdblur_p.h"

#include <QtCore/QCborMap>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtMath>
#include <QtPsdCore/QPsdVectorMaskSetting>
#include <QtPsdGui/QPsdBorder>
#include <QtPsdGui/QPsdPatternFill>
#include <QtPsdWidget/QPsdScene>

static void drawShapePath(QPainter *p, const QPsdAbstractLayerItem::PathInfo &pathInfo)
{
    switch (pathInfo.type) {
    case QPsdAbstractLayerItem::PathInfo::Rectangle:
        // Axis-aligned rectangles: disable AA to match Figma's crisp pixel rendering
        p->setRenderHint(QPainter::Antialiasing, false);
        p->drawRect(pathInfo.rect);
        p->setRenderHint(QPainter::Antialiasing, true);
        break;
    case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
        p->drawRoundedRect(pathInfo.rect, pathInfo.radius, pathInfo.radius);
        break;
    default:
        p->drawPath(pathInfo.path);
        break;
    }
}

QPsdShapeItem::QPsdShapeItem(const QModelIndex &index, const QPsdShapeLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QGraphicsItem *parent)
    : QPsdAbstractItem(index, psdData, maskItem, group, parent)
{}

void QPsdShapeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const auto *layer = this->layer<QPsdShapeLayerItem>();

    setMask(painter);

    const auto pathInfo = layer->pathInfo();

    // Drop shadow rendering (from dropShadow QCborMap, e.g. Figma import)
    const auto shadow = layer->dropShadow();
    if (!shadow.isEmpty()
        && pathInfo.type != QPsdAbstractLayerItem::PathInfo::None) {
        const QColor shadowColor(shadow.value(QLatin1String("color")).toString());
        const qreal shadowOpacity = shadow.value(QLatin1String("opacity")).toDouble(1.0);
        const qreal angle = shadow.value(QLatin1String("angle")).toDouble(0);
        const qreal distance = shadow.value(QLatin1String("distance")).toDouble(0);
        const qreal blur = shadow.value(QLatin1String("size")).toDouble(0);

        const qreal angleRad = qDegreesToRadians(angle);
        const QPointF offset(qCos(angleRad) * distance, qSin(angleRad) * distance);

        // Render shape silhouette with shadow color
        // Fill with shadow color at zero alpha so blur edge pixels inherit
        // the correct RGB (otherwise transparent pixels are black)
        const QSize shapeSize = layer->rect().size();
        const QColor transparentShadow(shadowColor.red(), shadowColor.green(), shadowColor.blue(), 0);
        QImage silhouette(shapeSize, QImage::Format_ARGB32);
        silhouette.fill(transparentShadow);
        {
            QPainter sp(&silhouette);
            sp.setRenderHint(QPainter::Antialiasing);
            sp.setPen(Qt::NoPen);
            sp.setBrush(shadowColor);
            drawShapePath(&sp, pathInfo);
        }

        // Apply blur (3-pass box blur approximates gaussian)
        if (blur > 0) {
            const int blurRadius = qMax(1, static_cast<int>(blur));
            const int margin = blurRadius * 2;
            QImage expanded(silhouette.width() + margin * 2,
                            silhouette.height() + margin * 2,
                            QImage::Format_ARGB32);
            expanded.fill(transparentShadow);
            QPainter ep(&expanded);
            ep.drawImage(margin, margin, silhouette);
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
            painter->drawImage(offset.toPoint(), silhouette);
            painter->restore();
        }
    }

    // Determine the effective blend mode
    const auto blendMode = groupCompositionMode() != QPainter::CompositionMode_SourceOver
        ? QPsdBlend::Mode::Normal
        : layer->record().blendMode();
    const bool useCustomBlend = QtPsdGui::isCustomBlendMode(blendMode);

    if (!useCustomBlend) {
        const auto compositionMode = groupCompositionMode() != QPainter::CompositionMode_SourceOver
            ? groupCompositionMode()
            : QtPsdGui::compositionMode(layer->record().blendMode());
        painter->setCompositionMode(compositionMode);
        painter->setOpacity(abstractLayer()->opacity() * abstractLayer()->fillOpacity());
    }
    painter->setRenderHint(QPainter::Antialiasing);

    // Check for raster layer mask - if present, render shape into a temp image
    // so we can apply per-pixel alpha masking (same technique as QPsdImageItem)
    const QImage layerMask = layer->layerMask();
    const bool hasMask = !layerMask.isNull();
    const qreal blurRadius = layer->layerBlur();
    const bool hasLayerBlur = blurRadius > 0;
    const bool needTempImage = hasMask || useCustomBlend || hasLayerBlur;

    // For layer blur, expand temp image by blur margin
    const int blurMargin = hasLayerBlur ? qMax(1, static_cast<int>(blurRadius + 0.5)) * 3 : 0;

    QImage tempImage;
    QPainter tempPainterObj;
    QPainter *p = painter;

    if (needTempImage) {
        const QSize imgSize = hasLayerBlur
            ? QSize(layer->rect().width() + blurMargin * 2, layer->rect().height() + blurMargin * 2)
            : layer->rect().size();
        tempImage = QImage(imgSize, QImage::Format_ARGB32);
        tempImage.fill(Qt::transparent);
        tempPainterObj.begin(&tempImage);
        tempPainterObj.setRenderHint(QPainter::Antialiasing);
        if (hasLayerBlur)
            tempPainterObj.translate(blurMargin, blurMargin);
        p = &tempPainterObj;
    }

    const auto *gradient = layer->gradient();
    const auto *border = layer->border();
    const auto *patternFill = layer->patternFill();
    if (gradient) {
        p->setPen(Qt::NoPen);
        p->setBrush(QBrush(*gradient));
    } else if (border) {
        p->setPen(QPen(border->color(), border->size()));
    } else if (patternFill) {
        auto *psdScene = qobject_cast<QPsdScene *>(scene());
        if (psdScene) {
            QImage patternImage = psdScene->patternImage(patternFill->patternID());
            if (!patternImage.isNull()) {
                qreal sc = patternFill->scale() / 100.0;
                if (sc != 1.0 && sc > 0) {
                    patternImage = patternImage.scaled(
                        qRound(patternImage.width() * sc),
                        qRound(patternImage.height() * sc),
                        Qt::IgnoreAspectRatio,
                        Qt::SmoothTransformation);
                }

                QBrush brush(patternImage);

                QTransform transform;
                if (patternFill->angle() != 0)
                    transform.rotate(patternFill->angle());
                const QPointF phase = patternFill->phase();
                if (!phase.isNull())
                    transform.translate(phase.x(), phase.y());
                if (!transform.isIdentity())
                    brush.setTransform(transform);

                p->setPen(Qt::NoPen);
                p->setBrush(brush);
            }
        }
    } else if (!layer->vscgPatternId().isEmpty()) {
        auto *psdScene = qobject_cast<QPsdScene *>(scene());
        if (psdScene) {
            QImage patternImage = psdScene->patternImage(layer->vscgPatternId());
            if (!patternImage.isNull()) {
                qreal sc = layer->vscgPatternScale();
                if (sc != 1.0 && sc > 0) {
                    patternImage = patternImage.scaled(
                        qRound(patternImage.width() * sc),
                        qRound(patternImage.height() * sc),
                        Qt::IgnoreAspectRatio,
                        Qt::SmoothTransformation);
                }

                QBrush brush(patternImage);

                QTransform transform;
                if (layer->vscgPatternAngle() != 0)
                    transform.rotate(layer->vscgPatternAngle());
                if (!transform.isIdentity())
                    brush.setTransform(transform);

                p->setPen(Qt::NoPen);
                p->setBrush(brush);
            }
        }
    } else {
        p->setPen(layer->pen());
        p->setBrush(layer->brush());
    }

    const auto strokeAlignment = layer->strokeAlignment();
    if (strokeAlignment == QPsdShapeLayerItem::StrokeInside && p->pen().style() != Qt::NoPen) {
        // For "inside" stroke: draw fill first, then clip to path and draw stroke with 2x width
        QPen strokePen = p->pen();
        QBrush fillBrush = p->brush();

        // Draw fill without stroke
        p->setPen(Qt::NoPen);
        p->setBrush(fillBrush);
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::None:
            p->drawRect(layer->rect());
            break;
        case QPsdAbstractLayerItem::PathInfo::Rectangle:
            p->drawRect(pathInfo.rect);
            break;
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
            p->drawRoundedRect(pathInfo.rect, pathInfo.radius, pathInfo.radius);
            break;
        default:
            p->drawPath(pathInfo.path);
            break;
        }

        // Draw stroke clipped to inside of the path
        p->save();
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::None: {
            QPainterPath clipPath;
            clipPath.addRect(layer->rect());
            p->setClipPath(clipPath, Qt::IntersectClip);
            break;
        }
        case QPsdAbstractLayerItem::PathInfo::Rectangle: {
            QPainterPath clipPath;
            clipPath.addRect(pathInfo.rect);
            p->setClipPath(clipPath, Qt::IntersectClip);
            break;
        }
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle: {
            QPainterPath clipPath;
            clipPath.addRoundedRect(pathInfo.rect, pathInfo.radius, pathInfo.radius);
            p->setClipPath(clipPath, Qt::IntersectClip);
            break;
        }
        default:
            p->setClipPath(pathInfo.path, Qt::IntersectClip);
            break;
        }
        strokePen.setWidthF(strokePen.widthF() * 2.0);
        if (strokePen.style() == Qt::CustomDashLine) {
            QVector<qreal> pattern = strokePen.dashPattern();
            for (int i = 0; i < pattern.size(); ++i) {
                pattern[i] /= 2.0;
            }
            strokePen.setDashPattern(pattern);
            strokePen.setDashOffset(strokePen.dashOffset() / 2.0);
        }
        p->setPen(strokePen);
        p->setBrush(Qt::NoBrush);
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::None:
            p->drawRect(layer->rect());
            break;
        case QPsdAbstractLayerItem::PathInfo::Rectangle:
            p->drawRect(pathInfo.rect);
            break;
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
            p->drawRoundedRect(pathInfo.rect, pathInfo.radius, pathInfo.radius);
            break;
        default:
            p->drawPath(pathInfo.path);
            break;
        }
        p->restore();
    } else if (strokeAlignment == QPsdShapeLayerItem::StrokeOutside && p->pen().style() != Qt::NoPen) {
        // For "outside" stroke: expand rect so inner edge of stroke aligns with shape boundary
        const auto dw = p->pen().widthF() / 2.0;
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::None:
            p->drawRect(layer->rect());
            break;
        case QPsdAbstractLayerItem::PathInfo::Rectangle:
            p->drawRect(pathInfo.rect.adjusted(-dw, -dw, dw, dw));
            break;
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
            p->drawRoundedRect(pathInfo.rect.adjusted(-dw, -dw, dw, dw), pathInfo.radius + dw, pathInfo.radius + dw);
            break;
        default:
            p->drawPath(pathInfo.path);
            break;
        }
    } else {
        // For "center" stroke (or no stroke): Qt centers stroke on path boundary by default
        drawShapePath(p, pathInfo);
    }

    // Inner shadow rendering (drawn on top of shape, clipped to shape boundary)
    const auto innerShadowMap = layer->innerShadow();
    if (!innerShadowMap.isEmpty()
        && pathInfo.type != QPsdAbstractLayerItem::PathInfo::None) {
        const QColor isColor(innerShadowMap.value(QLatin1String("color")).toString());
        const qreal isOpacity = innerShadowMap.value(QLatin1String("opacity")).toDouble(1.0);
        const qreal isAngle = innerShadowMap.value(QLatin1String("angle")).toDouble(0);
        const qreal isDistance = innerShadowMap.value(QLatin1String("distance")).toDouble(0);
        const qreal isBlur = innerShadowMap.value(QLatin1String("size")).toDouble(0);

        const qreal isAngleRad = qDegreesToRadians(isAngle);
        const QPointF isOffset(qCos(isAngleRad) * isDistance, qSin(isAngleRad) * isDistance);

        // Approach: blur a padded shape silhouette, then invert alpha
        // The padding ensures blur has transparent pixels at shape edges
        const QSize shapeSize = layer->rect().size();
        // Scale blur radius: 3-pass box blur gives sigma ≈ r, so use r/3
        // to match Figma's inner shadow spread of ~radius pixels
        const int blurRadius = isBlur > 0 ? qMax(1, static_cast<int>(isBlur / 3.0 + 0.5)) : 0;
        const int margin = blurRadius * 2;

        QImage silhouette(shapeSize.width() + margin * 2,
                          shapeSize.height() + margin * 2,
                          QImage::Format_ARGB32);
        silhouette.fill(Qt::transparent);
        {
            QPainter sp(&silhouette);
            sp.setRenderHint(QPainter::Antialiasing);
            sp.translate(margin, margin);
            sp.setPen(Qt::NoPen);
            sp.setBrush(Qt::white);
            drawShapePath(&sp, pathInfo);
        }

        // Blur the silhouette — transparent padding lets blur attenuate at edges
        if (blurRadius > 0) {
            boxBlurAlpha(silhouette, blurRadius);
            boxBlurAlpha(silhouette, blurRadius);
            boxBlurAlpha(silhouette, blurRadius);
        }

        // Invert alpha and fill with shadow color
        for (int y = 0; y < silhouette.height(); ++y) {
            QRgb *scanLine = reinterpret_cast<QRgb *>(silhouette.scanLine(y));
            for (int x = 0; x < silhouette.width(); ++x) {
                scanLine[x] = qRgba(isColor.red(), isColor.green(), isColor.blue(),
                                     255 - qAlpha(scanLine[x]));
            }
        }

        // Clip to shape boundary and draw with offset
        p->save();
        QPainterPath clipPath;
        switch (pathInfo.type) {
        case QPsdAbstractLayerItem::PathInfo::Rectangle:
            clipPath.addRect(pathInfo.rect);
            break;
        case QPsdAbstractLayerItem::PathInfo::RoundedRectangle:
            clipPath.addRoundedRect(pathInfo.rect, pathInfo.radius, pathInfo.radius);
            break;
        default:
            clipPath = pathInfo.path;
            break;
        }
        p->setClipPath(clipPath, Qt::IntersectClip);
        p->setOpacity(isOpacity);
        p->drawImage(isOffset.toPoint() - QPoint(margin, margin), silhouette);
        p->restore();
    }

    // Apply raster layer mask and/or custom blend, then draw result
    if (needTempImage) {
        tempPainterObj.end();

        // Apply layer blur (Gaussian approximation via 3-pass box blur)
        if (hasLayerBlur) {
            const int br = qMax(1, static_cast<int>(blurRadius / 3.0 + 0.5));
            boxBlurRgba(tempImage, br);
            boxBlurRgba(tempImage, br);
            boxBlurRgba(tempImage, br);
        }

        // Apply raster layer mask if present
        if (hasMask) {
            const QRect maskRect = layer->layerMaskRect();
            const QRect layerRect = layer->rect();
            const int defaultColor = layer->layerMaskDefaultColor();
            const int density = layer->layerMaskDensity();

            for (int y = 0; y < tempImage.height(); ++y) {
                QRgb *scanLine = reinterpret_cast<QRgb *>(tempImage.scanLine(y));
                for (int x = 0; x < tempImage.width(); ++x) {
                    const int maskX = (layerRect.x() + x) - maskRect.x();
                    const int maskY = (layerRect.y() + y) - maskRect.y();

                    int maskValue = defaultColor;
                    if (maskX >= 0 && maskX < layerMask.width()
                        && maskY >= 0 && maskY < layerMask.height()) {
                        maskValue = qGray(layerMask.pixel(maskX, maskY));
                    }

                    // Apply density: scale mask effect (255=full, 0=no masking)
                    maskValue = 255 - density * (255 - maskValue) / 255;

                    const int alpha = qAlpha(scanLine[x]);
                    const int newAlpha = (alpha * maskValue) / 255;
                    scanLine[x] = qRgba(qRed(scanLine[x]), qGreen(scanLine[x]), qBlue(scanLine[x]), newAlpha);
                }
            }
        }

        if (useCustomBlend) {
            const qreal opacity = abstractLayer()->opacity() * abstractLayer()->fillOpacity();
            QImage *backbuffer = dynamic_cast<QImage *>(painter->device());
            if (backbuffer) {
                const QTransform xf = painter->combinedTransform();
                const QRect deviceRect = xf.mapRect(QRectF(QPointF(0, 0), QSizeF(tempImage.size()))).toAlignedRect();
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
            } else {
                painter->drawImage(-blurMargin, -blurMargin, tempImage);
            }
        } else {
            painter->drawImage(-blurMargin, -blurMargin, tempImage);
        }
    }
}

QT_END_NAMESPACE

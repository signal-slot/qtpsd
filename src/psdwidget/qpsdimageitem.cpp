// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "qpsdimageitem.h"
#include <QtCore/QBuffer>
#include <QtCore/QDebug>
#include <QtCore/QtMath>
#include <QtGui/QImageReader>
#include <QtGui/QPainter>
#include <QtPsdCore/QPsdOglwEffect>
#include <QtPsdCore/QPsdSofiEffect>
#include <QtPsdCore/QPsdShadowEffect>
#include <QtPsdGui/QPsdBorder>

QT_BEGIN_NAMESPACE

static void boxBlurAlpha(QImage &img, int radius)
{
    if (radius <= 0 || img.isNull())
        return;
    img = img.convertToFormat(QImage::Format_ARGB32);
    const int w = img.width();
    const int h = img.height();
    const int size = 2 * radius + 1;

    // Horizontal pass
    QImage temp(w, h, QImage::Format_ARGB32);
    temp.fill(Qt::transparent);
    for (int y = 0; y < h; ++y) {
        const QRgb *src = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        QRgb *dst = reinterpret_cast<QRgb *>(temp.scanLine(y));
        int sum = 0;
        for (int x = -radius; x <= radius; ++x)
            sum += qAlpha(src[qBound(0, x, w - 1)]);
        for (int x = 0; x < w; ++x) {
            const int a = qBound(0, sum / size, 255);
            const QRgb s = src[x];
            dst[x] = qRgba(qRed(s), qGreen(s), qBlue(s), a);
            sum -= qAlpha(src[qBound(0, x - radius, w - 1)]);
            sum += qAlpha(src[qBound(0, x + radius + 1, w - 1)]);
        }
    }
    // Vertical pass
    for (int x = 0; x < w; ++x) {
        int sum = 0;
        for (int y = -radius; y <= radius; ++y)
            sum += qAlpha(reinterpret_cast<const QRgb *>(temp.constScanLine(qBound(0, y, h - 1)))[x]);
        for (int y = 0; y < h; ++y) {
            const int a = qBound(0, sum / size, 255);
            const QRgb s = reinterpret_cast<const QRgb *>(temp.constScanLine(y))[x];
            reinterpret_cast<QRgb *>(img.scanLine(y))[x] = qRgba(qRed(s), qGreen(s), qBlue(s), a);
            sum -= qAlpha(reinterpret_cast<const QRgb *>(temp.constScanLine(qBound(0, y - radius, h - 1)))[x]);
            sum += qAlpha(reinterpret_cast<const QRgb *>(temp.constScanLine(qBound(0, y + radius + 1, h - 1)))[x]);
        }
    }
}

// Dilate the alpha channel of an image using separable max filter
// Returns an image expanded by 'radius' pixels on each side
static QImage dilateAlpha(const QImage &source, int radius)
{
    const QImage img = source.convertToFormat(QImage::Format_ARGB32);
    const int srcW = img.width();
    const int srcH = img.height();
    const int outW = srcW + 2 * radius;
    const int outH = srcH + 2 * radius;

    // Extract alpha channel into a padded buffer
    QVector<uchar> alpha(outW * outH, 0);
    for (int y = 0; y < srcH; ++y) {
        const QRgb *scanLine = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < srcW; ++x) {
            alpha[(y + radius) * outW + (x + radius)] = qAlpha(scanLine[x]);
        }
    }

    // Horizontal max pass
    QVector<uchar> hMax(outW * outH, 0);
    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            uchar maxVal = 0;
            const int x0 = qMax(0, x - radius);
            const int x1 = qMin(outW - 1, x + radius);
            for (int dx = x0; dx <= x1; ++dx) {
                maxVal = qMax(maxVal, alpha[y * outW + dx]);
            }
            hMax[y * outW + x] = maxVal;
        }
    }

    // Vertical max pass
    QVector<uchar> dilated(outW * outH, 0);
    for (int x = 0; x < outW; ++x) {
        for (int y = 0; y < outH; ++y) {
            uchar maxVal = 0;
            const int y0 = qMax(0, y - radius);
            const int y1 = qMin(outH - 1, y + radius);
            for (int dy = y0; dy <= y1; ++dy) {
                maxVal = qMax(maxVal, hMax[dy * outW + x]);
            }
            dilated[y * outW + x] = maxVal;
        }
    }

    // Create result image with dilated alpha
    QImage result(outW, outH, QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    for (int y = 0; y < outH; ++y) {
        QRgb *scanLine = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < outW; ++x) {
            const uchar a = dilated[y * outW + x];
            if (a > 0) {
                scanLine[x] = qRgba(255, 255, 255, a);
            }
        }
    }

    return result;
}

QPsdImageItem::QPsdImageItem(const QModelIndex &index, const QPsdImageLayerItem *psdData, const QPsdAbstractLayerItem *maskItem, const QMap<quint32, QString> group, QGraphicsItem *parent)
    : QPsdAbstractItem(index, psdData, maskItem, group, parent)
{
}

void QPsdImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    
    setMask(painter);

    const auto *layer = this->layer<QPsdImageLayerItem>();
    QRect r = QRect{{0, 0}, layer->rect().size()};
    QImage image = layer->image();

    QImage linkedImage = layer->linkedImage();
    if (!linkedImage.isNull()) {
        image = linkedImage.scaled(r.width(), r.height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        r = QRect((r.width() - image.width()) / 2, (r.height() - image.height()) / 2, image.width(), image.height());
    }

    // Apply raster layer mask if present
    const QImage layerMask = layer->layerMask();
    if (!layerMask.isNull()) {
        const QRect maskRect = layer->layerMaskRect();
        const QRect layerRect = layer->rect();
        const int defaultColor = layer->layerMaskDefaultColor();  // 0=outside transparent, 255=outside opaque

        // Convert image to ARGB32 for alpha manipulation
        image = image.convertToFormat(QImage::Format_ARGB32);

        // Apply mask: multiply alpha by mask value
        for (int y = 0; y < image.height(); ++y) {
            QRgb *scanLine = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                // Calculate the corresponding position in the mask
                // Mask rect is in document coordinates, layer content is at (0,0) relative to layer rect
                const int maskX = (layerRect.x() + x) - maskRect.x();
                const int maskY = (layerRect.y() + y) - maskRect.y();

                int maskValue = defaultColor;  // Outside mask uses defaultColor
                if (maskX >= 0 && maskX < layerMask.width() &&
                    maskY >= 0 && maskY < layerMask.height()) {
                    // Get mask value (grayscale: 0=transparent, 255=opaque)
                    maskValue = qGray(layerMask.pixel(maskX, maskY));
                }

                // Multiply existing alpha by mask value
                const int alpha = qAlpha(scanLine[x]);
                const int newAlpha = (alpha * maskValue) / 255;
                scanLine[x] = qRgba(qRed(scanLine[x]), qGreen(scanLine[x]), qBlue(scanLine[x]), newAlpha);
            }
        }
    }

    const auto effects = layer->effects();
    
    // First pass: Draw drop shadows (they go behind the layer)
    for (const auto &effect : effects) {
        if (effect.canConvert<QPsdShadowEffect>()) {
            const auto dropShadow = effect.value<QPsdShadowEffect>();
            
            // Get shadow parameters (values are in 16.16 fixed-point format)
            const qreal angleDeg = dropShadow.angle() / 65536.0;
            const qreal distance = dropShadow.distance() / 65536.0;
            const qreal blur = dropShadow.blur() / 65536.0;
            const auto color = QColor(dropShadow.nativeColor());
            const auto opacity = dropShadow.opacity();

            // Calculate offset from angle and distance
            // PSD angles: 0° is right, 90° is up, 180° is left, 270° is down
            const qreal angleRad = qDegreesToRadians(angleDeg);
            const QPointF offset(
                qCos(angleRad) * distance,
                -qSin(angleRad) * distance  // Negate for PSD coordinate system
            );
            
            // Create shadow from the original image
            QImage shadowImage = image.convertToFormat(QImage::Format_ARGB32);
            
            // Fill shadow with shadow color while preserving alpha
            for (int y = 0; y < shadowImage.height(); ++y) {
                QRgb *scanLine = reinterpret_cast<QRgb *>(shadowImage.scanLine(y));
                for (int x = 0; x < shadowImage.width(); ++x) {
                    const int alpha = qAlpha(scanLine[x]);
                    if (alpha > 0) {
                        scanLine[x] = qRgba(color.red(), color.green(), color.blue(), alpha);
                    }
                }
            }
            
            // Apply blur (3-pass box blur approximates gaussian)
            if (blur > 0) {
                const int blurRadius = qMax(1, static_cast<int>(blur));
                // Expand the image to accommodate blur spread
                const int margin = blurRadius * 2;
                QImage expanded(shadowImage.width() + margin * 2,
                                shadowImage.height() + margin * 2,
                                QImage::Format_ARGB32);
                expanded.fill(Qt::transparent);
                QPainter ep(&expanded);
                ep.drawImage(margin, margin, shadowImage);
                ep.end();
                // 3-pass box blur ≈ gaussian
                boxBlurAlpha(expanded, blurRadius);
                boxBlurAlpha(expanded, blurRadius);
                boxBlurAlpha(expanded, blurRadius);
                shadowImage = expanded;
                // Adjust draw position for the expanded margin
                painter->save();
                painter->setOpacity(layer->opacity() * opacity);
                painter->drawImage(r.topLeft() + offset.toPoint() - QPoint(margin, margin), shadowImage);
                painter->restore();
            } else {
                painter->save();
                painter->setOpacity(layer->opacity() * opacity);
                painter->drawImage(r.translated(offset.toPoint()), shadowImage);
                painter->restore();
            }
        }
    }
    
    // 1.5 pass: Draw outer glow behind the layer but in front of drop shadow
    for (const auto &effect : effects) {
        if (effect.canConvert<QPsdOglwEffect>() && !effect.canConvert<QPsdShadowEffect>()) {
            const auto glow = effect.value<QPsdOglwEffect>();
            const qreal glowBlur = glow.blur() / 65536.0;  // 16.16 fixed-point
            const auto glowColor = QColor(glow.nativeColor());
            const auto glowOpacity = glow.opacity();

            if (glowBlur > 0) {
                // Create glow from the original image alpha
                const int blurRadius = qMax(1, static_cast<int>(glowBlur));
                const int margin = blurRadius * 2;
                QImage glowImage(image.width() + margin * 2,
                                 image.height() + margin * 2,
                                 QImage::Format_ARGB32);
                glowImage.fill(Qt::transparent);

                // Extract alpha from original and fill with glow color
                QImage alphaSource = image.convertToFormat(QImage::Format_ARGB32);
                for (int y = 0; y < alphaSource.height(); ++y) {
                    QRgb *scanLine = reinterpret_cast<QRgb *>(alphaSource.scanLine(y));
                    for (int x = 0; x < alphaSource.width(); ++x) {
                        const int alpha = qAlpha(scanLine[x]);
                        scanLine[x] = qRgba(glowColor.red(), glowColor.green(), glowColor.blue(), alpha);
                    }
                }
                QPainter gp(&glowImage);
                gp.drawImage(margin, margin, alphaSource);
                gp.end();

                // Blur to create glow spread
                boxBlurAlpha(glowImage, blurRadius);
                boxBlurAlpha(glowImage, blurRadius);
                boxBlurAlpha(glowImage, blurRadius);

                painter->save();
                painter->setCompositionMode(QtPsdGui::compositionMode(glow.blendMode()));
                painter->setOpacity(layer->opacity() * glowOpacity);
                painter->drawImage(r.topLeft() - QPoint(margin, margin), glowImage);
                painter->restore();
            }
        }
    }

    // Border/stroke effect (FrFX)
    const auto *border = layer->border();
    if (border && border->isEnable()) {
        const int strokeSize = border->size();
        const QColor strokeColor = border->color();
        const qreal strokeOpacity = border->opacity();
        const auto position = border->position();

        if (position == QPsdBorder::Outer || position == QPsdBorder::Center) {
            const int effectiveSize = (position == QPsdBorder::Center) ? (strokeSize + 1) / 2 : strokeSize;
            QImage dilated = dilateAlpha(image, effectiveSize);

            // Fill dilated alpha with stroke color
            for (int y = 0; y < dilated.height(); ++y) {
                QRgb *scanLine = reinterpret_cast<QRgb *>(dilated.scanLine(y));
                for (int x = 0; x < dilated.width(); ++x) {
                    const int a = qAlpha(scanLine[x]);
                    if (a > 0) {
                        scanLine[x] = qRgba(strokeColor.red(), strokeColor.green(), strokeColor.blue(), a);
                    }
                }
            }

            // Draw dilated shape behind the layer image
            // The original image drawn on top will naturally cover the interior,
            // leaving only the outer stroke band visible
            painter->save();
            painter->setOpacity(layer->opacity() * strokeOpacity);
            painter->drawImage(r.topLeft() - QPoint(effectiveSize, effectiveSize), dilated);
            painter->restore();
        }
    }

    // Second pass: Apply other effects to the image
    for (const auto &effect : effects) {
        if (effect.canConvert<QPsdSofiEffect>() && !effect.canConvert<QPsdOglwEffect>()) {
            const auto sofi = effect.value<QPsdSofiEffect>();
            QColor color(sofi.nativeColor());
            color.setAlphaF(sofi.opacity());
            switch (sofi.blendMode()) {
            case QPsdBlend::Mode::Normal: {
                // override pixels in the image with the color and opacity
                QPainter p(&image);
                p.setCompositionMode(QPainter::CompositionMode_SourceIn);
                p.fillRect(image.rect(), color);
                p.end();
                break; }
            default:
                qWarning() << sofi.blendMode() << "not supported blend mode";
                break;
            }
        }
    }

    const auto compositionMode = groupCompositionMode() != QPainter::CompositionMode_SourceOver
        ? groupCompositionMode()
        : QtPsdGui::compositionMode(layer->record().blendMode());
    painter->setCompositionMode(compositionMode);
    // Apply both opacity and fill opacity to the layer content
    // In Photoshop: opacity affects everything, fill opacity affects only layer pixels (not effects)
    // Effects (drop shadow etc.) were already drawn above with just opacity
    painter->setOpacity(layer->opacity() * layer->fillOpacity());
    // Finally, draw the layer itself
    painter->drawImage(r, image);

    // Border/stroke effect (FrFX) - inner strokes go on top of the layer
    if (border && border->isEnable() && border->position() == QPsdBorder::Inner) {
        const int strokeSize = border->size();
        const QColor strokeColor = border->color();
        const qreal strokeOpacity = border->opacity();

        // Dilate the image, then subtract the dilated version from the original
        // to find boundary pixels, then restrict to original alpha
        QImage dilated = dilateAlpha(image, strokeSize);

        // Fill dilated with stroke color
        for (int y = 0; y < dilated.height(); ++y) {
            QRgb *scanLine = reinterpret_cast<QRgb *>(dilated.scanLine(y));
            for (int x = 0; x < dilated.width(); ++x) {
                const int a = qAlpha(scanLine[x]);
                if (a > 0) {
                    scanLine[x] = qRgba(strokeColor.red(), strokeColor.green(), strokeColor.blue(), a);
                }
            }
        }

        // Create stroke mask: original alpha minus eroded alpha
        // Eroding = inverting alpha, dilating, inverting back
        // Simpler: create a stroke image by XOR-ing dilated with original
        QImage strokeImage(image.size(), QImage::Format_ARGB32);
        strokeImage.fill(Qt::transparent);
        {
            QPainter sp(&strokeImage);
            // Fill with stroke color using original alpha
            sp.drawImage(0, 0, image);
            sp.setCompositionMode(QPainter::CompositionMode_SourceIn);
            sp.fillRect(strokeImage.rect(), strokeColor);
            sp.end();
        }

        // Erase the interior (pixels that are far from the boundary)
        // We need to erode the alpha mask by strokeSize pixels
        // Erosion = complement of dilation of complement
        QImage eroded(image.size(), QImage::Format_ARGB32);
        eroded.fill(Qt::transparent);
        {
            // Create inverted alpha
            QImage invAlpha(image.size(), QImage::Format_ARGB32);
            invAlpha.fill(Qt::transparent);
            QImage srcImg = image.convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < srcImg.height(); ++y) {
                const QRgb *srcLine = reinterpret_cast<const QRgb *>(srcImg.constScanLine(y));
                QRgb *dstLine = reinterpret_cast<QRgb *>(invAlpha.scanLine(y));
                for (int x = 0; x < srcImg.width(); ++x) {
                    dstLine[x] = qRgba(255, 255, 255, 255 - qAlpha(srcLine[x]));
                }
            }

            // Dilate the inverted alpha
            QImage dilatedInv = dilateAlpha(invAlpha, strokeSize);

            // Invert back to get eroded alpha, extract the center region
            for (int y = 0; y < image.height(); ++y) {
                QRgb *erodedLine = reinterpret_cast<QRgb *>(eroded.scanLine(y));
                const QRgb *dilInvLine = reinterpret_cast<const QRgb *>(dilatedInv.constScanLine(y + strokeSize));
                for (int x = 0; x < image.width(); ++x) {
                    const int a = 255 - qAlpha(dilInvLine[x + strokeSize]);
                    erodedLine[x] = qRgba(255, 255, 255, qMax(0, a));
                }
            }
        }

        // Stroke region = original alpha - eroded alpha
        // Remove the eroded interior from the stroke
        {
            QPainter sp(&strokeImage);
            sp.setCompositionMode(QPainter::CompositionMode_DestinationOut);
            sp.drawImage(0, 0, eroded);
            sp.end();
        }

        painter->save();
        painter->setOpacity(layer->opacity() * strokeOpacity);
        painter->drawImage(r, strokeImage);
        painter->restore();
    }

    if (layer->gradient()) {
        QImage gradientImage = layer->applyGradient(image);
        painter->setOpacity(layer->opacity());
        painter->drawImage(r.topLeft(), gradientImage);
    }
}

QT_END_NAMESPACE

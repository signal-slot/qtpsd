// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef QPSDCUSTOMBLEND_P_H
#define QPSDCUSTOMBLEND_P_H

#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QPainter>
#include <QtPsdGui/QPsdAbstractLayerItem>

QT_BEGIN_NAMESPACE

// --- Inner layer effects (satin, inner glow) helpers -----------------------

// Two-pass chamfer (3-4) distance from the nearest outside pixel, in pixels.
// `alphaAt` supplies coverage 0-255 for a pixel.
template<typename AlphaFn>
inline QVector<float> psdDistanceFromEdge(int w, int h, AlphaFn alphaAt)
{
    const int INF = 1 << 29;
    QVector<int> dist(w * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            dist[y * w + x] = alphaAt(x, y) >= 128 ? INF : 0;
    auto relax = [&](int idx, int nIdx, int cost) {
        if (dist[nIdx] + cost < dist[idx])
            dist[idx] = dist[nIdx] + cost;
    };
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int i = y * w + x;
            if (x > 0) relax(i, i - 1, 3);
            if (y > 0) relax(i, i - w, 3);
            if (x > 0 && y > 0) relax(i, i - w - 1, 4);
            if (x < w - 1 && y > 0) relax(i, i - w + 1, 4);
            // Border pixels count as adjacent to outside
            if (x == 0 || y == 0) dist[i] = qMin(dist[i], 3);
        }
    }
    for (int y = h - 1; y >= 0; --y) {
        for (int x = w - 1; x >= 0; --x) {
            const int i = y * w + x;
            if (x < w - 1) relax(i, i + 1, 3);
            if (y < h - 1) relax(i, i + w, 3);
            if (x < w - 1 && y < h - 1) relax(i, i + w + 1, 4);
            if (x > 0 && y < h - 1) relax(i, i + w - 1, 4);
            if (x == w - 1 || y == h - 1) dist[i] = qMin(dist[i], 3);
        }
    }
    QVector<float> out(w * h);
    for (int i = 0; i < w * h; ++i)
        out[i] = dist[i] / 3.0f;
    return out;
}

// Separable box blur over a float field, two passes
inline void psdBoxBlurField(QVector<float> &field, int w, int h, int radius)
{
    if (radius < 1)
        return;
    for (int pass = 0; pass < 2; ++pass) {
        QVector<float> tmp(w * h, 0.0f);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float sum = 0; int n = 0;
                for (int k = -radius; k <= radius; ++k) {
                    const int nx = x + k;
                    if (nx >= 0 && nx < w) { sum += field[y * w + nx]; ++n; }
                }
                tmp[y * w + x] = sum / n;
            }
        }
        for (int x = 0; x < w; ++x) {
            for (int y = 0; y < h; ++y) {
                float sum = 0; int n = 0;
                for (int k = -radius; k <= radius; ++k) {
                    const int ny = y + k;
                    if (ny >= 0 && ny < h) { sum += tmp[ny * w + x]; ++n; }
                }
                field[y * w + x] = sum / n;
            }
        }
    }
}

// Build the ARGB image of an effect: `color` with alpha = intensity * opacity
// * coverage, ready to blend over the layer content
template<typename AlphaFn>
inline QImage psdEffectColorImage(int w, int h, const QColor &color,
                                  const QVector<float> &intensity, qreal opacity,
                                  AlphaFn alphaAt)
{
    QImage fx(w, h, QImage::Format_ARGB32);
    for (int y = 0; y < h; ++y) {
        QRgb *out = reinterpret_cast<QRgb *>(fx.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const qreal a = qBound<qreal>(0.0, intensity[y * w + x], 1.0)
                * opacity * (alphaAt(x, y) / 255.0);
            out[x] = qRgba(color.red(), color.green(), color.blue(), qRound(a * 255.0));
        }
    }
    return fx;
}

// Satin fold field: difference of the coverage shifted by +/- the light
// offset, blurred by size
template<typename AlphaFn>
inline QVector<float> psdSatinField(int w, int h, qreal angleDeg, qreal distance,
                                    qreal size, bool invert, AlphaFn alphaAt)
{
    const qreal angle = qDegreesToRadians(angleDeg);
    const int dx = qRound(qCos(angle) * distance);
    const int dy = -qRound(qSin(angle) * distance);
    auto safeAlpha = [&](int x, int y) -> int {
        if (x < 0 || x >= w || y < 0 || y >= h)
            return 0;
        return alphaAt(x, y);
    };
    QVector<float> field(w * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            field[y * w + x] = qAbs(safeAlpha(x + dx, y + dy) - safeAlpha(x - dx, y - dy)) / 255.0f;
    psdBoxBlurField(field, w, h, qMax(1, qRound(size / 2.0)));
    if (invert) {
        for (auto &v : field)
            v = 1.0f - v;
    }
    return field;
}

// Inner glow intensity field from a distance-from-edge map
inline QVector<float> psdInnerGlowField(const QVector<float> &dist, qreal size, bool center)
{
    QVector<float> field(dist.size());
    const qreal s = qMax(1.0, size);
    for (int i = 0; i < dist.size(); ++i) {
        const float t = float(dist[i] / s);
        field[i] = center ? t : 1.0f - t;
    }
    return field;
}

// Perform custom per-pixel blending of `src` against the current painter backbuffer.
// Works on both raster (QImage) and OpenGL paint devices.
inline void drawCustomBlended(QPainter *painter, const QImage &src,
                              const QRectF &srcRect,
                              QPsdBlend::Mode blendMode, qreal opacity)
{
    const QTransform xf = painter->combinedTransform();
    const QRect deviceRect = xf.mapRect(srcRect).toAlignedRect();

    // Fast path: QImage paint device — direct pixel access
    QImage *backbuffer = dynamic_cast<QImage *>(painter->device());
    if (backbuffer) {
        const QRect clipped = deviceRect.intersected(backbuffer->rect());
        if (clipped.isEmpty()) return;
        QImage destRegion = backbuffer->copy(clipped)
                                .convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QImage srcRegion = src.copy(
            clipped.x() - deviceRect.x(), clipped.y() - deviceRect.y(),
            clipped.width(), clipped.height()
        ).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QtPsdGui::customBlend(destRegion, srcRegion, blendMode, opacity);
        painter->save();
        painter->resetTransform();
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->setOpacity(1.0);
        painter->drawImage(clipped.topLeft(), destRegion);
        painter->restore();
        return;
    }

    // QPixmap paint device — e.g. a QGraphicsEffect source being rendered
    // inside an enclosing effect's subtree rasterization
    QPixmap *pixmapDevice = dynamic_cast<QPixmap *>(painter->device());
    if (pixmapDevice) {
        const QRect clipped = deviceRect.intersected(pixmapDevice->rect());
        if (clipped.isEmpty()) return;
        QImage destRegion = pixmapDevice->copy(clipped).toImage()
                                .convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QImage srcRegion = src.copy(
            clipped.x() - deviceRect.x(), clipped.y() - deviceRect.y(),
            clipped.width(), clipped.height()
        ).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QtPsdGui::customBlend(destRegion, srcRegion, blendMode, opacity);
        painter->save();
        painter->resetTransform();
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->setOpacity(1.0);
        painter->drawImage(clipped.topLeft(), destRegion);
        painter->restore();
        return;
    }

    // OpenGL path: read back framebuffer via glReadPixels
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (ctx) {
        const QSize devSize(painter->device()->width(), painter->device()->height());
        const QRect clipped = deviceRect.intersected(QRect(QPoint(0, 0), devSize));
        if (clipped.isEmpty()) return;

        // Flush pending draw commands and read framebuffer
        painter->beginNativePainting();
        QOpenGLFunctions *f = ctx->functions();
        QImage readback(clipped.width(), clipped.height(), QImage::Format_RGBA8888);
        f->glReadPixels(clipped.x(),
                        devSize.height() - clipped.y() - clipped.height(),
                        clipped.width(), clipped.height(),
                        GL_RGBA, GL_UNSIGNED_BYTE, readback.bits());
        painter->endNativePainting();

        // OpenGL Y-axis is inverted
        readback = readback.mirrored(false, true);
        QImage destRegion = readback.convertToFormat(QImage::Format_ARGB32_Premultiplied);

        QImage srcRegion = src.copy(
            clipped.x() - deviceRect.x(), clipped.y() - deviceRect.y(),
            clipped.width(), clipped.height()
        ).convertToFormat(QImage::Format_ARGB32_Premultiplied);

        QtPsdGui::customBlend(destRegion, srcRegion, blendMode, opacity);

        painter->save();
        painter->resetTransform();
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->setOpacity(1.0);
        painter->drawImage(clipped.topLeft(), destRegion);
        painter->restore();
        return;
    }

    // Final fallback: draw with SourceOver (incorrect but better than nothing)
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter->setOpacity(opacity);
    painter->drawImage(srcRect.toRect(), src);
}

QT_END_NAMESPACE

#endif // QPSDCUSTOMBLEND_P_H

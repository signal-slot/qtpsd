// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef QPSDCUSTOMBLEND_P_H
#define QPSDCUSTOMBLEND_P_H

#include <QtGui/QImage>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QPainter>
#include <QtPsdGui/QPsdAbstractLayerItem>

QT_BEGIN_NAMESPACE

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

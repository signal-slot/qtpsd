// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef QPSDBLUR_P_H
#define QPSDBLUR_P_H

#include <QtGui/QImage>

QT_BEGIN_NAMESPACE

inline void boxBlurAlpha(QImage &img, int radius)
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

QT_END_NAMESPACE

#endif // QPSDBLUR_P_H

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

// Separable Gaussian blur on all RGBA channels
inline void gaussianBlurRgba(QImage &img, qreal sigma)
{
    if (sigma <= 0 || img.isNull())
        return;
    img = img.convertToFormat(QImage::Format_ARGB32);
    const int w = img.width();
    const int h = img.height();

    // Build 1D Gaussian kernel (truncate at 3*sigma)
    const int radius = qMax(1, static_cast<int>(std::ceil(sigma * 3.0)));
    const int ksize = 2 * radius + 1;
    QVector<qreal> kernel(ksize);
    qreal sum = 0;
    for (int i = 0; i < ksize; ++i) {
        const qreal x = i - radius;
        kernel[i] = std::exp(-x * x / (2.0 * sigma * sigma));
        sum += kernel[i];
    }
    for (int i = 0; i < ksize; ++i)
        kernel[i] /= sum;

    QImage temp(w, h, QImage::Format_ARGB32);
    temp.fill(Qt::transparent);

    // Horizontal pass
    for (int y = 0; y < h; ++y) {
        const QRgb *src = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        QRgb *dst = reinterpret_cast<QRgb *>(temp.scanLine(y));
        for (int x = 0; x < w; ++x) {
            qreal r = 0, g = 0, b = 0, a = 0;
            for (int k = -radius; k <= radius; ++k) {
                const QRgb px = src[qBound(0, x + k, w - 1)];
                const qreal weight = kernel[k + radius];
                r += qRed(px) * weight;
                g += qGreen(px) * weight;
                b += qBlue(px) * weight;
                a += qAlpha(px) * weight;
            }
            dst[x] = qRgba(qBound(0, qRound(r), 255), qBound(0, qRound(g), 255),
                            qBound(0, qRound(b), 255), qBound(0, qRound(a), 255));
        }
    }
    // Vertical pass
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
            qreal r = 0, g = 0, b = 0, a = 0;
            for (int k = -radius; k <= radius; ++k) {
                const QRgb px = reinterpret_cast<const QRgb *>(temp.constScanLine(qBound(0, y + k, h - 1)))[x];
                const qreal weight = kernel[k + radius];
                r += qRed(px) * weight;
                g += qGreen(px) * weight;
                b += qBlue(px) * weight;
                a += qAlpha(px) * weight;
            }
            reinterpret_cast<QRgb *>(img.scanLine(y))[x] = qRgba(
                qBound(0, qRound(r), 255), qBound(0, qRound(g), 255),
                qBound(0, qRound(b), 255), qBound(0, qRound(a), 255));
        }
    }
}

QT_END_NAMESPACE

#endif // QPSDBLUR_P_H

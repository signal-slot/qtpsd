// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDADJUSTMENTS_H
#define QPSDADJUSTMENTS_H

#include <QtCore/QVariantMap>
#include <QtGui/QImage>
#include <QtGui/QRgb>
#include <QtPsdGui/qpsdguiglobal.h>

QT_BEGIN_NAMESPACE

class QPsdAdjustmentLayerItem;

namespace QtPsdGui {

// Applies the Photoshop adjustment described by `layer` to `image`
// (ARGB32, straight alpha), weighted per pixel by `weightMask`
// (Grayscale8 in the same coordinate space; null = full weight) and the
// layer opacity. Returns false when the adjustment type is not
// implemented, leaving the image untouched.
Q_PSDGUI_EXPORT bool applyAdjustmentToImage(QImage &image,
                                            const QPsdAdjustmentLayerItem *layer,
                                            const QImage &weightMask = QImage());

// Builds the per-pixel weight of an adjustment layer in document
// coordinates (Grayscale8): its raster mask faded by mask density,
// multiplied by the clipping-base layer's coverage when the adjustment is
// clipped. Returns a null image when the weight is 1 everywhere. Layer
// opacity is NOT included.
Q_PSDGUI_EXPORT QImage adjustmentWeightMask(const QPsdAdjustmentLayerItem *layer,
                                            const QPsdAbstractLayerItem *clipBase,
                                            const QSize &canvasSize);

// Builds the 256-entry gradient-map lookup table from grdm data
// (colorStops + method). Method "Smoo"/"Perc" interpolates in Oklab,
// "Lnr " in linear light, anything else (classic "Gcls") in gamma sRGB.
Q_PSDGUI_EXPORT QList<QRgb> gradientMapLut(const QVariantMap &grdmData);

// Builds a 256-entry curve lookup table from curve points
// (list of {input, output} maps) using natural cubic spline interpolation,
// matching Photoshop's Curves adjustment. Empty input yields identity.
Q_PSDGUI_EXPORT QList<quint8> curveLut(const QVariantList &points);

}

QT_END_NAMESPACE

#endif // QPSDADJUSTMENTS_H

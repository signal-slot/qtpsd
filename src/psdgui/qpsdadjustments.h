// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDADJUSTMENTS_H
#define QPSDADJUSTMENTS_H

#include <QtGui/QImage>
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

}

QT_END_NAMESPACE

#endif // QPSDADJUSTMENTS_H

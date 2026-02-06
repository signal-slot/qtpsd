// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDGUIGLOBAL_H
#define QPSDGUIGLOBAL_H

#include <QtCore/qstring.h>
#include <QtCore/qglobal.h>
#include <QtCore/qdebug.h>
#include <QtGui/QPainter>
#include <QtGui/QColorSpace>
#include <QtPsdGui/qtpsdguiexports.h>
#include <QtPsdCore/QPsdAbstractImage>
#include <QtPsdCore/QPsdFileHeader>
#include <QtPsdCore/QPsdColorModeData>
#include <QtPsdCore/qpsdblend.h>

using namespace Qt::Literals::StringLiterals;

QT_BEGIN_NAMESPACE

namespace QtPsdGui {
Q_PSDGUI_EXPORT QImage imageDataToImage(const QPsdAbstractImage &imageData, const QPsdFileHeader &fileHeader, const QPsdColorModeData &colorModeData = QPsdColorModeData(), const QByteArray &iccProfile = QByteArray());
Q_PSDGUI_EXPORT QPainter::CompositionMode compositionMode(QPsdBlend::Mode psdBlendMode);
}

QT_END_NAMESPACE

#endif // QPSDGUIGLOBAL_H

// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDTEXTLAYERITEM_H
#define QPSDTEXTLAYERITEM_H

#include <QtPsdGui/qpsdabstractlayeritem.h>

#include <QtGui/QColor>
#include <QtGui/QFont>

QT_BEGIN_NAMESPACE

class Q_PSDGUI_EXPORT QPsdTextLayerItem : public QPsdAbstractLayerItem
{
public:
    struct Run {
        QString text;
        QFont font;
        QString originalFontName;  // Original font name from PSD (e.g., "MyriadPro-Bold")
        QColor color;
        Qt::Alignment alignment = Qt::AlignVCenter;
    };
    enum class TextType {
        PointText,
        ParagraphText
    };

    QPsdTextLayerItem(const QPsdLayerRecord &record);
    QPsdTextLayerItem();
    ~QPsdTextLayerItem() override;
    Type type() const override { return Text; }

    QList<Run> runs() const;
    QRectF bounds() const;
    QRectF fontAdjustedBounds() const;
    TextType textType() const;

    // Returns the text origin point (tx, ty from the transform)
    // For point text, this is the baseline anchor position
    QPointF textOrigin() const;

    // Set the current PSD path context for font resolution (thread-local)
    static void setCurrentPsdPath(const QString &path);
    static QString currentPsdPath();

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDTEXTLAYERITEM_H

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
        qreal lineHeight = -1;  // px; -1 = use QFontMetrics::height()

        // Character-level properties
        bool fauxBold = false;
        bool fauxItalic = false;
        bool underline = false;
        bool strikethrough = false;
        int fontCaps = 0;          // 0=none, 1=smallCaps, 2=allCaps
        qreal baselineShift = 0;   // scaled points; positive = up
        qreal horizontalScale = 1.0; // 1.0 = 100%
        qreal verticalScale = 1.0;   // 1.0 = 100%

        // Paragraph-level properties
        qreal firstLineIndent = 0;  // scaled points
        qreal startIndent = 0;      // scaled points
        qreal endIndent = 0;        // scaled points
        qreal spaceBefore = 0;      // scaled points
        qreal spaceAfter = 0;       // scaled points
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
    TextType textType() const;

    // Returns the text origin point (tx, ty from the transform)
    // For point text, this is the baseline anchor position
    QPointF textOrigin() const;

    // Returns the text frame rectangle in document coordinates
    // For ParagraphText, this is the box that defines wrapping area
    // For PointText, returns an empty rect
    QRectF textFrame() const;

    void setRuns(const QList<Run> &runs);
    void setBounds(const QRectF &bounds);
    void setTextType(TextType type);
    void setTextOrigin(const QPointF &origin);
    void setTextFrame(const QRectF &frame);

    // Set the current PSD path context for font resolution (thread-local)
    static void setCurrentPsdPath(const QString &path);
    static QString currentPsdPath();

private:
    class Private;
    QScopedPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDTEXTLAYERITEM_H

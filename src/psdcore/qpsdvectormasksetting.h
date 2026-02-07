// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDVECTORMASKSETTING_H
#define QPSDVECTORMASKSETTING_H

#include <QtPsdCore/qpsdsection.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdVectorMaskSetting : public QPsdSection
{
public:
    enum FillRule {
        Same,
        EvenOdd,
        NonZero,
    };
    QPsdVectorMaskSetting();
    QPsdVectorMaskSetting(QIODevice *source, quint32 length);
    QPsdVectorMaskSetting(const QPsdVectorMaskSetting &other);
    QPsdVectorMaskSetting &operator=(const QPsdVectorMaskSetting &other);
    ~QPsdVectorMaskSetting() override;

    bool invert() const;
    bool notLink() const;
    bool disable() const;

    enum Type {
        Unknown,
        Open,
        Closed,
    };
    Type type() const;
    struct BezierPath {
        enum Type { Empty, Linked, Unlinked };
        Type type = Empty;
        QPointF preceding;
        QPointF anchor;
        QPointF leaving;
    };
    struct PathInfo {
        enum Operation {
            Xor,   // Difference
            Or,    // Union
            NotOr, // Subtract
            And,   // Intersect
        };
        Operation operation;
        FillRule fillRule = EvenOdd;
        enum Type {
            Rectangle,
            RoundedRectangle,
            Circle,
            Complex,
        };
        Type type = Complex;

        // TODO: union-ize
        QList<BezierPath> subPath;
        QRectF rect;
        qreal radius = 0;
    };
    QList<PathInfo> subPathList() const;

    QRectF clipboardRect() const;
    qreal clipboardResolution() const;
    FillRule initialFill() const;
    FillRule fillRule() const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDVECTORMASKSETTING_H

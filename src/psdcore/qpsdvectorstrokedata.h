// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDVECTORSTROKEDATA_H
#define QPSDVECTORSTROKEDATA_H

#include <QtPsdCore/qpsdsection.h>
#include <QtPsdCore/qpsdblend.h>
#include <QtPsdCore/qpsdenum.h>
#include <QtPsdCore/qpsdunitfloat.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdVectorStrokeData : public QPsdSection
{
public:
    QPsdVectorStrokeData();
    QPsdVectorStrokeData(QIODevice *source, quint32 length);
    QPsdVectorStrokeData(const QPsdVectorStrokeData &other);
    QPsdVectorStrokeData &operator=(const QPsdVectorStrokeData &other);
    ~QPsdVectorStrokeData() override;

    bool fillEnabled() const;
    bool strokeEnabled() const;
    QPsdBlend::Mode strokeStyleBlendMode() const;
    QString strokeStyleContent() const;
    QPsdEnum strokeStyleLineAlignment() const;
    QPsdEnum strokeStyleLineCapType() const;
    QPsdUnitFloat strokeStyleLineDashOffset() const;
    QList<qreal> strokeStyleLineDashSet() const;
    QPsdEnum strokeStyleLineJoinType() const;
    QPsdUnitFloat strokeStyleLineWidth() const;
    double strokeStyleMiterLimit() const;
    QPsdUnitFloat strokeStyleOpacity() const;
    double strokeStyleResolution() const;
    bool strokeStyleScaleLock() const;
    bool strokeStyleStrokeAdjust() const;
    int strokeStyleVersion() const;

    void write(QIODevice *dest) const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDVECTORSTROKEDATA_H

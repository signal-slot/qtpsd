// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDVECTORSTROKECONTENTSETTING_H
#define QPSDVECTORSTROKECONTENTSETTING_H

#include <QtPsdCore/qpsdsection.h>
#include <QtPsdCore/qpsdblend.h>
#include <QtPsdCore/qpsdenum.h>
#include <QtPsdCore/qpsdunitfloat.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdVectorStrokeContentSetting : public QPsdSection
{
public:
    QPsdVectorStrokeContentSetting();
    QPsdVectorStrokeContentSetting(QIODevice *source, quint32 length);
    QPsdVectorStrokeContentSetting(const QPsdVectorStrokeContentSetting &other);
    QPsdVectorStrokeContentSetting &operator=(const QPsdVectorStrokeContentSetting &other);
    ~QPsdVectorStrokeContentSetting() override;

    enum Type {
        SolidColor,
        GradientFill,
        PatternFill,
    };

    Type type() const;
    QString solidColor() const;
    enum GradientType {
        Linear,
        Radial,
        Angle,
        Reflected,
        Diamond,
    };
    QList<QPair<qreal, qreal>> opacities() const;
    QList<QPair<qreal, QString>> colors() const;
    GradientType gradientType() const;
    qreal angle() const;
    bool isDither() const;

    QString patternId() const;
    QString patternName() const;
    qreal patternScale() const;
    qreal patternOpacity() const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

QT_END_NAMESPACE

#endif // QPSDVECTORSTROKECONTENTSETTING_H

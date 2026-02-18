// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QPSDUNITFLOAT_H
#define QPSDUNITFLOAT_H

#include <QtPsdCore/qpsdsection.h>

QT_BEGIN_NAMESPACE

class Q_PSDCORE_EXPORT QPsdUnitFloat : public QPsdSection
{
public:
    enum Unit {
        Points,
        MilliMeters,
        Angle,
        Density,
        Distance,
        None,
        Percent,
        Pixels,
    };
    QPsdUnitFloat();
    QPsdUnitFloat(QIODevice *source , quint32 *length);
    QPsdUnitFloat(const QPsdUnitFloat &other);
    QPsdUnitFloat(const QByteArray &unit, double value);
    QPsdUnitFloat &operator=(const QPsdUnitFloat &other);
    QT_MOVE_ASSIGNMENT_OPERATOR_IMPL_VIA_PURE_SWAP(QPsdUnitFloat)
    void swap(QPsdUnitFloat &other) noexcept { d.swap(other.d); }
    virtual ~QPsdUnitFloat();

    Unit unit() const;
    double value() const;

    void write(QIODevice *dest) const;
    static QByteArray unitToTag(Unit unit);

private:
    class Private;
    QSharedDataPointer<Private> d;
};

Q_PSDCORE_EXPORT QDebug operator<<(QDebug s, const QPsdUnitFloat &value);

QT_END_NAMESPACE

Q_DECLARE_SHARED(QPsdUnitFloat)

#endif // QPSDUNITFLOAT_H

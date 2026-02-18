// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdunitfloat.h"

QT_BEGIN_NAMESPACE

class QPsdUnitFloat::Private : public QSharedData
{
public:
    Unit unit = None;
    double value = 0;

    void init(const QByteArray &unit, double value);
};

QPsdUnitFloat::QPsdUnitFloat()
    : d(new Private)
{}

void QPsdUnitFloat::Private::init(const QByteArray &unit, double value)
{
         if (unit == "#Pnt") this->unit = Points;
    else if (unit == "#Mlm") this->unit = MilliMeters;
    else if (unit == "#Ang") this->unit = Angle;
    else if (unit == "#Rsl") this->unit = Density;
    else if (unit == "#Rlt") this->unit = Distance;
    else if (unit == "#Nne") this->unit = None;
    else if (unit == "#Prc") this->unit = Percent;
    else if (unit == "#Pxl") this->unit = Pixels;
    else qWarning() << unit << "is not a valid unit";

    this->value = value;
}

QPsdUnitFloat::QPsdUnitFloat(QIODevice *source , quint32 *length)
    : d(new Private)
{
    const auto unit = readByteArray(source, 4, length);
    const auto value = readDouble(source, length);

    d->init(unit, value);
}

QPsdUnitFloat::QPsdUnitFloat(const QPsdUnitFloat &other)
    : d(other.d)
{}

QPsdUnitFloat::QPsdUnitFloat(const QByteArray &unit, double value) : d(new Private)
{
    d->init(unit, value);
}

QPsdUnitFloat &QPsdUnitFloat::operator=(const QPsdUnitFloat &other)
{
    if (this != &other)
        d.operator=(other.d);
    return *this;
}

QPsdUnitFloat::~QPsdUnitFloat() = default;

QPsdUnitFloat::Unit QPsdUnitFloat::unit() const
{
    return d->unit;
}

double QPsdUnitFloat::value() const
{
    return d->value;
}

QByteArray QPsdUnitFloat::unitToTag(Unit unit)
{
    switch (unit) {
    case Points: return "#Pnt"_ba;
    case MilliMeters: return "#Mlm"_ba;
    case Angle: return "#Ang"_ba;
    case Density: return "#Rsl"_ba;
    case Distance: return "#Rlt"_ba;
    case None: return "#Nne"_ba;
    case Percent: return "#Prc"_ba;
    case Pixels: return "#Pxl"_ba;
    }
    return "#Nne"_ba;
}

void QPsdUnitFloat::write(QIODevice *dest) const
{
    writeByteArray(dest, unitToTag(d->unit));
    writeDouble(dest, d->value);
}

QDebug operator<<(QDebug s, const QPsdUnitFloat &value)
{
    QDebugStateSaver saver(s);
    s.nospace() << "QPsdUnitFloat(" << value.value() << ", ";
    switch (value.unit()) {
#define CASE(x) case QPsdUnitFloat::x: s << #x; break
    CASE(Points);
    CASE(MilliMeters);
    CASE(Angle);
    CASE(Density);
    CASE(Distance);
    CASE(None);
    CASE(Percent);
    CASE(Pixels);
#undef CASE
    }
    s << ")";
    return s;
}

QT_END_NAMESPACE

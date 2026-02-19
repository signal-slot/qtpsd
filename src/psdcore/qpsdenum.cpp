// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdenum.h"

QT_BEGIN_NAMESPACE

class QPsdEnum::Private : public QSharedData
{
public:
    QByteArray type;
    QByteArray value;
};

QPsdEnum::QPsdEnum()
    : d(new Private)
{}

QPsdEnum::QPsdEnum(QIODevice *source , quint32 *length)
    : d(new Private)
{
    auto size = readS32(source, length);
    d->type = readByteArray(source, size == 0 ? 4 : size, length);
    size = readS32(source, length);
    d->value = readByteArray(source, size == 0 ? 4 : size, length);
}

QPsdEnum::QPsdEnum(const QPsdEnum &other)
    : d(other.d)
{}

QPsdEnum &QPsdEnum::operator=(const QPsdEnum &other)
{
    if (this != &other)
        d.operator=(other.d);
    return *this;
}

QPsdEnum::~QPsdEnum() = default;

QByteArray QPsdEnum::type() const
{
    return d->type;
}

void QPsdEnum::setType(const QByteArray &type)
{
    d->type = type;
}

QByteArray QPsdEnum::value() const
{
    return d->value;
}

void QPsdEnum::setValue(const QByteArray &value)
{
    d->value = value;
}

void QPsdEnum::write(QIODevice *dest) const
{
    // Write type: S32 length (0 means 4-byte key) + bytes
    writeS32(dest, d->type.size() == 4 ? 0 : d->type.size());
    writeByteArray(dest, d->type);
    // Write value: S32 length (0 means 4-byte key) + bytes
    writeS32(dest, d->value.size() == 4 ? 0 : d->value.size());
    writeByteArray(dest, d->value);
}

QDebug operator<<(QDebug s, const QPsdEnum &value)
{
    QDebugStateSaver saver(s);
    s.nospace() << "QPsdEnum(" << value.type() << ", " << value.value() << ")";
    return s;
}

QT_END_NAMESPACE

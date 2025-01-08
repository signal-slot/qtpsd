// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdimageresourceblock.h"

QT_BEGIN_NAMESPACE

class QPsdImageResourceBlock::Private : public QSharedData
{
public:
    Private();
    quint16 id;
    QByteArray name;
    QByteArray data;
};

QPsdImageResourceBlock::Private::Private()
    : id(0)
{}

QPsdImageResourceBlock::QPsdImageResourceBlock()
    : QPsdSection()
    , d(new Private)
{}

QPsdImageResourceBlock::QPsdImageResourceBlock(QIODevice *source)
    : QPsdImageResourceBlock()
{
    // Image Resource Blocks
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_46269

    // Signature: '8BIM'
    const auto signature = source->read(4);
    if (signature != "8BIM") {
        setErrorString("Signature mismatch"_L1);
        return;
    }

    // Unique identifier for the resource. Image resource IDs contains a list of resource IDs used by Photoshop.
    d->id = readU16(source);

    // Name: Pascal string, padded to make the size even (a null name consists of two bytes of 0)
    d->name = readPascalString(source, 2);

    // Length of image resource section. The length may be zero.
    // QByteArray size = source->read(4);
    // d->size = qFromBigEndian<quint32>(size.constData());
    auto length = readU32(source);
    EnsureSeek es(source, even(length));

    // The resource data, described in the sections on the individual resource types. It is padded to make the size even.
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_34945
    d->data = readByteArray(source, length); // TODO: parse data
}

QPsdImageResourceBlock::QPsdImageResourceBlock(const QPsdImageResourceBlock &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdImageResourceBlock &QPsdImageResourceBlock::operator=(const QPsdImageResourceBlock &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdImageResourceBlock::~QPsdImageResourceBlock() = default;


quint16 QPsdImageResourceBlock::id() const
{
    return d->id;
}

QByteArray QPsdImageResourceBlock::name() const
{
    return d->name;
}

QByteArray QPsdImageResourceBlock::data() const
{
    return d->data;
}

QT_END_NAMESPACE

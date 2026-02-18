// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdmetadataitem.h"

QT_BEGIN_NAMESPACE

class QPsdMetadataItem::Private : public QSharedData
{
public:
    QByteArray key;
    quint8 copyOnSheetDuplication = 0;
    QByteArray data;
};

QPsdMetadataItem::QPsdMetadataItem()
    : QPsdSection()
    , d(new Private)
{}

QPsdMetadataItem::QPsdMetadataItem(QIODevice *source, quint32 *length)
    : QPsdMetadataItem()
{
    // Signature of the data
    const auto signature = readByteArray(source, 4, length);
    Q_ASSERT(signature == "8BIM" || signature == "8ELE");
    // Key of the data
    d->key = readByteArray(source, 4, length);
    // Copy on sheet duplication
    d->copyOnSheetDuplication = readU8(source, length);
    // Padding
    skip(source, 3, length);
    // Length of data to follow
    auto len = readU32(source, length);
    // Undocumented data
    d->data = readByteArray(source, len, length);
}

QPsdMetadataItem::QPsdMetadataItem(const QPsdMetadataItem &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdMetadataItem &QPsdMetadataItem::operator=(const QPsdMetadataItem &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdMetadataItem::~QPsdMetadataItem() = default;

QByteArray QPsdMetadataItem::key() const
{
    return d->key;
}

quint8 QPsdMetadataItem::copyOnSheetDuplication() const
{
    return d->copyOnSheetDuplication;
}

QByteArray QPsdMetadataItem::data() const
{
    return d->data;
}

void QPsdMetadataItem::write(QIODevice *dest) const
{
    dest->write("8BIM", 4);
    writeByteArray(dest, d->key.leftJustified(4, '\0', true));
    writeU8(dest, d->copyOnSheetDuplication);
    dest->write(QByteArray(3, '\0'));
    writeU32(dest, d->data.size());
    dest->write(d->data);
}

QT_END_NAMESPACE

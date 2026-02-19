// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdsectiondividersetting.h"

QT_BEGIN_NAMESPACE

class QPsdSectionDividerSetting::Private : public QSharedData
{
public:
    Private();
    Type type = AnyOtherTypeOfLayer;
    QPsdBlend::Mode key = QPsdBlend::Invalid;
    SubType subType = Normal;
};

QPsdSectionDividerSetting::Private::Private()
{}

QPsdSectionDividerSetting::QPsdSectionDividerSetting()
    : QPsdSection()
    , d(new Private)
{}

QPsdSectionDividerSetting::QPsdSectionDividerSetting(QIODevice *source, quint32 *length)
    : QPsdSectionDividerSetting()
{
    // Type. 4 possible values, 0 = any other type of layer, 1 = open "folder", 2 = closed "folder", 3 = bounding section divider, hidden in the UI
    d->type = static_cast<Type>(readU32(source, length));
    if (*length >= 8) {
        // Signature: '8BIM'
        auto signature = readByteArray(source, 4, length);
        Q_ASSERT(signature == "8BIM");

        // Key. See blend mode keys in See Layer records.
        d->key = QPsdBlend::from(readByteArray(source, 4, length));
    }
    if (*length >= 4) {
        // Sub type. 0 = normal, 1 = scene group, affects the animation timeline.
        d->subType = static_cast<SubType>(readU32(source, length));
    }
}

QPsdSectionDividerSetting::QPsdSectionDividerSetting(const QPsdSectionDividerSetting &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdSectionDividerSetting &QPsdSectionDividerSetting::operator=(const QPsdSectionDividerSetting &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdSectionDividerSetting::~QPsdSectionDividerSetting() = default;

QPsdSectionDividerSetting::Type QPsdSectionDividerSetting::type() const
{
    return d->type;
}

void QPsdSectionDividerSetting::setType(Type type)
{
    d->type = type;
}

QPsdBlend::Mode QPsdSectionDividerSetting::key() const
{
    return d->key;
}

void QPsdSectionDividerSetting::setKey(QPsdBlend::Mode key)
{
    d->key = key;
}

QPsdSectionDividerSetting::SubType QPsdSectionDividerSetting::subType() const
{
    return d->subType;
}

void QPsdSectionDividerSetting::setSubType(SubType subType)
{
    d->subType = subType;
}

void QPsdSectionDividerSetting::write(QIODevice *dest) const
{
    writeU32(dest, static_cast<quint32>(d->type));
    if (d->key != QPsdBlend::Invalid) {
        dest->write("8BIM", 4);
        writeByteArray(dest, QPsdBlend::toKey(d->key).leftJustified(4, ' ', true));
        writeU32(dest, static_cast<quint32>(d->subType));
    }
}

QT_END_NAMESPACE

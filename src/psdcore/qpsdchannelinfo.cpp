// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdchannelinfo.h"

QT_BEGIN_NAMESPACE

class QPsdChannelInfo::Private : public QSharedData
{
public:
    Private();
    ChannelID id;
    quint32 length;
};

QPsdChannelInfo::Private::Private()
    : id(Red)
    , length(0)
{}

QPsdChannelInfo::QPsdChannelInfo()
    : QPsdSection()
    , d(new Private)
{}

QPsdChannelInfo::QPsdChannelInfo(QIODevice *source)
    : QPsdChannelInfo()
{
    // Channel information
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_13084

    // 2 bytes for Channel ID: 0 = red, 1 = green, etc.;
    // -1 = transparency mask; -2 = user supplied layer mask, -3 real user supplied layer mask (when both a user mask and a vector mask are present)
    d->id = static_cast<ChannelID>(readU16(source));

    // 4 bytes for length of corresponding channel data. (**PSB** 8 bytes for length of corresponding channel data.) See See Channel image data for structure of channel data.
    d->length = readU32(source);
}

QPsdChannelInfo::QPsdChannelInfo(const QPsdChannelInfo &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdChannelInfo &QPsdChannelInfo::operator=(const QPsdChannelInfo &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdChannelInfo::~QPsdChannelInfo() = default;

QPsdChannelInfo::ChannelID QPsdChannelInfo::id() const
{
    return d->id;
}

quint32 QPsdChannelInfo::length() const
{
    return d->length;
}

void QPsdChannelInfo::setId(ChannelID id)
{
    d->id = id;
}

void QPsdChannelInfo::setLength(quint32 length)
{
    d->length = length;
}

QT_END_NAMESPACE

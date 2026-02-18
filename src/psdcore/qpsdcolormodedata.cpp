// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdcolormodedata.h"

QT_BEGIN_NAMESPACE

class QPsdColorModeData::Private : public QSharedData
{
public:
    Private();
    QByteArray colorData;
};

QPsdColorModeData::Private::Private()
{}

QPsdColorModeData::QPsdColorModeData()
    : QPsdSection()
    , d(new Private)
{}

QPsdColorModeData::QPsdColorModeData(QIODevice *source)
    : QPsdColorModeData()
{
    // Color Mode Data Section
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_71638

    // The length of the following color data.
    auto length = readU32(source);
    EnsureSeek es(source, length);

    // The color data.
    if (length > 0)
        d->colorData = readByteArray(source, length);
}

QPsdColorModeData::QPsdColorModeData(const QPsdColorModeData &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdColorModeData &QPsdColorModeData::operator=(const QPsdColorModeData &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdColorModeData::~QPsdColorModeData() = default;

QByteArray QPsdColorModeData::colorData() const
{
    return d->colorData;
}

void QPsdColorModeData::setColorData(const QByteArray &colorData)
{
    d->colorData = colorData;
}

QT_END_NAMESPACE

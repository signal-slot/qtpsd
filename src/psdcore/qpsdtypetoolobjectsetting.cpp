// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdtypetoolobjectsetting.h"
#include <QtPsdCore/qpsdunitfloat.h>

QT_BEGIN_NAMESPACE

class QPsdTypeToolObjectSetting::Private : public QSharedData
{
public:
    QList<qreal> transform;
    QPsdDescriptor textData;
    QPsdDescriptor warpData;
    QRect rect;
    QRectF bounds;
};

QPsdTypeToolObjectSetting::QPsdTypeToolObjectSetting()
    : QPsdSection()
    , d(new Private)
{}

QPsdTypeToolObjectSetting::QPsdTypeToolObjectSetting(QIODevice *source, quint32 *length)
    : QPsdTypeToolObjectSetting()
{
    auto warnUnexpected = [](const char *field, quint32 actual, quint32 expected) {
        if (actual != expected) {
            qWarning("TypeToolObjectSetting: unexpected %s value %u (expected %u)",
                     field, actual, expected);
        }
    };

    // Version ( =1 for Photoshop 6.0)
    auto version = readU16(source, length);
    warnUnexpected("version", version, 1u);

    // Transform: xx, xy, yx, yy, tx, and ty respectively.
    auto xx = readDouble(source, length);
    auto xy = readDouble(source, length);
    auto yx = readDouble(source, length);
    auto yy = readDouble(source, length);
    auto tx = readDouble(source, length);
    auto ty = readDouble(source, length);
    d->transform = { xx, xy, yx, yy, tx, ty };
 
    // Text version ( = 50 for Photoshop 6.0)
    auto textVersion = readU16(source, length);
    warnUnexpected("textVersion", textVersion, 50u);

    // Descriptor version ( = 16 for Photoshop 6.0)
    auto descriptorVersion = readU32(source, length);
    warnUnexpected("textDescriptorVersion", descriptorVersion, 16u);

    // Text data (see See Descriptor structure)
    d->textData = QPsdDescriptor(source, length);

    const auto &data = d->textData.data();
    if (data.contains("bounds")) {
        const auto &bounds = data.value("bounds").value<QPsdDescriptor>();
        const auto l = bounds.data().value("Left").value<QPsdUnitFloat>().value();
        const auto t = bounds.data().value("Top ").value<QPsdUnitFloat>().value();
        const auto r = bounds.data().value("Rght").value<QPsdUnitFloat>().value();
        const auto b = bounds.data().value("Btom").value<QPsdUnitFloat>().value();

        const auto left = xx * l + xy * t;
        const auto top = yx * l + yy * t;
        const auto right = xx * r + xy * b;
        const auto bottom = yx * r + yy * b;

        d->bounds = QRectF(left + tx, top + ty, right - left, bottom - top);
    }

    // Warp version ( = 1 for Photoshop 6.0)
    auto warpVersion = readU16(source, length);
    warnUnexpected("warpVersion", warpVersion, 1u);

    // Descriptor version ( = 16 for Photoshop 6.0)
    descriptorVersion = readU32(source, length);
    warnUnexpected("warpDescriptorVersion", descriptorVersion, 16u);

    // Warp data (see See Descriptor structure)
    d->warpData = QPsdDescriptor(source, length);

    // left, top, right, bottom respectively.
    d->rect = readRectangle(source, length);
}

QPsdTypeToolObjectSetting::QPsdTypeToolObjectSetting(const QPsdTypeToolObjectSetting &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdTypeToolObjectSetting &QPsdTypeToolObjectSetting::operator=(const QPsdTypeToolObjectSetting &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdTypeToolObjectSetting::~QPsdTypeToolObjectSetting() = default;

QList<qreal> QPsdTypeToolObjectSetting::transform() const
{
    return d->transform;
}

QPsdDescriptor QPsdTypeToolObjectSetting::textData() const
{
    return d->textData;
}

QPsdDescriptor QPsdTypeToolObjectSetting::warpData() const
{
    return d->warpData;
}

QRect QPsdTypeToolObjectSetting::rect() const
{
    return d->rect;
}

QRectF QPsdTypeToolObjectSetting::bounds() const
{
    return d->bounds;
}

QT_END_NAMESPACE

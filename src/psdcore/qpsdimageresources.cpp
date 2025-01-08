// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdimageresources.h"

QT_BEGIN_NAMESPACE

class QPsdImageResources::Private : public QSharedData
{
public:
    Private();
    QList<QPsdImageResourceBlock> imageResourceBlocks;
};

QPsdImageResources::Private::Private()
{}

QPsdImageResources::QPsdImageResources()
    : QPsdSection()
    , d(new Private)
{}

QPsdImageResources::QPsdImageResources(QIODevice *source)
    : QPsdImageResources()
{
    // Image Resources Section
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_69883

    // Length of image resource section. The length may be zero.
    auto length = readU32(source);
    EnsureSeek es(source, length);

    // Image resources
    while (source->isOpen() && es.bytesAvailable() > 8) {
        qint64 initialAvailable = es.bytesAvailable();
        QPsdImageResourceBlock block(source);
        d->imageResourceBlocks.append(block);
        
        // Break if no progress was made to avoid infinite loop
        if (es.bytesAvailable() >= initialAvailable) {
            qWarning() << "No progress in reading image resource block. Breaking to avoid infinite loop.";
            break;
        }
    }
}

QPsdImageResources::QPsdImageResources(const QPsdImageResources &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdImageResources &QPsdImageResources::operator=(const QPsdImageResources &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdImageResources::~QPsdImageResources() = default;

QList<QPsdImageResourceBlock> QPsdImageResources::imageResourceBlocks() const
{
    return d->imageResourceBlocks;
}

QT_END_NAMESPACE

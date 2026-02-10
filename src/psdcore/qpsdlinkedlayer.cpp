// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdlinkedlayer.h"
#include "qpsddescriptor.h"

#include <QtCore/QLoggingCategory>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdLinkedLayer, "qt.psdcore.linkedlayer")

class QPsdLinkedLayer::Private : public QSharedData
{
public:
    QList<LinkedFile> files;
};

QPsdLinkedLayer::QPsdLinkedLayer()
    : QPsdSection()
    , d(new Private)
{}

QPsdLinkedLayer::QPsdLinkedLayer(QIODevice *source, quint32 length)
    : QPsdLinkedLayer()
{
    qCDebug(lcQPsdLinkedLayer) << (void *)source->pos() << length;
    auto cleanup = qScopeGuard([&] {
        qCDebug(lcQPsdLinkedLayer) << length;
        // Q_ASSERT(length == 0);
    });

    while (length > 20) {
        // workaround: adjust position to read write data
        const QByteArray preload = source->peek(20);
        const auto liF_ = preload.indexOf("liF");
        if (liF_ < 0) {
            break;
        }
        skip(source, liF_ - 8, &length);

        LinkedFile file;

        auto size = readU64(source, &length);
        EnsureSeek es(source, size);

        // Type ( = 'liFD' linked file data, 'liFE' linked file external or 'liFA' linked file alias )
        const auto type = readByteArray(source, 4, &length);
        if (type != "liFD" && type != "liFE" && type != "liFA") {
            qWarning("QPsdLinkedLayer: unsupported linked file type '%s'", type.constData());
            return;
        }

        // Version ( = 1 to 7 )
        const auto version = readU32(source, &length);
        qCDebug(lcQPsdLinkedLayer) << "version" << version;
        if (version < 1 || version > 7) {
            qWarning("QPsdLinkedLayer: unsupported version %u", version);
            return;
        }

        // Pascal string. Unique ID.
        file.uniqueId = readPascalString(source, 1, &length);
        qCDebug(lcQPsdLinkedLayer) << "uniqueId" << file.uniqueId;

        // Unicode string of the original file name
        file.name = readString(source, &length);
        qCDebug(lcQPsdLinkedLayer) << "name" << file.name;

        // File Type
        file.type = readByteArray(source, 4, &length);
        qCDebug(lcQPsdLinkedLayer) << "type" << file.type;

        // File Creator
        const auto fileCreator = readByteArray(source, 4, &length);
        qCDebug(lcQPsdLinkedLayer) << "fileCreator" << fileCreator;

        // Length of the data to follow
        size = readU64(source, &length);
        qCDebug(lcQPsdLinkedLayer) << "size" << size;

        // File open descriptor flag
        const auto fileOpenDescriptorFlag = readU8(source, &length);
        qCDebug(lcQPsdLinkedLayer) << "fileOpenDescriptorFlag" << fileOpenDescriptorFlag;
        // Descriptor of open parameters. Only present when above is true.


        QPsdDescriptor fileOpenDescriptor;
        if (fileOpenDescriptorFlag) {
            skip(source, 4, &length);
            fileOpenDescriptor = QPsdDescriptor(source, &length);
            qCDebug(lcQPsdLinkedLayer) << "fileOpenDescriptor" << fileOpenDescriptor.data();
        }

        if (type == "liFD") {
            // Raw bytes of the file.
            file.data = readByteArray(source, size, &length);
        }
        // If the version is greater than or equal to 5 then the following is next.
        if (version >= 5) {
            // Child Document ID.
            const auto childDocumentID = readString(source, &length);
            qCDebug(lcQPsdLinkedLayer) << "childDocumentID" << childDocumentID;
        }
        // If the version is greater than or equal to 6 then the following is next.
        if (version >= 6) {
            // Asset mod time.
            const auto assetModTime = readDouble(source, &length);
            qCDebug(lcQPsdLinkedLayer) << "assetModTime" << assetModTime;
        }
        // If the version is greater than or equal to 7 then the following is next.
        if (version >= 7) {
            // Asset locked state, for Libraries assets.
            const bool assetLockedState = readU8(source, &length);
            qCDebug(lcQPsdLinkedLayer) << "assetLockedState" << assetLockedState;
        }
        d->files.append(file);
    }
}

QPsdLinkedLayer::QPsdLinkedLayer(const QPsdLinkedLayer &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdLinkedLayer &QPsdLinkedLayer::operator=(const QPsdLinkedLayer &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdLinkedLayer::~QPsdLinkedLayer() = default;

QList<QPsdLinkedLayer::LinkedFile> QPsdLinkedLayer::files() const
{
    return d->files;
}

QT_END_NAMESPACE

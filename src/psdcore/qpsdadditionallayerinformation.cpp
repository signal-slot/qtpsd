// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdadditionallayerinformation.h"
#include "qpsdadditionallayerinformationplugin.h"

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdAdditionalLayerInformation, "qt.psdcore.additionalinformation")

class QPsdAdditionalLayerInformation::Private : public QSharedData
{
public:
    QByteArray key;
    QVariant data;
};

QPsdAdditionalLayerInformation::QPsdAdditionalLayerInformation()
    : QPsdSection()
    , d(new Private)
{}

QPsdAdditionalLayerInformation::QPsdAdditionalLayerInformation(QIODevice *source, int padding)
    : QPsdAdditionalLayerInformation()
{
    // Additional Layer Information
    // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_71546

    QByteArray peek4 = source->peek(40);
    if (peek4.indexOf('8') > 0) {
        qWarning() << "skipping" << peek4.indexOf('8') << "bytes to find right signature";
        source->read(peek4.indexOf('8'));
    }
    // Signature: '8BIM' or '8B64'
    const auto signature = readByteArray(source, 4);
    if (signature != "8BIM" && signature != "8B64") {
        qWarning() << signature;
        qFatal("Signature mismatch!");
    }

    // Key: a 4-character code (See individual sections)
    d->key = readByteArray(source, 4);

    // Length data below, rounded up to an even byte count.
    // (**PSB**, the following keys have a length count of 8 bytes: LMsk, Lr16, Lr32, Layr, Mt16, Mt32, Mtrn, Alph, FMsk, lnk2, FEid, FXid, PxSD.
    auto length = readU32(source);
    EnsureSeek es(source, length, padding);

    auto plugin = QPsdAdditionalLayerInformationPlugin::plugin(d->key);
    if (plugin) {
        qCDebug(lcQPsdAdditionalLayerInformation) << (void *)source->pos() << d->key << length;
        d->data = plugin->parse(source, length);
        qCDebug(lcQPsdAdditionalLayerInformation) << (void *)source->pos() << d->key << d->data;
    } else {
        QByteArray data;
        if (es.bytesAvailable() > 0)
            data = source->read(es.bytesAvailable());
        d->data = data;
        if (data.size() > 32) {
            data.resize(32);
            data.append("...");
        }
        qWarning() << source->pos() << d->key << length << "not supported" << data;
    }
}

QPsdAdditionalLayerInformation::QPsdAdditionalLayerInformation(const QPsdAdditionalLayerInformation &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdAdditionalLayerInformation &QPsdAdditionalLayerInformation::operator=(const QPsdAdditionalLayerInformation &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdAdditionalLayerInformation::~QPsdAdditionalLayerInformation() = default;

QByteArray QPsdAdditionalLayerInformation::key() const
{
    return d->key;
}

QVariant QPsdAdditionalLayerInformation::data() const
{
    return d->data;
}

QT_END_NAMESPACE

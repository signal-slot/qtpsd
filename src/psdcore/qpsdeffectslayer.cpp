// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdeffectslayer.h"
#include "qpsdeffectslayerplugin.h"

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdEffectsLayer, "qt.psdcore.effectslayer")

class QPsdEffectsLayer::Private : public QSharedData
{
public:
    QVariantList effects;
};

QPsdEffectsLayer::QPsdEffectsLayer()
    : QPsdSection()
    , d(new Private)
{}

QPsdEffectsLayer::QPsdEffectsLayer(QIODevice *source, quint32 *length)
    : QPsdEffectsLayer()
{
    // Version: 0
    auto version = readU16(source, length);
    Q_ASSERT(version == 0);

    // Effects count: may be 6 (for the 6 effects in Photoshop 5 and 6) or 7 (for Photoshop 7.0)
    auto effectsCount = readU16(source, length);
    // It seems that there are PSD files that do not comply with the specifications.
    // effectsCount == 1, e.g. ag-psd/test/read-write/animation-effects/expected.psd
    // effectsCount == 2, 3 e.g. ag-psd/test/read-write/effects/expected.psd
    Q_ASSERT(effectsCount == 6 || effectsCount == 7 || effectsCount == 1 || effectsCount == 2 || effectsCount  == 3);
    qCDebug(lcQPsdEffectsLayer) << "count =" << effectsCount;
    while (effectsCount-- > 0) {
        qCDebug(lcQPsdEffectsLayer) << effectsCount;
        // Signature: '8BIM'
        auto signature = readByteArray(source, 4, length);
        Q_ASSERT(signature == "8BIM");
        // Effects signatures: OSType key for which effects type to use:
        auto osType = readByteArray(source, 4, length);
        auto plugin = QPsdEffectsLayerPlugin::plugin(osType);
        if (plugin) {
            auto value = plugin->parse(osType, source, length);
            if (value.isNull())
                continue;
            d->effects.append(value);
            qCDebug(lcQPsdEffectsLayer) << value;
        } else {
            qCWarning(lcQPsdEffectsLayer) << osType << "not supported";
            continue;
        }
    }
}

QPsdEffectsLayer::QPsdEffectsLayer(const QPsdEffectsLayer &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdEffectsLayer &QPsdEffectsLayer::operator=(const QPsdEffectsLayer &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdEffectsLayer::~QPsdEffectsLayer() = default;

QVariantList QPsdEffectsLayer::effects() const
{
    return d->effects;
}

QT_END_NAMESPACE

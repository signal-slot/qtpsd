// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdeffectslayer.h"
#include "qpsdeffectslayerplugin.h"
#include "qpsdsofieffect.h"
#include "qpsdoglweffect.h"
#include "qpsdiglweffect.h"
#include "qpsdshadoweffect.h"
#include "qpsdbevleffect.h"
#include "qpsdblend.h"

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdEffectsLayer, "qt.psdcore.effectslayer")

class QPsdEffectsLayer::Private : public QSharedData
{
public:
    QVariantList effects;
    QByteArray rawData;
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
    if (version != 0) {
        qCWarning(lcQPsdEffectsLayer) << "Unsupported effects layer version:" << version;
        return;
    }

    // Effects count: may be 6 (for the 6 effects in Photoshop 5 and 6) or 7 (for Photoshop 7.0)
    auto effectsCount = readU16(source, length);
    // It seems that there are PSD files that do not comply with the specifications.
    // effectsCount == 1, e.g. ag-psd/test/read-write/animation-effects/expected.psd
    // effectsCount == 2, 3 e.g. ag-psd/test/read-write/effects/expected.psd
    if (!(effectsCount == 6 || effectsCount == 7 || effectsCount == 1 || effectsCount == 2 || effectsCount == 3)) {
        qCWarning(lcQPsdEffectsLayer) << "Unexpected effects count:" << effectsCount;
        return;
    }
    qCDebug(lcQPsdEffectsLayer) << "count =" << effectsCount;
    while (effectsCount-- > 0) {
        qCDebug(lcQPsdEffectsLayer) << effectsCount;
        // Signature: '8BIM'
        auto signature = readByteArray(source, 4, length);
        if (signature != "8BIM") {
            qCWarning(lcQPsdEffectsLayer) << "Invalid effects signature:" << signature;
            break;
        }
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
            // Each effect entry stores a size-prefixed payload.
            // Skip unsupported entries so the stream stays aligned.
            const auto size = readU32(source, length);
            skip(source, size, length);
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

QByteArray QPsdEffectsLayer::rawData() const
{
    return d->rawData;
}

void QPsdEffectsLayer::setRawData(const QByteArray &data)
{
    d->rawData = data;
}

void QPsdEffectsLayer::write(QIODevice *dest) const
{
    // Use raw data for lossless round-trip if available
    if (!d->rawData.isEmpty()) {
        dest->write(d->rawData);
        return;
    }

    // Color space data is lost during parse (toString()), write zeros as placeholder
    const QByteArray zeroColorSpace(10, '\0');

    // Version
    writeU16(dest, 0);
    // Effects count: actual effects + 1 for cmnS
    writeU16(dest, static_cast<quint16>(d->effects.size() + 1));

    // Write cmnS (common state) block
    dest->write("8BIM", 4);
    dest->write("cmnS", 4);
    writeU32(dest, 7);  // size
    writeU32(dest, 0);  // version
    writeU8(dest, 1);   // visible
    dest->write(QByteArray(2, '\0'));

    for (const auto &effect : d->effects) {
        dest->write("8BIM", 4);

        if (effect.canConvert<QPsdShadowEffect>()) {
            const auto e = effect.value<QPsdShadowEffect>();
            const bool isDrop = (e.type() == QPsdAbstractEffect::DropShadow);
            dest->write(isDrop ? "dsdw" : "isdw", 4);
            writeU32(dest, 51);  // size (version 2)
            writeU32(dest, 2);   // version
            writeS32(dest, static_cast<qint32>(e.blur()));
            writeU32(dest, e.intensity());
            writeS32(dest, static_cast<qint32>(e.angle()));
            writeS32(dest, static_cast<qint32>(e.distance()));
            dest->write(zeroColorSpace);  // color (lost)
            dest->write("8BIM", 4);
            dest->write(QPsdBlend::toKey(e.blendMode()));
            writeU8(dest, e.isEnabled() ? 1 : 0);
            writeU8(dest, e.useAngleInAllEffects() ? 1 : 0);
            writeU8(dest, static_cast<quint8>(qRound(e.opacity() * 255.0)));
            dest->write(zeroColorSpace);  // native color (lost)
        } else if (effect.canConvert<QPsdIglwEffect>()) {
            const auto e = effect.value<QPsdIglwEffect>();
            dest->write("iglw", 4);
            writeU32(dest, 43);  // size (version 2)
            writeU32(dest, 2);   // version
            writeU32(dest, e.blur());
            writeU32(dest, e.intensity());
            dest->write(zeroColorSpace);  // color (lost)
            dest->write("8BIM", 4);
            dest->write(QPsdBlend::toKey(e.blendMode()));
            writeU8(dest, e.isEnabled() ? 1 : 0);
            writeU8(dest, static_cast<quint8>(qRound(e.opacity() * 255.0)));
            writeU8(dest, e.invert() ? 1 : 0);
            dest->write(zeroColorSpace);  // native color (lost)
        } else if (effect.canConvert<QPsdOglwEffect>()) {
            const auto e = effect.value<QPsdOglwEffect>();
            dest->write("oglw", 4);
            writeU32(dest, 42);  // size (version 2)
            writeU32(dest, 2);   // version
            writeU32(dest, e.blur());
            writeU32(dest, e.intensity());
            dest->write(zeroColorSpace);  // color (lost)
            dest->write("8BIM", 4);
            dest->write(QPsdBlend::toKey(e.blendMode()));
            writeU8(dest, e.isEnabled() ? 1 : 0);
            writeU8(dest, static_cast<quint8>(qRound(e.opacity() * 255.0)));
            dest->write(zeroColorSpace);  // native color (lost)
        } else if (effect.canConvert<QPsdBevlEffect>()) {
            const auto e = effect.value<QPsdBevlEffect>();
            dest->write("bevl", 4);
            writeU32(dest, 78);  // size (version 2)
            writeU32(dest, 2);   // version
            writeU32(dest, e.angle());
            writeU32(dest, e.strength());
            writeU32(dest, e.blur());
            dest->write("8BIM", 4);
            dest->write(QPsdBlend::toKey(e.highlightBlendMode()));
            dest->write("8BIM", 4);
            dest->write(QPsdBlend::toKey(e.shadowBlendMode()));
            dest->write(zeroColorSpace);  // highlight color (lost)
            dest->write(zeroColorSpace);  // shadow color (lost)
            writeU8(dest, e.bevelStyle());
            writeU8(dest, static_cast<quint8>(qRound(e.highlightOpacity() * 255.0)));
            writeU8(dest, static_cast<quint8>(qRound(e.shadowOpacity() * 255.0)));
            writeU8(dest, e.isEnabled() ? 1 : 0);
            writeU8(dest, e.useGlobalAngle() ? 1 : 0);
            writeU8(dest, e.upOrDown() ? 1 : 0);
            dest->write(zeroColorSpace);  // real highlight color (lost)
            dest->write(zeroColorSpace);  // real shadow color (lost)
        } else if (effect.canConvert<QPsdSofiEffect>()) {
            const auto e = effect.value<QPsdSofiEffect>();
            dest->write("sofi", 4);
            writeU32(dest, 34);  // size
            writeU32(dest, 2);   // version
            dest->write("8BIM", 4);
            dest->write(QPsdBlend::toKey(e.blendMode()));
            dest->write(zeroColorSpace);  // color (lost)
            writeU8(dest, static_cast<quint8>(qRound(e.opacity() * 255.0)));
            writeU8(dest, e.isEnabled() ? 1 : 0);
            dest->write(zeroColorSpace);  // native color (lost)
        }
    }
}

QT_END_NAMESPACE

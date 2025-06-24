// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdeffectslayerplugin.h>
#include <QtPsdCore/qpsdlayerrecord.h>
#include <QtPsdCore/qpsdsofieffect.h>
#include <QtPsdCore/qpsdcolorspace.h>

QT_BEGIN_NAMESPACE

class QPsdEffectsLayerSofiPlugin : public QPsdEffectsLayerPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdEffectsLayerFactoryInterface" FILE "sofi.json")
public:
    // Effects layer, solid fill (added in Photoshop 7.0)
    QVariant parse(QByteArrayView key, QIODevice *source , quint32 *length) const override {
        Q_ASSERT(key == "sofi");
        // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_70055

        QPsdSofiEffect ret;

        // Size: 34
        const auto size = readU32(source, length);
        Q_ASSERT(size == 34);

        EnsureSeek es(source, size);

        // Version: 2
        const auto version = readU32(source, length);
        Q_ASSERT(version == 2);

        // THIS IS NOT DOCUMENTED
        // signature: '8BIM'
        const auto signature = readByteArray(source, 4, length);
        Q_ASSERT(signature == "8BIM");

        // Key for blend mode
        const auto blendKey = readByteArray(source, 4, length);
        ret.setBlendMode(blendKey);

        // Color space
        const auto colorSpace = readColorSpace(source, length);
        // Note: QPsdSofiEffect doesn't store the initial color, only native color

        // Opacity
        const auto opacity = readU8(source, length);
        ret.setOpacity(opacity);

        // Enabled
        const auto enabled = readU8(source, length);
        ret.setEnabled(enabled);

        // Native color space
        const auto nativeColorSpace = readColorSpace(source, length);
        const auto nativeColor = nativeColorSpace.toString();
        ret.setNativeColor(nativeColor);

        return ret.isEnabled() ? QVariant::fromValue(ret) : QVariant();
    }
};

QT_END_NAMESPACE

#include "sofi.moc"

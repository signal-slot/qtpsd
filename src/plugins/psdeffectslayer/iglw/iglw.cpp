// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdeffectslayerplugin.h>
#include <QtPsdCore/qpsdiglweffect.h>
#include <QtPsdCore/qpsdcolorspace.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdEffectsLayerIglwPlugin, "qt.psdcore.plugins.effects.iglw")

class QPsdEffectsLayerIglwPlugin : public QPsdEffectsLayerPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdEffectsLayerFactoryInterface" FILE "iglw.json")
public:
    // Effects layer, inner glow info
    QVariant parse(QByteArrayView key, QIODevice *source , quint32 *length) const override {
        Q_ASSERT(key == "iglw");
        // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_27692

        QPsdIglwEffect ret;

        // Size of the remaining items: 33 for Photoshop 5.0; 43 for 5.5
        const auto size = readU32(source, length);
        Q_ASSERT(size == 33 || size == 43);

        // Version: 0 for Photoshop 5.0; 2 for 5.5.
        const auto version = readU32(source, length);
        Q_ASSERT(version == 0 || version == 2);

        // Blur value in pixels.
        const auto blur = readU32(source, length);
        ret.setBlur(blur);
        qCDebug(lcQPsdEffectsLayerIglwPlugin) << "Blur:" << blur;

        // Intensity as a percent
        const auto intensity = readU32(source, length);
        ret.setIntensity(intensity);
        qCDebug(lcQPsdEffectsLayerIglwPlugin) << "Intensity:" << intensity;

        // Color: 2 bytes for space followed by 4 * 2 byte color component
        const auto colorSpace = readColorSpace(source, length);
        const auto color = colorSpace.toString();
        ret.setColor(color);
        qCDebug(lcQPsdEffectsLayerIglwPlugin) << "Color:" << color;

        // Blend mode: 4 bytes for signature and 4 bytes for the key
        const auto blendMode = readByteArray(source, 4, length);
        Q_ASSERT(blendMode == "8BIM");
        const auto blendKey = readByteArray(source, 4, length);
        ret.setBlendMode(blendKey);
        qCDebug(lcQPsdEffectsLayerIglwPlugin) << "blend" << blendKey;

        // Effect enabled
        const auto enabled = readU8(source, length);
        ret.setEnabled(enabled);
        qCDebug(lcQPsdEffectsLayerIglwPlugin) << "Enabled:" << enabled;

        // Opacity as a percent
        const auto opacity = readU8(source, length);
        ret.setOpacity(opacity);
        qCDebug(lcQPsdEffectsLayerIglwPlugin) << "Opacity:" << opacity;

        // Remaining fields present only in version 2
        if (version == 2) {
            // Invert
            const auto invert = readU8(source, length);
            ret.setInvert(invert);
            qCDebug(lcQPsdEffectsLayerIglwPlugin) << "Invert:" << invert;
            // (Version 2 only) Native color space. 2 bytes for space followed by 4 * 2 byte color component
            const auto nativeColorSpace = readColorSpace(source, length);
            const auto nativeColor = nativeColorSpace.toString();
            ret.setNativeColor(nativeColor);
            qCDebug(lcQPsdEffectsLayerIglwPlugin) << "Native color:" << nativeColor;
        }

        return ret.isEnabled() ? QVariant::fromValue(ret) : QVariant();
    }
};

QT_END_NAMESPACE

#include "iglw.moc"

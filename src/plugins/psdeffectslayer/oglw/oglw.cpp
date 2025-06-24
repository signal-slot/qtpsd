// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdeffectslayerplugin.h>
#include <QtPsdCore/qpsdoglweffect.h>
#include <QtPsdCore/qpsdcolorspace.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdEffectsLayerOglwPlugin, "qt.psdcore.plugins.effects.oglw")

class QPsdEffectsLayerOglwPlugin : public QPsdEffectsLayerPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdEffectsLayerFactoryInterface" FILE "oglw.json")
public:
    // Effects layer, outer glow info
    QVariant parse(QByteArrayView key, QIODevice *source , quint32 *length) const override {
        Q_ASSERT(key == "oglw");
        // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/PhotoshopFileFormats.htm#50577409_25738

        QPsdOglwEffect ret;

        // Size of the remaining items: 32 for Photoshop 5.0; 42 for 5.5
        const auto size = readU32(source, length);
        Q_ASSERT(size == 32 || size == 42);

        // Version: 0 for Photoshop 5.0; 2 for 5.5
        const auto version = readU32(source, length);
        Q_ASSERT(version == 0 || version == 2);

        // Blur value in pixels.
        const auto blur = readU32(source, length);
        ret.setBlur(blur);
        qCDebug(lcQPsdEffectsLayerOglwPlugin) << "Blur:" << blur;

        // Intensity as a percent
        const auto intensity = readU32(source, length);
        ret.setIntensity(intensity);
        qCDebug(lcQPsdEffectsLayerOglwPlugin) << "Intensity:" << intensity;

        // Color: 2 bytes for space followed by 4 * 2 byte color component
        const auto colorSpace = readColorSpace(source, length);
        const auto color = colorSpace.toString();
        ret.setColor(color);
        qCDebug(lcQPsdEffectsLayerOglwPlugin) << "Color:" << color;

        // Blend mode: 4 bytes for signature and 4 bytes for the key
        const auto blendMode = readByteArray(source, 4, length);
        Q_ASSERT(blendMode == "8BIM");
        const auto blendKey = readByteArray(source, 4, length);
        ret.setBlendMode(blendKey);
        qCDebug(lcQPsdEffectsLayerOglwPlugin) << "Blend:" << blendKey;

        // Effect enabled
        const auto enabled = readU8(source, length);
        ret.setEnabled(enabled);
        qCDebug(lcQPsdEffectsLayerOglwPlugin) << "Enabled:" << enabled;

        // Opacity as a percent
        const auto opacity = readU8(source, length);
        ret.setOpacity(opacity);
        qCDebug(lcQPsdEffectsLayerOglwPlugin) << "Opacity:" << opacity;

        // (Version 2 only) Native color space. 2 bytes for space followed by 4 * 2 byte color component
        if (version == 2) {
            const auto nativeColorSpace = readColorSpace(source, length);
            const auto nativeColor = nativeColorSpace.toString();
            ret.setNativeColor(nativeColor);
            qCDebug(lcQPsdEffectsLayerOglwPlugin) << "Native color:" << nativeColor;
        }

        return ret.isEnabled() ? QVariant::fromValue(ret) : QVariant();
    }
};

QT_END_NAMESPACE

#include "oglw.moc"

// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdeffectslayerplugin.h>
#include <QtPsdCore/qpsdshadoweffect.h>
#include <QtPsdCore/qpsdcolorspace.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdEffectsLayerShadowPlugin, "qt.psdcore.plugins.effects.shadow")

class QPsdEffectsLayerShadowPlugin : public QPsdEffectsLayerPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdEffectsLayerFactoryInterface" FILE "shadow.json")
public:
    // Effects layer, drop shadow and inner shadow info
    QVariant parse(QByteArrayView key, QIODevice *source , quint32 *length) const override {
        Q_ASSERT(key == "dsdw" || key == "isdw");
        // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/PhotoshopFileFormats.htm#50577409_22203

        QPsdShadowEffect ret(key == "dsdw" ? QPsdShadowEffect::DropShadow : QPsdShadowEffect::InnerShadow);

        // Size of the remaining items: 41 or 51 (depending on version)
        const auto size = readU32(source, length);
        Q_ASSERT(size == 41 || size == 51);

        // Version: 0 ( Photoshop 5.0) or 2 ( Photoshop 5.5)
        const auto version = readU32(source, length);
        Q_ASSERT(version == 0 || version == 2);

        // Blur value in pixels
        const auto blur = readS32(source, length);
        ret.setBlur(blur);
        qCDebug(lcQPsdEffectsLayerShadowPlugin) << "Blur:" << blur;

        // Intensity as a percent
        const auto intensity = readU32(source, length);
        ret.setIntensity(intensity);
        qCDebug(lcQPsdEffectsLayerShadowPlugin) << "Intensity:" << intensity;

        // Angle in degrees
        const auto angle = readS32(source, length);
        ret.setAngle(angle);
        qCDebug(lcQPsdEffectsLayerShadowPlugin) << "Angle:" << angle;

        // Distance in pixels
        const auto distance = readS32(source, length);
        ret.setDistance(distance);
        qCDebug(lcQPsdEffectsLayerShadowPlugin) << "Distance:" << distance;

        // Color: 2 bytes for space followed by 4 * 2 byte color component
        const auto colorSpace = readColorSpace(source, length);
        const auto color = colorSpace.toString();
        ret.setColor(color);
        qCDebug(lcQPsdEffectsLayerShadowPlugin) << "Color:" << color;

        // Blend mode: 4 bytes for signature and 4 bytes for key
        const auto blendMode = readByteArray(source, 4, length);
        Q_ASSERT(blendMode == "8BIM");
        const auto blendKey = readByteArray(source, 4, length);
        ret.setBlendMode(blendKey);
        qCDebug(lcQPsdEffectsLayerShadowPlugin) << "Blend" << blendKey;

        // Effect enabled
        const auto enabled = readU8(source, length);
        ret.setEnabled(enabled);
        qCDebug(lcQPsdEffectsLayerShadowPlugin) << "Enabled:" << enabled;

        // Use this angle in all of the layer effects
        const auto useAngleInAllEffects = readU8(source, length);
        ret.setUseAngleInAllEffects(useAngleInAllEffects);
        qCDebug(lcQPsdEffectsLayerShadowPlugin) << "Use angle in all effects:" << useAngleInAllEffects;

        // Opacity as a percent
        const auto opacity = readU8(source, length);
        ret.setOpacity(opacity);
        qCDebug(lcQPsdEffectsLayerShadowPlugin) << "Opacity:" << opacity;

        // Native color: 2 bytes for space followed by 4 * 2 byte color component
        const auto nativeColorSpace = readColorSpace(source, length);
        const auto nativeColor = nativeColorSpace.toString();
        ret.setNativeColor(nativeColor);
        qCDebug(lcQPsdEffectsLayerShadowPlugin) << "Native color:" << nativeColor;

        return ret.isEnabled() ? QVariant::fromValue(ret) : QVariant();
    }
};

QT_END_NAMESPACE

#include "shadow.moc"

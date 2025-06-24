// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/qpsdeffectslayerplugin.h>
#include <QtPsdCore/qpsdbevleffect.h>
#include <QtPsdCore/qpsdcolorspace.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdEffectsLayerBevlPlugin, "qt.psdcore.plugins.effects.bevl")

class QPsdEffectsLayerBevlPlugin : public QPsdEffectsLayerPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPsdEffectsLayerFactoryInterface" FILE "bevl.json")
public:
    // Effects layer, bevel info
    QVariant parse(QByteArrayView key, QIODevice *source , quint32 *length) const override {
        Q_ASSERT(key == "bevl");
        // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_31889

        QPsdBevlEffect ret;

        // Size of the remaining items (58 for version 0, 78 for version 20
        const auto size = readU32(source, length);
        Q_ASSERT(size == 58 || size == 78);

        // Version: 0 for Photoshop 5.0; 2 for 5.5
        const auto version = readU32(source, length);
        Q_ASSERT(version == 0 || version == 2);

        // Angle in degrees
        const auto angle = readU32(source, length);
        ret.setAngle(angle);
        qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Angle:" << angle;

        // Strength. Depth in pixels
        const auto strength = readU32(source, length);
        ret.setStrength(strength);
        qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Strength:" << strength;

        // Blur value in pixels.
        const auto blur = readU32(source, length);
        ret.setBlur(blur);
        qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Blur:" << blur;

        // Highlight blend mode: 4 bytes for signature and 4 bytes for the key
        const auto highlightBlendMode = readByteArray(source, 4, length);
        Q_ASSERT(highlightBlendMode == "8BIM");
        const auto highlightBlendKey = readByteArray(source, 4, length);
        ret.setHighlightBlendMode(highlightBlendKey);
        qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Highlight blend" << highlightBlendKey;

        // Shadow blend mode: 4 bytes for signature and 4 bytes for the key
        const auto shadowBlendMode = readByteArray(source, 4, length);
        Q_ASSERT(shadowBlendMode == "8BIM");
        const auto shadowBlendKey = readByteArray(source, 4, length);
        ret.setShadowBlendMode(shadowBlendKey);
        qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Shadow blend" << shadowBlendKey;

        // Highlight color: 2 bytes for space followed by 4 * 2 byte color component
        const auto highlightColorSpace = readColorSpace(source, length);
        const auto highlightColor = highlightColorSpace.toString();
        ret.setHighlightColor(highlightColor);
        qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Highlight color:" << highlightColor;

        // Shadow color: 2 bytes for space followed by 4 * 2 byte color component
        const auto shadowColorSpace = readColorSpace(source, length);
        const auto shadowColor = shadowColorSpace.toString();
        ret.setShadowColor(shadowColor);
        qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Shadow color:" << shadowColor;

        // Bevel style
        const auto bevelStyle = readU8(source, length);
        ret.setBevelStyle(bevelStyle);
        qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Bevel style:" << bevelStyle;

        // Hightlight opacity as a percent
        const auto highlightOpacity = readU8(source, length);
        ret.setHighlightOpacity(highlightOpacity);
        qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Highlight opacity:" << highlightOpacity;

        // Shadow opacity as a percent
        const auto shadowOpacity = readU8(source, length);
        ret.setShadowOpacity(shadowOpacity);
        qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Shadow opacity:" << shadowOpacity;

        // Effect enabled
        const auto enabled = readU8(source, length);
        ret.setEnabled(enabled);
        qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Enabled:" << enabled;

        // Use this angle in all of the layer effects
        const auto useGlobalAngle = readU8(source, length);
        ret.setUseGlobalAngle(useGlobalAngle);
        qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Use global angle:" << useGlobalAngle;

        // Up or down
        const auto upOrDown = readU8(source, length);
        ret.setUpOrDown(upOrDown);
        qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Up or down:" << upOrDown;

        // The following are present in version 2 only
        if (version == 2) {
            // Real highlight color: 2 bytes for space; 4 * 2 byte color component
            const auto realHighlightColorSpace = readColorSpace(source, length);
            const auto realHighlightColor = realHighlightColorSpace.toString();
            ret.setRealHighlightColor(realHighlightColor);
            qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Real highlight color:" << realHighlightColor;

            // Real shadow color: 2 bytes for space; 4 * 2 byte color component
            const auto realShadowColorSpace = readColorSpace(source, length);
            const auto realShadowColor = realShadowColorSpace.toString();
            ret.setRealShadowColor(realShadowColor);
            qCDebug(lcQPsdEffectsLayerBevlPlugin) << "Real shadow color:" << realShadowColor;
        }

        return ret.isEnabled() ? QVariant::fromValue(ret) : QVariant();
    }
};

QT_END_NAMESPACE

#include "bevl.moc"

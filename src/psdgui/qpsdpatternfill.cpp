// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdpatternfill.h"

QT_BEGIN_NAMESPACE

class QPsdPatternFill::Private
{
public:
    Private(const QHash<QByteArray, QVariant> &descriptor) {
        // Mode
        if (descriptor.contains("Md  ")) {
            const auto md__ = descriptor.value("Md  ").value<QPsdEnum>();
            Q_ASSERT(md__.type() == "BlnM");
            blendMode = QPsdBlend::from(md__.value());
        }

        // Opacity
        if (descriptor.contains("Opct")) {
            const auto opct = descriptor.value("Opct").value<QPsdUnitFloat>();
            Q_ASSERT(opct.unit() == QPsdUnitFloat::Percent);
            opacity = opct.value() / 100.0;
        }

        // Pattern
        if (descriptor.contains("Ptrn")) {
            const auto patn = descriptor.value("Ptrn").value<QPsdDescriptor>().data();
            const auto idnt = patn.value("Idnt").toString();
            patternID = idnt;
        }

        // Snap
        if (descriptor.contains("Algn")) {
            aligned = descriptor.value("Algn").toBool();
        }

        // Angle
        if (descriptor.contains("Angl")) {
            const auto angl = descriptor.value("Angl").value<QPsdUnitFloat>();
            Q_ASSERT(angl.unit() == QPsdUnitFloat::Angle);
            angle = angl.value();
        }
        // Scale
        if (descriptor.contains("Scl ")) {
            const auto scl_ = descriptor.value("Scl ").value<QPsdUnitFloat>();
            Q_ASSERT(scl_.unit() == QPsdUnitFloat::Percent);
            scale = scl_.value();
        }

        // Phase?
        if (descriptor.contains("phase")) {
            const auto pnt_ = descriptor.value("phase").value<QPsdDescriptor>().data();
            const auto vrtc = pnt_.value("Vrtc").toDouble();
            const auto hrzn = pnt_.value("Hrzn").toDouble();
            phase = QPointF(hrzn, vrtc);
        }
    }

    QPsdBlend::Mode blendMode {QPsdBlend::Mode::Normal};
    qreal opacity = 1.0;
    QString patternID;
    bool aligned = false;
    qreal angle = 0;
    qreal scale = 100;
    QPointF phase{0, 0};
};

QPsdPatternFill::QPsdPatternFill(const QPsdDescriptor &descriptor)
    : d(new Private(descriptor.data()))
{}

QPsdPatternFill::~QPsdPatternFill() = default;

QPsdBlend::Mode QPsdPatternFill::blendMode() const
{
    return d->blendMode;
}

qreal QPsdPatternFill::opacity() const
{
    return d->opacity;
}

QString QPsdPatternFill::patternID() const
{
    return d->patternID;
}

bool QPsdPatternFill::aligned() const
{
    return d->aligned;
}

qreal QPsdPatternFill::angle() const
{
    return d->angle;
}

qreal QPsdPatternFill::scale() const
{
    return d->scale;
}

QPointF QPsdPatternFill::phase() const
{
    return d->phase;
}

QT_END_NAMESPACE

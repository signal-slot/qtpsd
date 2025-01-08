// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdvectorstrokedata.h"
#include "qpsddescriptor.h"

#include <QtCore/QLoggingCategory>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdVectorStrokeData, "qt.psdcore.vstk")

class QPsdVectorStrokeData::Private : public QSharedData
{
public:
    bool fillEnabled = false;
    bool strokeEnabled = false;
    QPsdBlend::Mode strokeStyleBlendMode = QPsdBlend::Invalid;
    QString strokeStyleContent;
    QPsdEnum strokeStyleLineAlignment;
    QPsdEnum strokeStyleLineCapType;
    QPsdUnitFloat strokeStyleLineDashOffset;
    QList<qreal> strokeStyleLineDashSet;
    QPsdEnum strokeStyleLineJoinType;
    QPsdUnitFloat strokeStyleLineWidth;
    double strokeStyleMiterLimit = 0.0;
    QPsdUnitFloat strokeStyleOpacity;
    double strokeStyleResolution;
    bool strokeStyleScaleLock = false;
    bool strokeStyleStrokeAdjust = false;
    int strokeStyleVersion = 0;
};

#define FOREACH(f) \
    f(fillEnabled); \
    f(strokeEnabled); \
    f(strokeStyleBlendMode); \
    f(strokeStyleContent); \
    f(strokeStyleLineAlignment); \
    f(strokeStyleLineCapType); \
    f(strokeStyleLineDashOffset); \
    f(strokeStyleLineDashSet); \
    f(strokeStyleLineJoinType); \
    f(strokeStyleLineWidth); \
    f(strokeStyleMiterLimit); \
    f(strokeStyleOpacity); \
    f(strokeStyleResolution); \
    f(strokeStyleScaleLock); \
    f(strokeStyleStrokeAdjust); \
    f(strokeStyleVersion)

QPsdVectorStrokeData::QPsdVectorStrokeData()
    : QPsdSection()
    , d(new Private)
{}

QPsdVectorStrokeData::QPsdVectorStrokeData(QIODevice *source, quint32 length)
    : QPsdVectorStrokeData()
{
    auto cleanup = qScopeGuard([&] {
        // Q_ASSERT(length == 0);
    });
    // Version ( = 16 for Photoshop 6.0)
    auto version = readU32(source, &length);
    Q_ASSERT(version == 16);
    QPsdDescriptor descriptor(source, &length);
    const auto data = descriptor.data();
    auto keys = data.keys();
    std::sort(keys.begin(), keys.end(), std::less<QString>());
    for (const auto &key : keys) {
        const auto value = data.value(key);
#define STOREVALUE(name) \
        if (key == #name) \
            d->name = value.value<decltype(d->name)>()

        FOREACH(STOREVALUE);
#undef STOREVALUE
        if (key == "strokeStyleContent") {
            const auto descriptor = value.value<QPsdDescriptor>();
            if (descriptor.data().size() == 1) {
                const auto clr_ = descriptor.data().value("Clr ").value<QPsdDescriptor>().data();
                const int rd__ = clr_.value("Rd  ").toDouble();
                const int grn_ = clr_.value("Grn ").toDouble();
                const int bl__ = clr_.value("Bl  ").toDouble();
                d->strokeStyleContent = QString("#%1%2%3"_L1).arg(rd__, 2, 16, '0'_L1).arg(grn_, 2, 16, '0'_L1).arg(bl__, 2, 16, '0'_L1);
            } else {
                // TODO
            }
        } else if (key == "strokeStyleLineDashSet") {
            const auto list = value.toList();
            if (!list.isEmpty()) {
                QList<double> doubleList;
                doubleList.reserve(list.size());
                for (const auto &item : list) {
                    doubleList.append(item.toDouble());
                }
                qFatal() << value << doubleList;
            }
        }
        if (value.canConvert<QPsdEnum>()) {
            qCDebug(lcQPsdVectorStrokeData) << key << value.value<QPsdEnum>().type() << value.value<QPsdEnum>().value();
        } else if (value.canConvert<QPsdDescriptor>()) {
            qCDebug(lcQPsdVectorStrokeData) << key << value.value<QPsdDescriptor>().data().keys();
        } else if (value.canConvert<QPsdUnitFloat>()) {
            qCDebug(lcQPsdVectorStrokeData) << key << value.value<QPsdUnitFloat>().unit() << value.value<QPsdUnitFloat>().value();
        } else {
            qCDebug(lcQPsdVectorStrokeData) << key << value;
        }
    }
}

QPsdVectorStrokeData::QPsdVectorStrokeData(const QPsdVectorStrokeData &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdVectorStrokeData &QPsdVectorStrokeData::operator=(const QPsdVectorStrokeData &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdVectorStrokeData::~QPsdVectorStrokeData() = default;

#define GETTER(name) \
    decltype(QPsdVectorStrokeData::Private::name) QPsdVectorStrokeData::name() const { return d->name; }
FOREACH(GETTER)
#undef GETTER

QT_END_NAMESPACE

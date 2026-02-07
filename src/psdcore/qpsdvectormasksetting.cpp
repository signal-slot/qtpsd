// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdvectormasksetting.h"

#include <QtCore/QLoggingCategory>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQPsdVectorMaskSetting, "qt.psdcore.vmsk")

class QPsdVectorMaskSetting::Private : public QSharedData
{
public:
    Type type = Unknown;
    quint32 flags = 0;
    QList<PathInfo> subPathList;
    FillRule fillRule = Same;
    QRectF clipboardRect;
    qreal clipboardResolution = 0;
    FillRule initialFill = Same;

    static void checkPathType(PathInfo *pathInfo);
};

void QPsdVectorMaskSetting::Private::checkPathType(PathInfo *pathInfo)
{
    const auto subPath = pathInfo->subPath;

    const auto isRectangle = [&]() -> bool {
        if (subPath.length() != 4)
            return false;
        int i = 0;
        for (const auto &path : subPath) {
            if (path.anchor != path.preceding || path.anchor != path.leaving) {
                return false;
            }
            const auto previousPath = subPath.at((i + 7) % 4);
            if (path.anchor.x() != previousPath.anchor.x() && path.anchor.y() != previousPath.anchor.y()) {
                return false;
            }
            i++;
        }
        return true;
    };
    if (isRectangle()) {
        pathInfo->type = PathInfo::Rectangle;
        pathInfo->rect = QRectF(subPath.at(0).anchor, subPath.at(2).anchor);
        return;
    }

    const auto isCircle = [&]() -> bool {
        if (subPath.length() != 4)
            return false;
        const auto s0 = subPath.at(0);
        const auto s1 = subPath.at(1);
        const auto s2 = subPath.at(2);
        const auto s3 = subPath.at(3);
        return s0.anchor.x() == s2.anchor.x()
               && s0.anchor.y() == s0.leaving.y() && s0.anchor.y() == s0.preceding.y()
               && s0.preceding.x() == s2.leaving.x()
               && s0.leaving.x() == s2.preceding.x()
               && s1.anchor.x() == s1.preceding.x() && s1.anchor.x() == s1.leaving.x()
               && s1.anchor.y() == s3.anchor.y()
               && s1.preceding.y() == s3.leaving.y()
               && s1.leaving.y() == s3.preceding.y()
               && s2.anchor.y() == s2.preceding.y() && s2.anchor.y() == s2.leaving.y()
               && s3.anchor.x() == s3.preceding.x() && s3.anchor.x() == s3.leaving.x()
            ;
    };
    if (isCircle()) {
        pathInfo->type = PathInfo::Circle;
        pathInfo->rect.setTop(subPath.at(0).anchor.y());
        pathInfo->rect.setLeft(subPath.at(3).anchor.x());
        pathInfo->rect.setBottom(subPath.at(2).anchor.y());
        pathInfo->rect.setRight(subPath.at(1).anchor.x());
        return;
    }

    const auto isRoundedRectangle = [&]() -> bool {
        if (subPath.length() != 8)
            return false;
        int i = 0;
        for (const auto &path : subPath) {
            if (((path.anchor == path.preceding) == (i % 2 == 0))
                || ((path.anchor == path.leaving) != (i % 2 == 0))) {
                return false;
            }
            i++;
        }
        if (subPath.at(0).anchor.x() != subPath.at(5).anchor.x()
            || subPath.at(1).anchor.x() != subPath.at(4).anchor.x()
            || subPath.at(2).anchor.x() != subPath.at(3).anchor.x()
            || subPath.at(6).anchor.x() != subPath.at(7).anchor.x())
            return false;

        if (subPath.at(0).anchor.y() != subPath.at(1).anchor.y()
            || subPath.at(2).anchor.y() != subPath.at(7).anchor.y()
            || subPath.at(3).anchor.y() != subPath.at(6).anchor.y()
            || subPath.at(4).anchor.y() != subPath.at(5).anchor.y())
            return false;
        return true;
    };
    if (isRoundedRectangle()) {
        pathInfo->type = PathInfo::RoundedRectangle;
        pathInfo->rect = QRectF(QPointF(subPath.at(7).anchor.x(), subPath.at(0).anchor.y()), QPointF(subPath.at(3).anchor.x(), subPath.at(4).anchor.y()));
        pathInfo->radius = subPath.at(0).anchor.x() - subPath.at(7).anchor.x();
        return;
    }
}

QPsdVectorMaskSetting::QPsdVectorMaskSetting()
    : QPsdSection()
    , d(new Private)
{}

QPsdVectorMaskSetting::QPsdVectorMaskSetting(QIODevice *source, quint32 length)
    : QPsdVectorMaskSetting()
{
    // Version ( = 3 for Photoshop 6.0)
    const auto version = readU32(source, &length);
    Q_ASSERT(version == 3);

    // Flags. bit 1 = invert, bit 2 = not link, bit 3 = disable
    d->flags = readU32(source, &length);
    qCDebug(lcQPsdVectorMaskSetting) << "#############" << d->flags;

    EnsureSeek es(source, length);
    quint16 numberOfPoints = 0;
    PathInfo pathInfo;
    while (es.bytesAvailable() >= 26) {
        // TODO: https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_17587
        EnsureSeek es2(source, 26);
        const auto type = readU16(source, &length);
        BezierPath path;
        switch (type) {
        case 0:
            d->type = Closed;
            qCDebug(lcQPsdVectorMaskSetting) << "Closed";
            break;
        case 1:
            d->type = Closed;
            path.type = BezierPath::Linked;
            qCDebug(lcQPsdVectorMaskSetting) << "Closed" << "Linked";
            break;
        case 2:
            d->type = Closed;
            path.type = BezierPath::Unlinked;
            qCDebug(lcQPsdVectorMaskSetting) << "Closed" << "Unlinked";
            break;
        case 3:
            d->type = Open;
            qCDebug(lcQPsdVectorMaskSetting) << "Open";
            break;
        case 4:
            d->type = Open;
            path.type = BezierPath::Linked;
            qCDebug(lcQPsdVectorMaskSetting) << "Open" << "Linked";
            break;
        case 5:
            d->type = Open;
            path.type = BezierPath::Unlinked;
            qCDebug(lcQPsdVectorMaskSetting) << "Open" << "Unlinked";
            break;
        default:
            break;
        }

        // qCDebug(lcQPsdVectorMaskSetting) << type;
        switch (type) {
        case 0:   // Closed subpath length record
        case 3: { // Open subpath length record
            numberOfPoints = readU16(source, &length);
            qCDebug(lcQPsdVectorMaskSetting) << "nuberOfPoints" << numberOfPoints;
            // https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577409_17587
            // says "The remaining 24 bytes of the first record are zeroes." However...
            pathInfo.operation = static_cast<PathInfo::Operation>(readU16(source, &length));
            // Per-path fill rule: 2 = non-zero winding, otherwise even-odd
            const auto pathFlags = readU16(source, &length);
            pathInfo.fillRule = (pathFlags == 2) ? NonZero : EvenOdd;
            qCDebug(lcQPsdVectorMaskSetting) << "pathFlags" << pathFlags << "fillRule" << pathInfo.fillRule;
            break; }
        case 1:   // Closed subpath Bezier knot, linked
        case 2:   // Closed subpath Bezier knot, unlinked
        case 4:   // Open subpath Bezier knot, linked
        case 5: { // Open subpath Bezier knot, unlinked
            qCDebug(lcQPsdVectorMaskSetting) << pathInfo.subPath.length();
            const auto precedingVert = readPathNumber(source, &length);
            const auto precedingHriz = readPathNumber(source, &length);
            path.preceding = QPointF(precedingHriz, precedingVert);
            qCDebug(lcQPsdVectorMaskSetting) << path.preceding;
            const auto anchorVert = readPathNumber(source, &length);
            const auto anchorHriz = readPathNumber(source, &length);
            path.anchor = QPointF(anchorHriz, anchorVert);
            qCDebug(lcQPsdVectorMaskSetting) << path.anchor;
            const auto leavingVert = readPathNumber(source, &length);
            const auto leavingHriz = readPathNumber(source, &length);
            path.leaving = QPointF(leavingHriz, leavingVert);
            qCDebug(lcQPsdVectorMaskSetting) << path.leaving;
            pathInfo.subPath.append(path);
            if (pathInfo.subPath.length() == numberOfPoints) {
                Private::checkPathType(&pathInfo);
                d->subPathList.append(pathInfo);
                pathInfo.subPath.clear();
                numberOfPoints = 0;
            }
            break; }
        case 6: { // Path fill rule record
            d->fillRule = static_cast<FillRule>(readU16(source, &length));
            qCDebug(lcQPsdVectorMaskSetting) << "fillRule" << d->fillRule;
            break;}
        case 7: { // Clipboard record
            const auto clipboardTop = readPathNumber(source, &length);
            const auto clipboardLeft = readPathNumber(source, &length);
            const auto clipboardBottom = readPathNumber(source, &length);
            const auto clipboardRight = readPathNumber(source, &length);
            d->clipboardRect.setTop(clipboardTop);
            d->clipboardRect.setLeft(clipboardLeft);
            d->clipboardRect.setBottom(clipboardBottom - 1);
            d->clipboardRect.setRight(clipboardRight - 1);
            d->clipboardResolution = readPathNumber(source, &length);
            qCDebug(lcQPsdVectorMaskSetting) << d->clipboardRect << d->clipboardResolution;
            break; }
        case 8: { // Initial fill rule record
            d->initialFill = static_cast<FillRule>(readU16(source, &length));
            qCDebug(lcQPsdVectorMaskSetting) << "initialFill" << d->initialFill;
            break; }
        default:
            qWarning() << type << "not supporeted";
        }
        skip(source, es2.bytesAvailable(), &length);
    }
    if (pathInfo.subPath.length() == numberOfPoints) {
        if (numberOfPoints > 0) {
            Private::checkPathType(&pathInfo);
            d->subPathList.append(pathInfo);
        }
    } else {
        qWarning() << "pathInfo.subPath.length() != nuberOfPoints" << pathInfo.subPath.length() << numberOfPoints;
    }

    qCDebug(lcQPsdVectorMaskSetting);

    // not sure why
    skip(source, es.bytesAvailable(), &length);
}

QPsdVectorMaskSetting::QPsdVectorMaskSetting(const QPsdVectorMaskSetting &other)
    : QPsdSection(other)
    , d(other.d)
{}

QPsdVectorMaskSetting &QPsdVectorMaskSetting::operator=(const QPsdVectorMaskSetting &other)
{
    if (this != &other) {
        QPsdSection::operator=(other);
        d.operator=(other.d);
    }
    return *this;
}

QPsdVectorMaskSetting::~QPsdVectorMaskSetting() = default;

bool QPsdVectorMaskSetting::invert() const
{
    return d->flags & 0x01;
}

bool QPsdVectorMaskSetting::notLink() const
{
    return d->flags & 0x02;
}

bool QPsdVectorMaskSetting::disable() const
{
    return d->flags & 0x04;
}

QPsdVectorMaskSetting::Type QPsdVectorMaskSetting::type() const
{
    return d->type;
}

QList<QPsdVectorMaskSetting::PathInfo> QPsdVectorMaskSetting::subPathList() const
{
    return d->subPathList;
}

QRectF QPsdVectorMaskSetting::clipboardRect() const
{
    return d->clipboardRect;
}

qreal QPsdVectorMaskSetting::clipboardResolution() const
{
    return d->clipboardResolution;
}

QPsdVectorMaskSetting::FillRule QPsdVectorMaskSetting::initialFill() const
{
    return d->initialFill;
}

QPsdVectorMaskSetting::FillRule QPsdVectorMaskSetting::fillRule() const
{
    return d->fillRule;
}

QT_END_NAMESPACE

// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtCore/QCborMap>
#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtGui/QFontMetrics>
#include <QtGui/QGuiApplication>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QScreen>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtPsdCore/QPsdParser>
#include <QtPsdCore/QPsdFileHeader>
#include <QtPsdCore/QPsdLayerInfo>
#include <QtPsdCore/QPsdLayerAndMaskInformation>
#include <QtPsdCore/QPsdChannelImageData>
#include <QtPsdCore/QPsdSectionDividerSetting>
#include <QtPsdGui/QPsdAbstractLayerItem>
#include <QtPsdGui/QPsdFolderLayerItem>
#include <QtPsdGui/QPsdImageLayerItem>
#include <QtPsdGui/QPsdShapeLayerItem>
#include <QtPsdGui/QPsdTextLayerItem>
#include <QtPsdWidget/QPsdWidgetTreeItemModel>
#include <QtPsdExporter/QPsdExporterTreeItemModel>
#include <QtPsdImporter/QPsdImporterPlugin>

#include "fig_archive.h"
#include "fig_kiwi.h"
#include "fig_to_rest.h"

using namespace Qt::StringLiterals;

// ─── FigmaClient ───────────────────────────────────────────────────────────

class FigmaClient : public QObject
{
    Q_OBJECT
public:
    explicit FigmaClient(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_token = qEnvironmentVariable("FIGMA_API_KEY");
        if (m_token.isEmpty())
            m_token = qEnvironmentVariable("FIGMA_ACCESS_TOKEN");
    }

    void setToken(const QString &token) { m_token = token; }
    QString token() const { return m_token; }

    QJsonObject fetchFile(const QString &fileKey)
    {
        QUrl url(u"https://api.figma.com/v1/files/%1?geometry=paths"_s.arg(fileKey));
        return getJson(url);
    }

    QJsonObject fetchImages(const QString &fileKey, const QStringList &nodeIds,
                            const QString &format = u"png"_s, int scale = 2)
    {
        QString ids = nodeIds.join(","_L1);
        QUrl url(u"https://api.figma.com/v1/images/%1?ids=%2&format=%3&scale=%4"_s
                     .arg(fileKey, ids, format, QString::number(scale)));
        return getJson(url);
    }

    // Fetch actual image fill URLs (imageRef → URL mapping)
    QJsonObject fetchFileImages(const QString &fileKey)
    {
        QUrl url(u"https://api.figma.com/v1/files/%1/images"_s.arg(fileKey));
        return getJson(url);
    }

    QImage downloadImage(const QUrl &url)
    {
        QNetworkRequest request(url);
        auto *reply = m_nam.get(request);
        m_currentReply = reply;
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        m_currentReply = nullptr;

        QImage image;
        if (reply->error() == QNetworkReply::NoError) {
            image.loadFromData(reply->readAll());
        } else if (reply->error() != QNetworkReply::OperationCanceledError) {
            qWarning() << "Image download failed:" << reply->errorString();
        }
        reply->deleteLater();
        return image;
    }

public Q_SLOTS:
    // Abort the reply currently being awaited (if any). Safe to call from
    // anywhere; abort() emits finished() which unblocks the nested event loop.
    void abort()
    {
        if (m_currentReply)
            m_currentReply->abort();
    }

private:
    QJsonObject getJson(const QUrl &url)
    {
        QNetworkRequest request(url);
        request.setRawHeader("X-Figma-Token", m_token.toUtf8());
        auto *reply = m_nam.get(request);
        m_currentReply = reply;

        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        m_currentReply = nullptr;

        QJsonObject result;
        if (reply->error() == QNetworkReply::NoError) {
            result = QJsonDocument::fromJson(reply->readAll()).object();
        } else {
            result["error"_L1] = reply->errorString();
            const auto body = reply->readAll();
            if (!body.isEmpty()) {
                const auto errObj = QJsonDocument::fromJson(body).object();
                if (errObj.contains("err"_L1))
                    result["figmaError"_L1] = errObj["err"_L1];
            }
        }
        reply->deleteLater();
        return result;
    }

    QNetworkAccessManager m_nam;
    QString m_token;
    QPointer<QNetworkReply> m_currentReply;
};

// ─── FigmaImageCache ──────────────────────────────────────────────────────

class FigmaImageCache
{
public:
    FigmaImageCache(const QString &fileKey, const QString &lastModified)
    {
        const auto cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        m_baseDir = cacheRoot + "/figma/"_L1 + sanitize(fileKey);

        QDir dir(m_baseDir);
        dir.mkpath(u"nodes"_s);
        dir.mkpath(u"fills"_s);

        // Check version and invalidate nodes/ if the file was modified
        const auto versionFile = m_baseDir + "/version"_L1;
        QFile vf(versionFile);
        bool needsInvalidation = true;
        if (vf.open(QIODevice::ReadOnly)) {
            const auto cached = QString::fromUtf8(vf.readAll()).trimmed();
            vf.close();
            if (cached == lastModified)
                needsInvalidation = false;
        }

        if (needsInvalidation) {
            // Clear rendered node images (they depend on file version)
            QDir nodesDir(m_baseDir + "/nodes"_L1);
            const auto entries = nodesDir.entryList(QDir::Files);
            for (const auto &f : entries)
                nodesDir.remove(f);

            // Write new version stamp
            if (vf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                vf.write(lastModified.toUtf8());
                vf.close();
            }
        }
    }

    QImage nodeImage(const QString &nodeId, int scale) const
    {
        const auto path = m_baseDir + "/nodes/"_L1 + sanitize(nodeId)
                          + u"@%1x.png"_s.arg(scale);
        QImage img;
        if (QFile::exists(path))
            img.load(path);
        return img;
    }

    void saveNodeImage(const QString &nodeId, int scale, const QImage &image)
    {
        const auto path = m_baseDir + "/nodes/"_L1 + sanitize(nodeId)
                          + u"@%1x.png"_s.arg(scale);
        image.save(path);
    }

    QImage fillImage(const QString &imageRef) const
    {
        const auto path = m_baseDir + "/fills/"_L1 + sanitize(imageRef) + u".png"_s;
        QImage img;
        if (QFile::exists(path))
            img.load(path);
        return img;
    }

    void saveFillImage(const QString &imageRef, const QImage &image)
    {
        const auto path = m_baseDir + "/fills/"_L1 + sanitize(imageRef) + u".png"_s;
        image.save(path);
    }

private:
    static QString sanitize(const QString &id)
    {
        QString s = id;
        s.replace(u':', u'_');
        s.replace(u'/', u'_');
        return s;
    }

    QString m_baseDir;
};

// ─── SVG Path Parser ───────────────────────────────────────────────────────

static void arcToBezier(QPainterPath &path, qreal x1, qreal y1,
                        qreal rx, qreal ry, qreal xRotation,
                        int largeArcFlag, int sweepFlag,
                        qreal x2, qreal y2)
{
    if (qFuzzyCompare(x1, x2) && qFuzzyCompare(y1, y2))
        return;

    if (qFuzzyIsNull(rx) || qFuzzyIsNull(ry)) {
        path.lineTo(x2, y2);
        return;
    }

    rx = qAbs(rx);
    ry = qAbs(ry);

    const qreal phi = xRotation * M_PI / 180.0;
    const qreal cosPhi = std::cos(phi);
    const qreal sinPhi = std::sin(phi);

    const qreal dx = (x1 - x2) / 2.0;
    const qreal dy = (y1 - y2) / 2.0;
    const qreal x1p = cosPhi * dx + sinPhi * dy;
    const qreal y1p = -sinPhi * dx + cosPhi * dy;

    qreal rxSq = rx * rx;
    qreal rySq = ry * ry;
    const qreal x1pSq = x1p * x1p;
    const qreal y1pSq = y1p * y1p;

    qreal lambda = x1pSq / rxSq + y1pSq / rySq;
    if (lambda > 1.0) {
        const qreal lambdaSqrt = std::sqrt(lambda);
        rx *= lambdaSqrt;
        ry *= lambdaSqrt;
        rxSq = rx * rx;
        rySq = ry * ry;
    }

    qreal num = rxSq * rySq - rxSq * y1pSq - rySq * x1pSq;
    qreal den = rxSq * y1pSq + rySq * x1pSq;
    qreal sq = qMax(0.0, num / den);
    qreal root = std::sqrt(sq);
    if (largeArcFlag == sweepFlag)
        root = -root;

    const qreal cxp = root * rx * y1p / ry;
    const qreal cyp = -root * ry * x1p / rx;

    const qreal cx = cosPhi * cxp - sinPhi * cyp + (x1 + x2) / 2.0;
    const qreal cy = sinPhi * cxp + cosPhi * cyp + (y1 + y2) / 2.0;

    auto vectorAngle = [](qreal ux, qreal uy, qreal vx, qreal vy) -> qreal {
        const qreal dot = ux * vx + uy * vy;
        const qreal len = std::sqrt(ux * ux + uy * uy) * std::sqrt(vx * vx + vy * vy);
        qreal ang = std::acos(qBound(-1.0, dot / len, 1.0));
        if (ux * vy - uy * vx < 0)
            ang = -ang;
        return ang;
    };

    const qreal theta1 = vectorAngle(1.0, 0.0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    qreal dtheta = vectorAngle((x1p - cxp) / rx, (y1p - cyp) / ry,
                               (-x1p - cxp) / rx, (-y1p - cyp) / ry);

    if (sweepFlag == 0 && dtheta > 0)
        dtheta -= 2.0 * M_PI;
    else if (sweepFlag == 1 && dtheta < 0)
        dtheta += 2.0 * M_PI;

    int segments = qCeil(qAbs(dtheta) / (M_PI / 2.0));
    if (segments == 0) segments = 1;
    const qreal segAngle = dtheta / segments;

    for (int s = 0; s < segments; ++s) {
        const qreal t1 = theta1 + s * segAngle;
        const qreal t2 = theta1 + (s + 1) * segAngle;

        const qreal alpha = std::sin(segAngle)
            * (std::sqrt(4.0 + 3.0 * std::pow(std::tan(segAngle / 2.0), 2)) - 1.0) / 3.0;

        const qreal cosT1 = std::cos(t1), sinT1 = std::sin(t1);
        const qreal cosT2 = std::cos(t2), sinT2 = std::sin(t2);

        const qreal cp1x = cosT1 - alpha * sinT1;
        const qreal cp1y = sinT1 + alpha * cosT1;
        const qreal cp2x = cosT2 + alpha * sinT2;
        const qreal cp2y = sinT2 - alpha * cosT2;

        auto transform = [&](qreal px, qreal py) -> QPointF {
            return QPointF(cosPhi * rx * px - sinPhi * ry * py + cx,
                           sinPhi * rx * px + cosPhi * ry * py + cy);
        };

        const QPointF c1 = transform(cp1x, cp1y);
        const QPointF c2 = transform(cp2x, cp2y);
        const QPointF ep = transform(cosT2, sinT2);

        path.cubicTo(c1, c2, ep);
    }
}

// Build a PathInfo from per-corner radii using CSS-style proportional scaling.
// When adjacent corner radii exceed the side length, all radii are scaled down
// by a single factor so that they fit (CSS Backgrounds Level 3 §5.5).
static QPsdAbstractLayerItem::PathInfo pathInfoFromCornerRadii(
    qreal w, qreal h,
    qreal rawTl, qreal rawTr, qreal rawBr, qreal rawBl,
    qreal uniformRadius = 0)
{
    QPsdAbstractLayerItem::PathInfo pi;
    pi.rect = QRectF(0, 0, w, h);

    const bool hasPerCorner = (rawTl > 0 || rawTr > 0 || rawBr > 0 || rawBl > 0);
    if (hasPerCorner) {
        // CSS-style proportional scaling
        qreal scale = 1.0;
        if (rawTl + rawTr > 0) scale = qMin(scale, w / (rawTl + rawTr));
        if (rawTr + rawBr > 0) scale = qMin(scale, h / (rawTr + rawBr));
        if (rawBr + rawBl > 0) scale = qMin(scale, w / (rawBr + rawBl));
        if (rawBl + rawTl > 0) scale = qMin(scale, h / (rawBl + rawTl));

        const qreal tl = rawTl * scale;
        const qreal tr = rawTr * scale;
        const qreal br = rawBr * scale;
        const qreal bl = rawBl * scale;

        if (tl != tr || tr != br || br != bl) {
            pi.type = QPsdAbstractLayerItem::PathInfo::Path;
            QPainterPath pp;
            pp.moveTo(tl, 0);
            pp.lineTo(w - tr, 0);
            if (tr > 0) pp.arcTo(w - 2 * tr, 0, 2 * tr, 2 * tr, 90, -90);
            pp.lineTo(w, h - br);
            if (br > 0) pp.arcTo(w - 2 * br, h - 2 * br, 2 * br, 2 * br, 0, -90);
            pp.lineTo(bl, h);
            if (bl > 0) pp.arcTo(0, h - 2 * bl, 2 * bl, 2 * bl, 270, -90);
            pp.lineTo(0, tl);
            if (tl > 0) pp.arcTo(0, 0, 2 * tl, 2 * tl, 180, -90);
            pp.closeSubpath();
            pi.path = pp;
        } else if (tl > 0) {
            pi.type = QPsdAbstractLayerItem::PathInfo::RoundedRectangle;
            pi.radius = tl;
        } else {
            pi.type = QPsdAbstractLayerItem::PathInfo::Rectangle;
        }
    } else if (uniformRadius > 0) {
        const qreal maxR = qMin(w, h) / 2.0;
        pi.type = QPsdAbstractLayerItem::PathInfo::RoundedRectangle;
        pi.radius = qMin(uniformRadius, maxR);
    } else {
        pi.type = QPsdAbstractLayerItem::PathInfo::Rectangle;
    }
    return pi;
}

static QPainterPath parseSvgPath(const QString &pathData)
{
    QPainterPath path;
    int i = 0;
    const int len = pathData.length();
    QPointF currentPos;
    QPointF lastControlPoint;
    QChar lastCommand;

    auto skipSpacesAndCommas = [&]() {
        while (i < len && (pathData[i].isSpace() || pathData[i] == ','_L1))
            ++i;
    };

    auto parseNumber = [&]() -> qreal {
        skipSpacesAndCommas();
        int start = i;
        if (i < len && (pathData[i] == '-'_L1 || pathData[i] == '+'_L1))
            ++i;
        while (i < len && (pathData[i].isDigit() || pathData[i] == '.'_L1))
            ++i;
        if (i < len && (pathData[i] == 'e'_L1 || pathData[i] == 'E'_L1)) {
            ++i;
            if (i < len && (pathData[i] == '-'_L1 || pathData[i] == '+'_L1))
                ++i;
            while (i < len && pathData[i].isDigit())
                ++i;
        }
        return pathData.mid(start, i - start).toDouble();
    };

    auto hasMoreNumbers = [&]() -> bool {
        int j = i;
        while (j < len && (pathData[j].isSpace() || pathData[j] == ','_L1))
            ++j;
        if (j >= len) return false;
        QChar c = pathData[j];
        return c.isDigit() || c == '-'_L1 || c == '+'_L1 || c == '.'_L1;
    };

    while (i < len) {
        skipSpacesAndCommas();
        if (i >= len) break;

        QChar cmd = pathData[i];
        if (!cmd.isLetter()) { ++i; continue; }
        ++i;

        switch (cmd.unicode()) {
        case 'M': {
            qreal x = parseNumber();
            qreal y = parseNumber();
            path.moveTo(x, y);
            currentPos = QPointF(x, y);
            while (hasMoreNumbers()) {
                x = parseNumber();
                y = parseNumber();
                path.lineTo(x, y);
                currentPos = QPointF(x, y);
            }
            break;
        }
        case 'm': {
            qreal dx = parseNumber();
            qreal dy = parseNumber();
            currentPos += QPointF(dx, dy);
            path.moveTo(currentPos);
            while (hasMoreNumbers()) {
                dx = parseNumber();
                dy = parseNumber();
                currentPos += QPointF(dx, dy);
                path.lineTo(currentPos);
            }
            break;
        }
        case 'L': {
            do {
                qreal x = parseNumber();
                qreal y = parseNumber();
                path.lineTo(x, y);
                currentPos = QPointF(x, y);
            } while (hasMoreNumbers());
            break;
        }
        case 'l': {
            do {
                qreal dx = parseNumber();
                qreal dy = parseNumber();
                currentPos += QPointF(dx, dy);
                path.lineTo(currentPos);
            } while (hasMoreNumbers());
            break;
        }
        case 'H': {
            do {
                qreal x = parseNumber();
                currentPos.setX(x);
                path.lineTo(currentPos);
            } while (hasMoreNumbers());
            break;
        }
        case 'h': {
            do {
                qreal dx = parseNumber();
                currentPos.rx() += dx;
                path.lineTo(currentPos);
            } while (hasMoreNumbers());
            break;
        }
        case 'V': {
            do {
                qreal y = parseNumber();
                currentPos.setY(y);
                path.lineTo(currentPos);
            } while (hasMoreNumbers());
            break;
        }
        case 'v': {
            do {
                qreal dy = parseNumber();
                currentPos.ry() += dy;
                path.lineTo(currentPos);
            } while (hasMoreNumbers());
            break;
        }
        case 'C': {
            do {
                qreal x1 = parseNumber(), y1 = parseNumber();
                qreal x2 = parseNumber(), y2 = parseNumber();
                qreal x = parseNumber(), y = parseNumber();
                path.cubicTo(x1, y1, x2, y2, x, y);
                lastControlPoint = QPointF(x2, y2);
                currentPos = QPointF(x, y);
            } while (hasMoreNumbers());
            break;
        }
        case 'c': {
            do {
                qreal dx1 = parseNumber(), dy1 = parseNumber();
                qreal dx2 = parseNumber(), dy2 = parseNumber();
                qreal dx = parseNumber(), dy = parseNumber();
                QPointF cp2(currentPos.x() + dx2, currentPos.y() + dy2);
                path.cubicTo(currentPos.x() + dx1, currentPos.y() + dy1,
                             cp2.x(), cp2.y(),
                             currentPos.x() + dx, currentPos.y() + dy);
                lastControlPoint = cp2;
                currentPos += QPointF(dx, dy);
            } while (hasMoreNumbers());
            break;
        }
        case 'Q': {
            do {
                qreal x1 = parseNumber(), y1 = parseNumber();
                qreal x = parseNumber(), y = parseNumber();
                path.quadTo(x1, y1, x, y);
                lastControlPoint = QPointF(x1, y1);
                currentPos = QPointF(x, y);
            } while (hasMoreNumbers());
            break;
        }
        case 'q': {
            do {
                qreal dx1 = parseNumber(), dy1 = parseNumber();
                qreal dx = parseNumber(), dy = parseNumber();
                QPointF cp(currentPos.x() + dx1, currentPos.y() + dy1);
                path.quadTo(cp.x(), cp.y(),
                            currentPos.x() + dx, currentPos.y() + dy);
                lastControlPoint = cp;
                currentPos += QPointF(dx, dy);
            } while (hasMoreNumbers());
            break;
        }
        case 'S': {
            do {
                QPointF cp1 = currentPos;
                if (lastCommand == 'C' || lastCommand == 'c' || lastCommand == 'S' || lastCommand == 's')
                    cp1 = 2.0 * currentPos - lastControlPoint;
                qreal x2 = parseNumber(), y2 = parseNumber();
                qreal x = parseNumber(), y = parseNumber();
                path.cubicTo(cp1.x(), cp1.y(), x2, y2, x, y);
                lastControlPoint = QPointF(x2, y2);
                currentPos = QPointF(x, y);
            } while (hasMoreNumbers());
            break;
        }
        case 's': {
            do {
                QPointF cp1 = currentPos;
                if (lastCommand == 'C' || lastCommand == 'c' || lastCommand == 'S' || lastCommand == 's')
                    cp1 = 2.0 * currentPos - lastControlPoint;
                qreal dx2 = parseNumber(), dy2 = parseNumber();
                qreal dx = parseNumber(), dy = parseNumber();
                QPointF cp2(currentPos.x() + dx2, currentPos.y() + dy2);
                path.cubicTo(cp1.x(), cp1.y(), cp2.x(), cp2.y(),
                             currentPos.x() + dx, currentPos.y() + dy);
                lastControlPoint = cp2;
                currentPos += QPointF(dx, dy);
            } while (hasMoreNumbers());
            break;
        }
        case 'T': {
            do {
                QPointF cp = currentPos;
                if (lastCommand == 'Q' || lastCommand == 'q' || lastCommand == 'T' || lastCommand == 't')
                    cp = 2.0 * currentPos - lastControlPoint;
                qreal x = parseNumber(), y = parseNumber();
                path.quadTo(cp.x(), cp.y(), x, y);
                lastControlPoint = cp;
                currentPos = QPointF(x, y);
            } while (hasMoreNumbers());
            break;
        }
        case 't': {
            do {
                QPointF cp = currentPos;
                if (lastCommand == 'Q' || lastCommand == 'q' || lastCommand == 'T' || lastCommand == 't')
                    cp = 2.0 * currentPos - lastControlPoint;
                qreal dx = parseNumber(), dy = parseNumber();
                path.quadTo(cp.x(), cp.y(), currentPos.x() + dx, currentPos.y() + dy);
                lastControlPoint = cp;
                currentPos += QPointF(dx, dy);
            } while (hasMoreNumbers());
            break;
        }
        case 'A': {
            do {
                qreal rx = parseNumber(), ry = parseNumber();
                qreal xRotation = parseNumber();
                int largeArcFlag = qRound(parseNumber());
                int sweepFlag = qRound(parseNumber());
                qreal x = parseNumber(), y = parseNumber();
                arcToBezier(path, currentPos.x(), currentPos.y(),
                            rx, ry, xRotation, largeArcFlag, sweepFlag, x, y);
                currentPos = QPointF(x, y);
            } while (hasMoreNumbers());
            break;
        }
        case 'a': {
            do {
                qreal rx = parseNumber(), ry = parseNumber();
                qreal xRotation = parseNumber();
                int largeArcFlag = qRound(parseNumber());
                int sweepFlag = qRound(parseNumber());
                qreal dx = parseNumber(), dy = parseNumber();
                qreal x = currentPos.x() + dx, y = currentPos.y() + dy;
                arcToBezier(path, currentPos.x(), currentPos.y(),
                            rx, ry, xRotation, largeArcFlag, sweepFlag, x, y);
                currentPos = QPointF(x, y);
            } while (hasMoreNumbers());
            break;
        }
        case 'Z':
        case 'z':
            path.closeSubpath();
            currentPos = path.currentPosition();
            break;
        default:
            break;
        }
        lastCommand = cmd;
    }

    return path;
}

// ─── FigmaLayerTreeItemModel ──────────────────────────────────────────────

class FigmaLayerTreeItemModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum Roles {
        LayerIdRole = Qt::UserRole + 1,
        NameRole,
        RectRole,
        LayerRecordObjectRole,  // placeholder — matches QPsdLayerTreeItemModel
        FolderTypeRole,
        GroupIndexesRole,
        ClippingMaskIndexRole,
        LayerItemObjectRole,
        BlendModeRole,
        IsMaskRole,
    };

    enum class FolderType {
        NotFolder = 0,
        OpenFolder,
        ClosedFolder,
    };

    explicit FigmaLayerTreeItemModel(QObject *parent = nullptr)
        : QAbstractItemModel(parent)
    {}

    ~FigmaLayerTreeItemModel() override
    {
        qDeleteAll(m_layerItems);
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {
            {LayerIdRole, "LayerId"},
            {NameRole, "Name"},
            {RectRole, "Rect"},
            {FolderTypeRole, "FolderType"},
            {GroupIndexesRole, "GroupIndexes"},
            {ClippingMaskIndexRole, "ClippingMaskIndex"},
            {LayerItemObjectRole, "LayerItemObject"},
            {BlendModeRole, "BlendMode"},
        };
    }

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override
    {
        if (!hasIndex(row, column, parent))
            return {};
        const auto &children = parent.isValid()
            ? m_nodes[parent.internalId()].childIds
            : m_rootIds;
        if (row >= children.size())
            return {};
        return createIndex(row, column, children.at(row));
    }

    QModelIndex parent(const QModelIndex &index) const override
    {
        if (!index.isValid())
            return {};
        quintptr id = index.internalId();
        if (!m_nodes.contains(id))
            return {};
        quintptr parentId = m_nodes[id].parentId;
        if (parentId == 0)
            return {};
        quintptr grandParentId = m_nodes[parentId].parentId;
        const auto &siblings = grandParentId == 0
            ? m_rootIds
            : m_nodes[grandParentId].childIds;
        int row = siblings.indexOf(parentId);
        if (row < 0)
            return {};
        return createIndex(row, 0, parentId);
    }

    int rowCount(const QModelIndex &parent = {}) const override
    {
        if (parent.column() > 0)
            return 0;
        if (!parent.isValid())
            return m_rootIds.size();
        quintptr id = parent.internalId();
        if (!m_nodes.contains(id))
            return 0;
        return m_nodes[id].childIds.size();
    }

    int columnCount(const QModelIndex & = {}) const override { return 1; }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid())
            return {};
        quintptr id = index.internalId();
        if (!m_nodes.contains(id))
            return {};
        const auto &node = m_nodes[id];

        switch (role) {
        case Qt::DisplayRole:
        case NameRole:
            return node.name;
        case LayerIdRole:
            return QVariant::fromValue(node.layerId);
        case RectRole:
            return node.rect;
        case FolderTypeRole:
            return QVariant::fromValue(node.folderType);
        case LayerItemObjectRole:
            return QVariant::fromValue(node.layerItem);
        case GroupIndexesRole: {
            QList<QPersistentModelIndex> groups;
            quintptr pid = node.parentId;
            while (pid != 0 && m_nodes.contains(pid)) {
                const auto &parentSiblings = m_nodes[pid].parentId == 0
                    ? m_rootIds : m_nodes[m_nodes[pid].parentId].childIds;
                int row = parentSiblings.indexOf(pid);
                if (row >= 0)
                    groups.prepend(createIndex(row, 0, pid));
                pid = m_nodes[pid].parentId;
            }
            return QVariant::fromValue(groups);
        }
        case ClippingMaskIndexRole:
            return QVariant::fromValue(QPersistentModelIndex());
        case BlendModeRole:
            return QVariant::fromValue(static_cast<int>(node.blendMode));
        case IsMaskRole:
            return node.isMask;
        }
        return {};
    }

    QSize size() const { return m_size; }
    QColor canvasColor() const { return m_canvasColor; }

    struct ArtboardInfo {
        QString figmaId;
        QString name;
        QRect rect;
    };
    QList<ArtboardInfo> artboards() const { return m_artboards; }

    void buildFromFigmaJson(const QJsonObject &fileJson, const QString &artboardId = {}, int pageIndex = 0)
    {
        beginResetModel();

        qDeleteAll(m_layerItems);
        m_layerItems.clear();
        m_nodes.clear();
        m_rootIds.clear();
        m_nextId = 1;
        m_canvasOrigin = QPoint();
        m_nodeIdMap.clear();
        m_artboards.clear();

        const auto document = fileJson["document"_L1].toObject();
        const auto children = document["children"_L1].toArray();
        if (children.isEmpty()) {
            endResetModel();
            return;
        }

        // Select page by index
        const auto page = children.at(qBound(0, pageIndex, children.size() - 1)).toObject();
        const auto pageChildren = page["children"_L1].toArray();

        // Extract page background color
        const auto bgColor = page["backgroundColor"_L1].toObject();
        if (!bgColor.isEmpty()) {
            m_canvasColor = QColor::fromRgbF(
                bgColor["r"_L1].toDouble(),
                bgColor["g"_L1].toDouble(),
                bgColor["b"_L1].toDouble(),
                bgColor["a"_L1].toDouble(1.0));
        }

        QRect canvasRect;
        for (const auto &child : pageChildren) {
            const auto obj = child.toObject();
            const auto bbox = obj["absoluteBoundingBox"_L1].toObject();
            QRect r(qRound(bbox["x"_L1].toDouble()), qRound(bbox["y"_L1].toDouble()),
                    qRound(bbox["width"_L1].toDouble()), qRound(bbox["height"_L1].toDouble()));
            canvasRect = canvasRect.united(r);

            const auto type = obj["type"_L1].toString();
            if (type == "FRAME"_L1 || type == "COMPONENT"_L1) {
                m_artboards.append({obj["id"_L1].toString(), obj["name"_L1].toString(), r});
            }
        }

        m_fullCanvasSize = canvasRect.isEmpty() ? QSize(800, 600) : canvasRect.size();
        m_fullCanvasOrigin = canvasRect.topLeft();

        bool artboardFound = false;
        if (!artboardId.isEmpty()) {
            for (const auto &ab : m_artboards) {
                if (ab.figmaId == artboardId) {
                    m_canvasOrigin = ab.rect.topLeft();
                    m_size = ab.rect.size();
                    artboardFound = true;
                    break;
                }
            }
        }
        if (!artboardFound) {
            m_canvasOrigin = m_fullCanvasOrigin;
            m_size = m_fullCanvasSize;
        }

        QString activeArtboardId;
        if (artboardFound) {
            activeArtboardId = artboardId;
        }

        for (int ci = pageChildren.size() - 1; ci >= 0; --ci) {
            const auto childObj = pageChildren.at(ci).toObject();
            const auto childId = childObj["id"_L1].toString();
            const auto childType = childObj["type"_L1].toString();

            if (!activeArtboardId.isEmpty()
                && (childType == "FRAME"_L1 || childType == "COMPONENT"_L1)
                && childId != activeArtboardId) {
                continue;
            }

            quintptr nodeId = processNode(childObj, 0);
            if (nodeId != 0)
                m_rootIds.append(nodeId);
        }

        endResetModel();
    }

    QString figmaNodeId(quintptr internalId) const
    {
        if (m_nodes.contains(internalId))
            return m_nodes[internalId].figmaId;
        return {};
    }

    QJsonArray nodeFills(const QString &figmaId) const
    {
        quintptr id = m_nodeIdMap.value(figmaId, 0);
        if (id != 0 && m_nodes.contains(id))
            return m_nodes[id].fills;
        return {};
    }

    quintptr findByFigmaId(const QString &figmaId) const
    {
        return m_nodeIdMap.value(figmaId, 0);
    }

    QModelIndex indexFromFigmaId(const QString &figmaId) const
    {
        quintptr id = findByFigmaId(figmaId);
        if (id == 0) return {};
        quintptr parentId = m_nodes[id].parentId;
        const auto &siblings = parentId == 0 ? m_rootIds : m_nodes[parentId].childIds;
        int row = siblings.indexOf(id);
        if (row < 0) return {};
        return createIndex(row, 0, id);
    }

    // Returns node IDs for regular image nodes (NOT _imgbg) — these use the render API
    QStringList imageNodeIds() const
    {
        QStringList ids;
        QSet<QString> seen;
        for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
            if (it->layerItem && it->layerItem->type() == QPsdAbstractLayerItem::Image) {
                if (it->figmaId.endsWith("_imgbg"_L1))
                    continue;  // _imgbg nodes use the file images API
                if (!seen.contains(it->figmaId)) {
                    ids.append(it->figmaId);
                    seen.insert(it->figmaId);
                }
            }
        }
        return ids;
    }

    // Returns imageRef → list of figmaIds mapping for _imgbg nodes — these use the file images API
    // Multiple _imgbg nodes may share the same imageRef (e.g. component instances)
    QHash<QString, QStringList> imageFillRefs() const
    {
        QHash<QString, QStringList> refs;
        for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
            if (!it->imageRef.isEmpty() && it->figmaId.endsWith("_imgbg"_L1)) {
                refs[it->imageRef].append(it->figmaId);
            }
        }
        return refs;
    }

    void setNodeImage(const QString &figmaId, const QImage &image)
    {
        quintptr id = findByFigmaId(figmaId);
        if (id != 0) {
            auto &node = m_nodes[id];
            if (node.layerItem && node.layerItem->type() == QPsdAbstractLayerItem::Image) {
                static_cast<QPsdImageLayerItem *>(node.layerItem)->setImage(image);
                return;
            }
        }
        // Fallback: try _imgbg suffix (for backward compat with render API results)
        quintptr bgId = findByFigmaId(figmaId + "_imgbg"_L1);
        if (bgId != 0) {
            auto &bgNode = m_nodes[bgId];
            if (bgNode.layerItem && bgNode.layerItem->type() == QPsdAbstractLayerItem::Image) {
                static_cast<QPsdImageLayerItem *>(bgNode.layerItem)->setImage(image);
            }
        }
    }

private:
    struct Node {
        quintptr id = 0;
        quintptr parentId = 0;
        QString figmaId;
        QString name;
        quint32 layerId = 0;
        QRect rect;
        FolderType folderType = FolderType::NotFolder;
        QPsdBlend::Mode blendMode = QPsdBlend::Normal;
        QPsdAbstractLayerItem *layerItem = nullptr;
        QList<quintptr> childIds;
        QJsonArray fills;
        bool isMask = false;
        QString imageRef;  // For _imgbg nodes: the Figma imageRef of the IMAGE fill
    };

    quintptr processNode(const QJsonObject &nodeJson, quintptr parentId, QPoint parentOffset = {})
    {
        const QString figmaId = nodeJson["id"_L1].toString();
        const QString name = nodeJson["name"_L1].toString();
        const QString nodeType = nodeJson["type"_L1].toString();
        const bool visible = nodeJson.value("visible"_L1).toBool(true);
        const qreal opacity = nodeJson.value("opacity"_L1).toDouble(1.0);

        if (!visible) return 0;

        quint32 layerId = qHash(figmaId);

        // Get bounding box (absolute coordinates from REST API), normalized to canvas origin
        // Then subtract parentOffset so coordinates are relative to the parent folder
        const auto bbox = nodeJson["absoluteBoundingBox"_L1].toObject();
        const int absX = qRound(bbox["x"_L1].toDouble()) - m_canvasOrigin.x();
        const int absY = qRound(bbox["y"_L1].toDouble()) - m_canvasOrigin.y();
        QRect rect(absX - parentOffset.x(), absY - parentOffset.y(),
                   qRound(bbox["width"_L1].toDouble()), qRound(bbox["height"_L1].toDouble()));

        const auto blendMode = blendModeFromFigma(nodeJson.value("blendMode"_L1).toString());

        quintptr nodeId = m_nextId++;
        Node node;
        node.id = nodeId;
        node.parentId = parentId;
        node.figmaId = figmaId;
        node.name = name;
        node.layerId = layerId;
        node.rect = rect;
        node.blendMode = blendMode;
        node.isMask = nodeJson.value("isMask"_L1).toBool(false);

        static const QSet<QString> folderTypes = {
            "FRAME"_L1, "GROUP"_L1, "COMPONENT"_L1, "INSTANCE"_L1,
            "SECTION"_L1, "COMPONENT_SET"_L1
        };
        static const QSet<QString> shapeTypes = {
            "RECTANGLE"_L1, "ELLIPSE"_L1, "VECTOR"_L1, "LINE"_L1,
            "STAR"_L1, "POLYGON"_L1, "BOOLEAN_OPERATION"_L1, "REGULAR_POLYGON"_L1
        };

        bool hasImageFill = false;
        const auto fills = nodeJson["fills"_L1].toArray();
        node.fills = fills;
        for (const auto &fill : fills) {
            if (fill.toObject()["type"_L1].toString() == "IMAGE"_L1) {
                hasImageFill = true;
                break;
            }
        }

        if (nodeType == "TEXT"_L1) {
            auto *textItem = new QPsdTextLayerItem();
            textItem->setId(layerId);
            textItem->setName(name);
            textItem->setVisible(visible);
            textItem->setOpacity(opacity);
            textItem->setRect(rect);

            QList<QPsdTextLayerItem::Run> runs;
            auto characters = nodeJson["characters"_L1].toString();
            // Figma uses U+2028 (LINE SEPARATOR) and U+2029 (PARAGRAPH SEPARATOR)
            // for line breaks; normalize to \n for QPsdTextItem which splits on \n.
            characters.replace(QChar(0x2028), QChar('\n'));
            characters.replace(QChar(0x2029), QChar('\n'));
            const auto style = nodeJson["style"_L1].toObject();

            // QPsdTextItem divides lineHeight by dpiScale, but Figma's lineHeightPx
            // is already in actual pixels. Pre-multiply to counteract the division.
            const qreal dpiScale = QGuiApplication::primaryScreen()->logicalDotsPerInchY() / 72.0;

            auto applyTextCase = [](QString text, const QString &textCase) -> QString {
                if (textCase == "UPPER"_L1) return text.toUpper();
                if (textCase == "LOWER"_L1) return text.toLower();
                if (textCase == "TITLE"_L1) {
                    bool capitalizeNext = true;
                    for (int i = 0; i < text.size(); ++i) {
                        if (text[i].isSpace()) {
                            capitalizeNext = true;
                        } else if (capitalizeNext) {
                            text[i] = text[i].toUpper();
                            capitalizeNext = false;
                        } else {
                            text[i] = text[i].toLower();
                        }
                    }
                }
                return text;
            };

            // Prefer styledTextSegments (more reliable per-segment fills/styles)
            const auto segments = nodeJson["styledTextSegments"_L1].toArray();
            if (!segments.isEmpty()) {
                const QString nodeTextCase = style["textCase"_L1].toString();
                for (const auto &seg : segments) {
                    const auto segObj = seg.toObject();
                    QPsdTextLayerItem::Run run;
                    const auto segFills = segObj["fills"_L1].toArray();
                    const QString segTextCase = segObj.contains("textCase"_L1)
                        ? segObj["textCase"_L1].toString() : nodeTextCase;
                    run.text = applyTextCase(segObj["characters"_L1].toString(), segTextCase);
                    run.font = fontFromStyle(segObj);
                    if (segTextCase == "SMALL_CAPS"_L1) {
                        run.font.setCapitalization(QFont::SmallCaps);
                        run.fontCaps = 1;
                    } else if (segTextCase == "UPPER"_L1) {
                        run.fontCaps = 2;
                    }
                    run.originalFontName = segObj["fontFamily"_L1].toString();
                    run.color = !segFills.isEmpty() ? colorFromFills(segFills) : colorFromFills(fills);
                    run.alignment = alignmentFromStyle(style);
                    // Parse lineHeight: styledTextSegments uses object format {value, unit}
                    if (segObj.contains("lineHeight"_L1)) {
                        const auto lh = segObj["lineHeight"_L1];
                        if (lh.isObject()) {
                            const auto lhObj = lh.toObject();
                            const auto unit = lhObj["unit"_L1].toString();
                            if (unit == "PIXELS"_L1)
                                run.lineHeight = lhObj["value"_L1].toDouble() * dpiScale;
                            else if (unit == "PERCENT"_L1 || unit == "FONT_SIZE_%"_L1)
                                run.lineHeight = run.font.pointSizeF() * lhObj["value"_L1].toDouble() / 100.0 * dpiScale;
                        } else if (lh.isDouble()) {
                            run.lineHeight = lh.toDouble() * dpiScale;
                        }
                    } else if (segObj.contains("lineHeightPx"_L1)) {
                        run.lineHeight = segObj["lineHeightPx"_L1].toDouble() * dpiScale;
                    }
                    runs.append(run);
                }
                // Apply line height from top-level style if segments don't include it
                if (style.contains("lineHeightPx"_L1)
                    && style["lineHeightUnit"_L1].toString() != "INTRINSIC_%"_L1) {
                    const qreal topLevelLineHeight = style["lineHeightPx"_L1].toDouble() * dpiScale;
                    for (auto &run : runs) {
                        if (run.lineHeight < 0)
                            run.lineHeight = topLevelLineHeight;
                    }
                }
                // Merge adjacent runs with identical styling
                QList<QPsdTextLayerItem::Run> mergedRuns;
                for (const auto &run : runs) {
                    if (!mergedRuns.isEmpty() &&
                        mergedRuns.last().font == run.font &&
                        mergedRuns.last().color == run.color &&
                        mergedRuns.last().alignment == run.alignment &&
                        qFuzzyCompare(mergedRuns.last().lineHeight, run.lineHeight)) {
                        mergedRuns.last().text += run.text;
                    } else {
                        mergedRuns.append(run);
                    }
                }
                runs = mergedRuns;
            } else {
                // Fallback to characterStyleOverrides
                const auto overrides = nodeJson["characterStyleOverrides"_L1].toArray();
                const auto overrideTable = nodeJson["styleOverrideTable"_L1].toObject();

                if (overrides.isEmpty() || characters.isEmpty()) {
                    QPsdTextLayerItem::Run run;
                    const QString tc = style["textCase"_L1].toString();
                    run.text = applyTextCase(characters, tc);
                    run.font = fontFromStyle(style);
                    if (tc == "SMALL_CAPS"_L1) {
                        run.font.setCapitalization(QFont::SmallCaps);
                        run.fontCaps = 1;
                    } else if (tc == "UPPER"_L1) {
                        run.fontCaps = 2;
                    }
                    run.originalFontName = style["fontFamily"_L1].toString();
                    run.color = colorFromFills(fills);
                    run.alignment = alignmentFromStyle(style);
                    if (style.contains("lineHeightPx"_L1))
                        run.lineHeight = style["lineHeightPx"_L1].toDouble() * dpiScale;
                    runs.append(run);
                } else {
                    int pos = 0;
                    while (pos < characters.length() && pos < overrides.size()) {
                        int styleId = overrides[pos].toInt();
                        int start = pos;
                        while (pos < characters.length() && pos < overrides.size()
                               && overrides[pos].toInt() == styleId) {
                            ++pos;
                        }

                        QPsdTextLayerItem::Run run;
                        const QString rawText = characters.mid(start, pos - start);

                        QJsonObject effectiveStyle = style;
                        if (styleId > 0) {
                            const auto override_ = overrideTable[QString::number(styleId)].toObject();
                            for (auto it = override_.begin(); it != override_.end(); ++it)
                                effectiveStyle[it.key()] = it.value();
                        }

                        const QString etcCase = effectiveStyle["textCase"_L1].toString();
                        run.text = applyTextCase(rawText, etcCase);
                        run.font = fontFromStyle(effectiveStyle);
                        if (etcCase == "SMALL_CAPS"_L1) {
                            run.font.setCapitalization(QFont::SmallCaps);
                            run.fontCaps = 1;
                        } else if (etcCase == "UPPER"_L1) {
                            run.fontCaps = 2;
                        }
                        run.originalFontName = effectiveStyle["fontFamily"_L1].toString();
                        run.color = colorFromFills(effectiveStyle.contains("fills"_L1)
                                                     ? effectiveStyle["fills"_L1].toArray()
                                                     : fills);
                        run.alignment = alignmentFromStyle(style);
                        if (effectiveStyle.contains("lineHeightPx"_L1))
                            run.lineHeight = effectiveStyle["lineHeightPx"_L1].toDouble() * dpiScale;
                        runs.append(run);
                    }

                    if (pos < characters.length()) {
                        QPsdTextLayerItem::Run run;
                        const QString tailCase = style["textCase"_L1].toString();
                        run.text = applyTextCase(characters.mid(pos), tailCase);
                        run.font = fontFromStyle(style);
                        if (tailCase == "SMALL_CAPS"_L1) {
                            run.font.setCapitalization(QFont::SmallCaps);
                            run.fontCaps = 1;
                        } else if (tailCase == "UPPER"_L1) {
                            run.fontCaps = 2;
                        }
                        run.originalFontName = style["fontFamily"_L1].toString();
                        run.color = colorFromFills(fills);
                        run.alignment = alignmentFromStyle(style);
                        if (style.contains("lineHeightPx"_L1)
                            && style["lineHeightUnit"_L1].toString() != "INTRINSIC_%"_L1)
                            run.lineHeight = style["lineHeightPx"_L1].toDouble() * dpiScale;
                        runs.append(run);
                    }

                    QList<QPsdTextLayerItem::Run> mergedRuns;
                    for (const auto &run : runs) {
                        if (!mergedRuns.isEmpty() &&
                            mergedRuns.last().font == run.font &&
                            mergedRuns.last().color == run.color &&
                            mergedRuns.last().alignment == run.alignment &&
                            qFuzzyCompare(mergedRuns.last().lineHeight, run.lineHeight)) {
                            mergedRuns.last().text += run.text;
                        } else {
                            mergedRuns.append(run);
                        }
                    }
                    runs = mergedRuns;
                }
            }

            textItem->setRuns(runs);

            // Compute capHeightOffset to compensate for QPsdTextItem adjustment.
            // QPsdTextItem subtracts (ascent - capHeight) from drawTop, which
            // shifts Figma text too high. Add it back to the textFrame Y.
            QFont primaryFont = fontFromStyle(style);
            QFont adjustedFont = primaryFont;
            adjustedFont.setPointSizeF(adjustedFont.pointSizeF() / dpiScale);
            adjustedFont.setStyleStrategy(QFont::PreferTypoLineMetrics);
            adjustedFont.setHintingPreference(QFont::PreferNoHinting);
            QFontMetrics fm(adjustedFont);
            const qreal capHeightOffset = fm.ascent() - fm.capHeight();

            // Figma uses CSS-style half-leading: extra space from lineHeight
            // is split equally above and below the text glyphs. QPsdTextItem
            // draws from the top of the frame, so we must offset Y by
            // (lineHeight - fontHeight) / 2 to match Figma's positioning.
            qreal halfLeading = 0;
            if (style.contains("lineHeightPx"_L1)
                && style["lineHeightUnit"_L1].toString() != "INTRINSIC_%"_L1) {
                const qreal lineHeightPx = style["lineHeightPx"_L1].toDouble();
                const qreal fontHeight = fm.height();
                if (lineHeightPx > fontHeight)
                    halfLeading = (lineHeightPx - fontHeight) / 2;
            }

            // textFrame must use absolute coordinates (absX/absY) because
            // cloneLayerItem creates items from PSD records with absolute rects.
            const qreal textFrameY = absY + capHeightOffset + halfLeading;
            const auto textAutoResize = style["textAutoResize"_L1].toString();
            if (textAutoResize != "WIDTH_AND_HEIGHT"_L1) {
                // Fixed-width text box (NONE, HEIGHT, TRUNCATE, or absent):
                // use ParagraphText with word wrapping at bbox width
                textItem->setTextType(QPsdTextLayerItem::TextType::ParagraphText);
                textItem->setTextFrame(QRectF(absX, textFrameY,
                                              rect.width(), rect.height()));
            } else {
                // Auto-width text: Figma imposes no width constraint (bbox is a
                // measured rendering result, not input). Use PointText so Qt
                // renders without word-wrap, avoiding wrapping caused by minor
                // glyph advance differences between Figma and Qt text engines.
                textItem->setTextType(QPsdTextLayerItem::TextType::PointText);

                // PointText anchor: baseline position in absolute coordinates
                // (matching how flattenFigmaTree converts rect to absolute).
                const auto alignment = alignmentFromStyle(style);
                const auto hAlign = alignment & Qt::AlignHorizontal_Mask;
                qreal originX = absX;
                if (hAlign == Qt::AlignHCenter)
                    originX = absX + rect.width() / 2.0;
                else if (hAlign == Qt::AlignRight)
                    originX = absX + rect.width();

                // Vertical: baseline = absY + halfLeading + ascent
                textItem->setTextOrigin(QPointF(originX,
                                                absY + halfLeading + fm.ascent()));
            }

            node.layerItem = textItem;
            node.folderType = FolderType::NotFolder;

        } else if (hasImageFill && !folderTypes.contains(nodeType)) {
            auto *imageItem = new QPsdImageLayerItem();
            imageItem->setId(layerId);
            imageItem->setName(name);
            imageItem->setVisible(visible);
            imageItem->setOpacity(opacity);
            imageItem->setRect(rect);
            node.layerItem = imageItem;
            node.folderType = FolderType::NotFolder;

        } else if (folderTypes.contains(nodeType)) {
            auto *folderItem = new QPsdFolderLayerItem();
            folderItem->setId(layerId);
            folderItem->setName(name);
            folderItem->setVisible(visible);
            folderItem->setOpacity(opacity);
            folderItem->setIsOpened(true);

            const bool isTopLevelFrame = (parentId == 0)
                && (nodeType == "FRAME"_L1 || nodeType == "COMPONENT"_L1
                    || nodeType == "SECTION"_L1 || nodeType == "COMPONENT_SET"_L1);
            folderItem->setRect(rect);

            // Extract corner radius for frames/components
            const qreal frameCornerRadius = nodeJson["cornerRadius"_L1].toDouble(0);
            const auto frameRadiiArray = nodeJson["rectangleCornerRadii"_L1].toArray();

            // Build PathInfo from frame corner radii (uniform or per-corner)
            auto buildFramePathInfo = [&](qreal w, qreal h) {
                if (frameRadiiArray.size() == 4) {
                    return pathInfoFromCornerRadii(w, h,
                        frameRadiiArray[0].toDouble(), frameRadiiArray[1].toDouble(),
                        frameRadiiArray[2].toDouble(), frameRadiiArray[3].toDouble());
                }
                return pathInfoFromCornerRadii(w, h, 0, 0, 0, 0, frameCornerRadius);
            };

            if (nodeJson.value("clipsContent"_L1).toBool(false)) {
                auto clipMask = buildFramePathInfo(rect.width(), rect.height());
                folderItem->setVectorMask(clipMask);
            }

            bool needsGradientBg = false;
            if (isTopLevelFrame) {
                folderItem->setArtboardRect(rect);
                if (!fills.isEmpty()) {
                    const auto frameBrush = brushFromFigmaFills(fills, rect);
                    if (frameBrush.gradient()) {
                        needsGradientBg = true;
                    } else if (frameBrush.style() != Qt::NoBrush) {
                        folderItem->setArtboardBackground(frameBrush.color());
                    }
                }
            } else if ((nodeType == "FRAME"_L1 || nodeType == "COMPONENT"_L1
                        || nodeType == "INSTANCE"_L1 || nodeType == "COMPONENT_SET"_L1)
                       && !fills.isEmpty()) {
                const auto nestedBrush = brushFromFigmaFills(fills, rect);
                if (nestedBrush.style() != Qt::NoBrush)
                    needsGradientBg = true;
            }

            node.layerItem = folderItem;
            node.folderType = FolderType::OpenFolder;

            // Process children (reverse: Figma back-to-front → PSD front-to-back)
            // Pass absolute position so children use parent-relative coords.
            // flattenFigmaTree converts back to absolute for PSD records.
            const QPoint childOffset(absX, absY);
            const auto children = nodeJson["children"_L1].toArray();
            for (int ci = children.size() - 1; ci >= 0; --ci) {
                quintptr childId = processNode(children.at(ci).toObject(), nodeId, childOffset);
                if (childId != 0)
                    node.childIds.append(childId);
            }

            // Synthetic background layers are appended in PSD front-to-back order:
            // SOLID/gradient fill (in front) → IMAGE fill (behind)
            // Note: IMAGE fills normally composite on TOP with blend modes (e.g. LINEAR_BURN),
            // but without blend mode support they'd wash out the solid fill.
            // Placing them behind keeps correct dark backgrounds.
            if (needsGradientBg) {
                const auto frameBrush = brushFromFigmaFills(fills, rect);
                quintptr bgId = m_nextId++;
                Node bgNode;
                bgNode.id = bgId;
                bgNode.parentId = nodeId;
                bgNode.figmaId = figmaId + "_bg"_L1;
                bgNode.name = name + " background"_L1;
                bgNode.layerId = qHash(bgNode.figmaId);
                const QRect bgRect(0, 0, rect.width(), rect.height());
                bgNode.rect = bgRect;
                bgNode.blendMode = QPsdBlend::Normal;

                auto *bgShape = new QPsdShapeLayerItem();
                bgShape->setId(bgNode.layerId);
                bgShape->setName(bgNode.name);
                bgShape->setVisible(true);
                bgShape->setOpacity(1.0);
                bgShape->setRect(bgRect);
                bgShape->setBrush(frameBrush);
                auto bgPath = buildFramePathInfo(rect.width(), rect.height());
                bgShape->setPathInfo(bgPath);

                bgNode.layerItem = bgShape;
                bgNode.folderType = FolderType::NotFolder;

                m_nodes.insert(bgId, bgNode);
                m_nodeIdMap.insert(bgNode.figmaId, bgId);
                m_layerItems.append(bgShape);
                node.childIds.append(bgId);
            }

            if (hasImageFill) {
                // Extract imageRef from the first visible IMAGE fill
                QString imgRef;
                for (const auto &fill : fills) {
                    const auto obj = fill.toObject();
                    if (obj["type"_L1].toString() == "IMAGE"_L1
                        && obj.value("visible"_L1).toBool(true)) {
                        imgRef = obj["imageRef"_L1].toString();
                        if (!imgRef.isEmpty())
                            break;
                    }
                }

                quintptr imgBgId = m_nextId++;
                Node imgBgNode;
                imgBgNode.id = imgBgId;
                imgBgNode.parentId = nodeId;
                imgBgNode.figmaId = figmaId + "_imgbg"_L1;
                imgBgNode.name = name + " image"_L1;
                imgBgNode.layerId = qHash(imgBgNode.figmaId);
                const QRect imgBgRect(0, 0, rect.width(), rect.height());
                imgBgNode.rect = imgBgRect;
                imgBgNode.blendMode = QPsdBlend::Normal;
                imgBgNode.imageRef = imgRef;

                auto *imgItem = new QPsdImageLayerItem();
                imgItem->setId(imgBgNode.layerId);
                imgItem->setName(imgBgNode.name);
                imgItem->setVisible(true);
                imgItem->setOpacity(1.0);
                imgItem->setRect(imgBgRect);

                imgBgNode.layerItem = imgItem;
                imgBgNode.folderType = FolderType::NotFolder;

                m_nodes.insert(imgBgId, imgBgNode);
                m_nodeIdMap.insert(imgBgNode.figmaId, imgBgId);
                m_layerItems.append(imgItem);
                node.childIds.append(imgBgId);
            }

        } else if (shapeTypes.contains(nodeType)) {
            auto *shapeItem = new QPsdShapeLayerItem();
            shapeItem->setId(layerId);
            shapeItem->setName(name);
            shapeItem->setVisible(visible);
            shapeItem->setOpacity(opacity);
            shapeItem->setRect(rect);

            // Figma mask layers define clipping shapes for siblings above
            // them.  Their fills determine the mask region, but should NOT
            // render as visible content in the scene.
            if (!node.isMask) {
                const auto brush = brushFromFigmaFills(fills, rect);
                if (brush.style() != Qt::NoBrush)
                    shapeItem->setBrush(brush);
            }

            const auto strokes = nodeJson["strokes"_L1].toArray();
            const auto strokeWeight = nodeJson["strokeWeight"_L1].toDouble(0);
            if (!strokes.isEmpty() && strokeWeight > 0) {
                const auto strokeObj = strokes.first().toObject();
                if (strokeObj.value("visible"_L1).toBool(true)) {
                    const auto strokeType = strokeObj["type"_L1].toString();
                    QPen pen;
                    if (strokeType == "SOLID"_L1) {
                        const auto color = strokeObj["color"_L1].toObject();
                        QColor c = QColor::fromRgbF(
                            color["r"_L1].toDouble(),
                            color["g"_L1].toDouble(),
                            color["b"_L1].toDouble(),
                            strokeObj.value("opacity"_L1).toDouble(1.0));
                        pen = QPen(c);
                    } else if (strokeType.startsWith("GRADIENT_"_L1)) {
                        QBrush strokeBrush = brushFromSingleFill(strokeObj, rect);
                        if (strokeBrush.style() != Qt::NoBrush)
                            pen = QPen(strokeBrush, strokeWeight);
                    }

                    if (pen.style() != Qt::NoPen) {
                        pen.setWidthF(strokeWeight);

                        const auto strokeCap = nodeJson["strokeCap"_L1].toString();
                        if (strokeCap == "ROUND"_L1)
                            pen.setCapStyle(Qt::RoundCap);
                        else if (strokeCap == "SQUARE"_L1)
                            pen.setCapStyle(Qt::SquareCap);
                        else
                            pen.setCapStyle(Qt::FlatCap);

                        const auto strokeJoin = nodeJson["strokeJoin"_L1].toString();
                        if (strokeJoin == "ROUND"_L1)
                            pen.setJoinStyle(Qt::RoundJoin);
                        else if (strokeJoin == "BEVEL"_L1)
                            pen.setJoinStyle(Qt::BevelJoin);
                        else
                            pen.setJoinStyle(Qt::MiterJoin);

                        const auto strokeDashes = nodeJson["strokeDashes"_L1].toArray();
                        if (!strokeDashes.isEmpty() && strokeWeight > 0) {
                            QVector<qreal> pattern;
                            for (const auto &d : strokeDashes)
                                pattern.append(d.toDouble() / strokeWeight);
                            pen.setDashPattern(pattern);
                        }

                        shapeItem->setPen(pen);

                        const auto strokeAlign = nodeJson["strokeAlign"_L1].toString();
                        if (strokeAlign == "INSIDE"_L1)
                            shapeItem->setStrokeAlignment(QPsdShapeLayerItem::StrokeInside);
                        else if (strokeAlign == "OUTSIDE"_L1)
                            shapeItem->setStrokeAlignment(QPsdShapeLayerItem::StrokeOutside);
                        else
                            shapeItem->setStrokeAlignment(QPsdShapeLayerItem::StrokeCenter);
                    }
                }
            }

            QPsdAbstractLayerItem::PathInfo pathInfo;
            const qreal w = rect.width();
            const qreal h = rect.height();

            auto parseGeometry = [&](const QString &key) -> QPainterPath {
                const auto geometry = nodeJson[key].toArray();
                if (geometry.isEmpty())
                    return {};
                QPainterPath pp;
                for (const auto &geom : geometry) {
                    const auto svgPathData = geom.toObject()["path"_L1].toString();
                    if (!svgPathData.isEmpty())
                        pp.addPath(parseSvgPath(svgPathData));
                }
                return pp;
            };

            if (nodeType == "RECTANGLE"_L1) {
                const auto radiiArray = nodeJson["rectangleCornerRadii"_L1].toArray();
                const auto cornerRadius = nodeJson["cornerRadius"_L1].toDouble(0);
                if (radiiArray.size() == 4) {
                    pathInfo = pathInfoFromCornerRadii(w, h,
                        radiiArray[0].toDouble(), radiiArray[1].toDouble(),
                        radiiArray[2].toDouble(), radiiArray[3].toDouble());
                } else {
                    pathInfo = pathInfoFromCornerRadii(w, h, 0, 0, 0, 0, cornerRadius);
                }
            } else if (nodeType == "ELLIPSE"_L1) {
                pathInfo.type = QPsdAbstractLayerItem::PathInfo::Path;
                QPainterPath pp;
                pp.addEllipse(QRectF(0, 0, w, h));
                pathInfo.path = pp;
                pathInfo.rect = QRectF(0, 0, w, h);
            } else if (nodeType == "LINE"_L1) {
                // LINE nodes: strokeGeometry already includes the complete
                // visual (stroke width + caps) as a filled shape. Use it
                // as fill and clear the stroke pen to avoid double rendering.
                QPainterPath pp = parseGeometry("strokeGeometry"_L1);
                if (!pp.isEmpty()) {
                    shapeItem->setBrush(shapeItem->pen().brush());
                    shapeItem->setPen(Qt::NoPen);
                } else {
                    pp.moveTo(0, h / 2.0);
                    pp.lineTo(w, h / 2.0);
                }
                pathInfo.type = QPsdAbstractLayerItem::PathInfo::Path;
                pathInfo.path = pp;
            } else {
                // STAR, POLYGON, REGULAR_POLYGON, VECTOR, BOOLEAN_OPERATION
                QPainterPath pp = parseGeometry("fillGeometry"_L1);
                if (pp.isEmpty())
                    pp = parseGeometry("strokeGeometry"_L1);
                if (!pp.isEmpty()) {
                    pathInfo.type = QPsdAbstractLayerItem::PathInfo::Path;
                    pathInfo.path = pp;
                }
            }
            // Apply rotation/scale from relativeTransform to the parsed path.
            // Figma's fillGeometry/strokeGeometry paths are in the node's local
            // coordinate space (pre-transform), but absoluteBoundingBox gives
            // post-transform bounds. Transform the path to match.
            if (pathInfo.type == QPsdAbstractLayerItem::PathInfo::Path) {
                const auto transform = nodeJson["relativeTransform"_L1].toArray();
                if (transform.size() == 2) {
                    const auto row0 = transform[0].toArray();
                    const auto row1 = transform[1].toArray();
                    if (row0.size() >= 2 && row1.size() >= 2) {
                        const qreal a = row0[0].toDouble();
                        const qreal b = row0[1].toDouble();
                        const qreal c = row1[0].toDouble();
                        const qreal d = row1[1].toDouble();
                        // Check for non-identity rotation/scale
                        if (qAbs(a - 1.0) > 1e-4 || qAbs(d - 1.0) > 1e-4
                            || qAbs(b) > 1e-4 || qAbs(c) > 1e-4) {
                            QTransform xform(a, c, b, d, 0, 0);
                            pathInfo.path = xform.map(pathInfo.path);
                            const QRectF bounds = pathInfo.path.boundingRect();
                            if (!bounds.isNull())
                                pathInfo.path.translate(-bounds.x(), -bounds.y());
                        }
                    }
                }
            }

            shapeItem->setPathInfo(pathInfo);

            if (pathInfo.type == QPsdAbstractLayerItem::PathInfo::None)
                shapeItem->setBrush(Qt::NoBrush);

            node.layerItem = shapeItem;
            node.folderType = FolderType::NotFolder;
        } else {
            auto *imageItem = new QPsdImageLayerItem();
            imageItem->setId(layerId);
            imageItem->setName(name);
            imageItem->setVisible(visible);
            imageItem->setOpacity(opacity);
            imageItem->setRect(rect);
            node.layerItem = imageItem;
            node.folderType = FolderType::NotFolder;
        }

        // Parse shadow effects
        if (node.layerItem) {
            const auto effects = nodeJson["effects"_L1].toArray();
            for (const auto &effect : effects) {
                const auto eff = effect.toObject();
                if (!eff.value("visible"_L1).toBool(true))
                    continue;
                const auto effType = eff["type"_L1].toString();
                if (effType == "DROP_SHADOW"_L1) {
                    const auto c = eff["color"_L1].toObject();
                    const auto offset = eff["offset"_L1].toObject();
                    const qreal ox = offset["x"_L1].toDouble();
                    const qreal oy = offset["y"_L1].toDouble();
                    QCborMap shadow;
                    shadow.insert(QLatin1String("color"), QColor::fromRgbF(
                        c["r"_L1].toDouble(), c["g"_L1].toDouble(), c["b"_L1].toDouble()).name());
                    shadow.insert(QLatin1String("opacity"), c["a"_L1].toDouble(1.0));
                    shadow.insert(QLatin1String("angle"), std::atan2(oy, ox) * 180.0 / M_PI);
                    shadow.insert(QLatin1String("distance"), std::sqrt(ox * ox + oy * oy));
                    shadow.insert(QLatin1String("spread"), eff["spread"_L1].toDouble(0));
                    shadow.insert(QLatin1String("size"), eff["radius"_L1].toDouble(0));
                    node.layerItem->setDropShadow(shadow);
                } else if (effType == "INNER_SHADOW"_L1) {
                    const auto c = eff["color"_L1].toObject();
                    const auto offset = eff["offset"_L1].toObject();
                    const qreal ox = offset["x"_L1].toDouble();
                    const qreal oy = offset["y"_L1].toDouble();
                    QCborMap shadow;
                    shadow.insert(QLatin1String("color"), QColor::fromRgbF(
                        c["r"_L1].toDouble(), c["g"_L1].toDouble(), c["b"_L1].toDouble()).name());
                    shadow.insert(QLatin1String("opacity"), c["a"_L1].toDouble(1.0));
                    shadow.insert(QLatin1String("angle"), std::atan2(oy, ox) * 180.0 / M_PI);
                    shadow.insert(QLatin1String("distance"), std::sqrt(ox * ox + oy * oy));
                    shadow.insert(QLatin1String("size"), eff["radius"_L1].toDouble(0));
                    node.layerItem->setInnerShadow(shadow);
                } else if (effType == "LAYER_BLUR"_L1) {
                    node.layerItem->setLayerBlur(eff["radius"_L1].toDouble(0));
                }
            }
        }

        m_nodes.insert(nodeId, node);
        m_nodeIdMap.insert(figmaId, nodeId);
        m_layerItems.append(node.layerItem);

        return nodeId;
    }

    static QFont fontFromStyle(const QJsonObject &style)
    {
        QFont font;
        font.setFamily(style["fontFamily"_L1].toString());
        font.setPointSizeF(style["fontSize"_L1].toDouble(12));
        font.setWeight(static_cast<QFont::Weight>(style["fontWeight"_L1].toInt(400)));
        if (style["italic"_L1].toBool())
            font.setItalic(true);
        const auto decoration = style["textDecoration"_L1].toString();
        if (decoration == "UNDERLINE"_L1)
            font.setUnderline(true);
        else if (decoration == "STRIKETHROUGH"_L1)
            font.setStrikeOut(true);
        // letterSpacing: number (from style) or object {value, unit} (from styledTextSegments)
        const auto ls = style["letterSpacing"_L1];
        if (ls.isObject()) {
            const auto lsObj = ls.toObject();
            const auto unit = lsObj["unit"_L1].toString();
            const auto value = lsObj["value"_L1].toDouble(0);
            if (unit == "PERCENT"_L1) {
                if (qAbs(value) > 0.01)
                    font.setLetterSpacing(QFont::PercentageSpacing, 100 + value);
            } else if (qAbs(value) > 0.01) {
                font.setLetterSpacing(QFont::AbsoluteSpacing, value);
            }
        } else {
            const auto letterSpacing = ls.toDouble(0);
            if (qAbs(letterSpacing) > 0.01)
                font.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
        }
        return font;
    }

    static QColor colorFromFills(const QJsonArray &fills)
    {
        for (const auto &fill : fills) {
            const auto obj = fill.toObject();
            if (obj["type"_L1].toString() == "SOLID"_L1
                && obj.value("visible"_L1).toBool(true)) {
                const auto color = obj["color"_L1].toObject();
                return QColor::fromRgbF(
                    color["r"_L1].toDouble(),
                    color["g"_L1].toDouble(),
                    color["b"_L1].toDouble(),
                    obj.value("opacity"_L1).toDouble(1.0));
            }
        }
        return Qt::black;
    }

    static QPsdBlend::Mode blendModeFromFigma(const QString &mode)
    {
        static const QHash<QString, QPsdBlend::Mode> map = {
            {"PASS_THROUGH"_L1, QPsdBlend::PassThrough},
            {"NORMAL"_L1, QPsdBlend::Normal},
            {"DARKEN"_L1, QPsdBlend::Darken},
            {"MULTIPLY"_L1, QPsdBlend::Multiply},
            {"COLOR_BURN"_L1, QPsdBlend::ColorBurn},
            {"LINEAR_BURN"_L1, QPsdBlend::LinearBurn},
            {"LIGHTEN"_L1, QPsdBlend::Lighten},
            {"SCREEN"_L1, QPsdBlend::Screen},
            {"COLOR_DODGE"_L1, QPsdBlend::ColorDodge},
            {"LINEAR_DODGE"_L1, QPsdBlend::LinearDodge},
            {"OVERLAY"_L1, QPsdBlend::Overlay},
            {"SOFT_LIGHT"_L1, QPsdBlend::SoftLight},
            {"HARD_LIGHT"_L1, QPsdBlend::HardLight},
            {"DIFFERENCE"_L1, QPsdBlend::Difference},
            {"EXCLUSION"_L1, QPsdBlend::Exclusion},
            {"HUE"_L1, QPsdBlend::Hue},
            {"SATURATION"_L1, QPsdBlend::Saturation},
            {"COLOR"_L1, QPsdBlend::Color},
            {"LUMINOSITY"_L1, QPsdBlend::Luminosity},
        };
        return map.value(mode, QPsdBlend::Normal);
    }

    static QBrush brushFromSingleFill(const QJsonObject &obj, const QRect &rect)
    {
        const auto type = obj["type"_L1].toString();
        const qreal fillOpacity = obj.value("opacity"_L1).toDouble(1.0);

        if (type == "SOLID"_L1) {
            const auto color = obj["color"_L1].toObject();
            QColor c = QColor::fromRgbF(
                color["r"_L1].toDouble(),
                color["g"_L1].toDouble(),
                color["b"_L1].toDouble(),
                fillOpacity);
            return QBrush(c);
        }

        const auto handles = obj["gradientHandlePositions"_L1].toArray();
        const auto stopsArr = obj["gradientStops"_L1].toArray();
        if (handles.size() < 2 || stopsArr.isEmpty())
            return QBrush();

        QGradientStops stops;
        for (const auto &s : stopsArr) {
            const auto sObj = s.toObject();
            const auto sc = sObj["color"_L1].toObject();
            QColor stopColor = QColor::fromRgbF(
                sc["r"_L1].toDouble(),
                sc["g"_L1].toDouble(),
                sc["b"_L1].toDouble(),
                sc["a"_L1].toDouble(1.0) * fillOpacity);
            stops.append({sObj["position"_L1].toDouble(), stopColor});
        }

        const auto h0 = handles[0].toObject();
        const auto h1 = handles[1].toObject();
        const qreal x0 = h0["x"_L1].toDouble() * rect.width();
        const qreal y0 = h0["y"_L1].toDouble() * rect.height();
        const qreal x1 = h1["x"_L1].toDouble() * rect.width();
        const qreal y1 = h1["y"_L1].toDouble() * rect.height();

        if (type == "GRADIENT_LINEAR"_L1) {
            // Linear gradients go from h0 (start) to h1 (end).
            // The optional h2 (perpendicular handle) can skew the gradient coordinate
            // system, but for a linear gradient the perpendicular direction is irrelevant
            // — the gradient is invariant along it. So we always use (x0,y0)→(x1,y1).
            QLinearGradient gradient(x0, y0, x1, y1);
            gradient.setInterpolationMode(QGradient::ComponentInterpolation);
            gradient.setStops(stops);
            return QBrush(gradient);
        } else if (type == "GRADIENT_DIAMOND"_L1) {
            // Diamond gradient: iso-lines are diamond-shaped (manhattan distance)
            // h0 = center, h1 = primary axis end, h2 = secondary axis end
            const qreal dx1 = x1 - x0, dy1 = y1 - y0;
            const qreal r1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
            qreal dx2 = 0, dy2 = 0, r2 = r1;
            if (handles.size() >= 3) {
                const auto h2 = handles[2].toObject();
                const qreal x2 = h2["x"_L1].toDouble() * rect.width();
                const qreal y2 = h2["y"_L1].toDouble() * rect.height();
                dx2 = x2 - x0; dy2 = y2 - y0;
                r2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
            } else {
                // Perpendicular to primary axis
                dx2 = -dy1; dy2 = dx1;
            }
            if (r1 < 0.001 || r2 < 0.001)
                return QBrush();
            // Unit vectors along primary and secondary axes
            const qreal ux1 = dx1 / r1, uy1 = dy1 / r1;
            const qreal ux2 = dx2 / r2, uy2 = dy2 / r2;
            // Render diamond gradient into an image
            QImage img(rect.size(), QImage::Format_ARGB32);
            for (int py = 0; py < rect.height(); ++py) {
                QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(py));
                for (int px = 0; px < rect.width(); ++px) {
                    const qreal dx = px - x0, dy = py - y0;
                    // Project onto primary and secondary axes, use manhattan distance
                    const qreal t = qAbs(dx * ux1 + dy * uy1) / r1
                                  + qAbs(dx * ux2 + dy * uy2) / r2;
                    // Sample gradient color at position t
                    QColor c;
                    if (t <= stops.first().first) {
                        c = stops.first().second;
                    } else if (t >= stops.last().first) {
                        c = stops.last().second;
                    } else {
                        for (int i = 1; i < stops.size(); ++i) {
                            if (t <= stops[i].first) {
                                const qreal f = (t - stops[i-1].first) / (stops[i].first - stops[i-1].first);
                                const QColor &c0 = stops[i-1].second;
                                const QColor &c1 = stops[i].second;
                                // Premultiplied alpha interpolation (matches Figma)
                                const qreal a0 = c0.alphaF(), a1 = c1.alphaF();
                                const qreal pa0r = c0.redF() * a0, pa0g = c0.greenF() * a0, pa0b = c0.blueF() * a0;
                                const qreal pa1r = c1.redF() * a1, pa1g = c1.greenF() * a1, pa1b = c1.blueF() * a1;
                                const qreal a = a0 + (a1 - a0) * f;
                                if (a > 0.0001) {
                                    const qreal pr = pa0r + (pa1r - pa0r) * f;
                                    const qreal pg = pa0g + (pa1g - pa0g) * f;
                                    const qreal pb = pa0b + (pa1b - pa0b) * f;
                                    c = QColor::fromRgbF(pr / a, pg / a, pb / a, a);
                                } else {
                                    c = QColor::fromRgbF(0, 0, 0, 0);
                                }
                                break;
                            }
                        }
                    }
                    line[px] = c.rgba();
                }
            }
            return QBrush(img);
        } else if (type == "GRADIENT_RADIAL"_L1) {
            const qreal dx1 = x1 - x0, dy1 = y1 - y0;
            const qreal r1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
            if (handles.size() >= 3 && r1 > 0.001) {
                // Elliptical radial gradient: h0=center, h1=primary axis, h2=secondary axis
                const auto h2 = handles[2].toObject();
                const qreal x2 = h2["x"_L1].toDouble() * rect.width();
                const qreal y2 = h2["y"_L1].toDouble() * rect.height();
                const qreal dx2 = x2 - x0, dy2 = y2 - y0;
                const qreal r2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
                if (r2 > 0.001) {
                    // Unit circle gradient transformed to the ellipse
                    QRadialGradient gradient(0, 0, 1.0);
                    gradient.setInterpolationMode(QGradient::ComponentInterpolation);
                    gradient.setStops(stops);
                    const qreal angle = std::atan2(dy1, dx1) * 180.0 / M_PI;
                    QTransform t;
                    t.translate(x0, y0);
                    t.rotate(angle);
                    t.scale(r1, r2);
                    QBrush brush(gradient);
                    brush.setTransform(t);
                    return brush;
                }
            }
            // Fallback: circular radial gradient
            QRadialGradient gradient(x0, y0, r1 > 0.001 ? r1 : 1.0);
            gradient.setInterpolationMode(QGradient::ComponentInterpolation);
            gradient.setStops(stops);
            return QBrush(gradient);
        } else if (type == "GRADIENT_ANGULAR"_L1) {
            const qreal angle = std::atan2(y1 - y0, x1 - x0) * 180.0 / M_PI;
            QConicalGradient gradient(x0, y0, angle);
            gradient.setInterpolationMode(QGradient::ComponentInterpolation);
            // Figma sweeps CW, Qt sweeps CCW: reverse stops to flip direction
            QGradientStops cwStops;
            for (int i = stops.size() - 1; i >= 0; --i) {
                qreal pos = 1.0 - stops[i].first;
                cwStops.append({pos, stops[i].second});
            }
            gradient.setStops(cwStops);
            return QBrush(gradient);
        }
        return QBrush();
    }

    static QBrush brushFromFigmaFills(const QJsonArray &fills, const QRect &rect)
    {
        // Collect visible fills (bottom to top)
        QList<QJsonObject> visibleFills;
        for (int i = 0; i < fills.size(); ++i) {
            const auto obj = fills[i].toObject();
            if (!obj.value("visible"_L1).toBool(true))
                continue;
            if (obj["type"_L1].toString() == "IMAGE"_L1)
                continue;
            visibleFills.append(obj);
        }

        if (visibleFills.isEmpty())
            return QBrush();

        // Single fill: return directly (no compositing needed)
        if (visibleFills.size() == 1)
            return brushFromSingleFill(visibleFills.first(), rect);

        // Multiple fills: composite into a QImage
        QImage composited(rect.size(), QImage::Format_ARGB32_Premultiplied);
        composited.fill(Qt::transparent);
        QPainter p(&composited);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF localRect(QPointF(0, 0), QSizeF(rect.size()));
        for (const auto &obj : visibleFills) {
            QBrush brush = brushFromSingleFill(obj, rect);
            if (brush.style() != Qt::NoBrush) {
                p.setPen(Qt::NoPen);
                p.setBrush(brush);
                p.drawRect(localRect);
            }
        }
        p.end();
        return QBrush(composited);
    }

    static Qt::Alignment alignmentFromStyle(const QJsonObject &style)
    {
        Qt::Alignment alignment;
        const auto hAlign = style["textAlignHorizontal"_L1].toString();
        const auto vAlign = style["textAlignVertical"_L1].toString();

        if (hAlign == "CENTER"_L1) alignment |= Qt::AlignHCenter;
        else if (hAlign == "RIGHT"_L1) alignment |= Qt::AlignRight;
        else if (hAlign == "JUSTIFIED"_L1) alignment |= Qt::AlignJustify;
        else alignment |= Qt::AlignLeft;

        if (vAlign == "CENTER"_L1) alignment |= Qt::AlignVCenter;
        else if (vAlign == "BOTTOM"_L1) alignment |= Qt::AlignBottom;
        else alignment |= Qt::AlignTop;

        return alignment;
    }

    QHash<quintptr, Node> m_nodes;
    QList<quintptr> m_rootIds;
    quintptr m_nextId = 1;
    QSize m_size;
    QColor m_canvasColor;
    QPoint m_canvasOrigin;
    QSize m_fullCanvasSize;
    QPoint m_fullCanvasOrigin;
    QHash<QString, quintptr> m_nodeIdMap;
    QList<QPsdAbstractLayerItem *> m_layerItems;
    QList<ArtboardInfo> m_artboards;
};

// ─── buildQPsdModel ────────────────────────────────────────────────────────

static QPsdAbstractLayerItem *cloneLayerItem(const QPsdAbstractLayerItem *src, const QPsdLayerRecord &record)
{
    if (!src) return nullptr;
    switch (src->type()) {
    case QPsdAbstractLayerItem::Text: {
        auto *s = static_cast<const QPsdTextLayerItem *>(src);
        auto *d = new QPsdTextLayerItem(record);
        d->setRuns(s->runs());
        d->setTextType(s->textType());
        d->setTextFrame(s->textFrame());
        d->setTextOrigin(s->textOrigin());
        d->setDropShadow(s->dropShadow());
        d->setInnerShadow(s->innerShadow());
        d->setLayerBlur(s->layerBlur());
        return d;
    }
    case QPsdAbstractLayerItem::Shape: {
        auto *s = static_cast<const QPsdShapeLayerItem *>(src);
        auto *d = new QPsdShapeLayerItem(record);
        d->setBrush(s->brush());
        d->setPen(s->pen());
        d->setPathInfo(s->pathInfo());
        d->setStrokeAlignment(s->strokeAlignment());
        d->setDropShadow(s->dropShadow());
        d->setInnerShadow(s->innerShadow());
        d->setLayerBlur(s->layerBlur());
        return d;
    }
    case QPsdAbstractLayerItem::Image: {
        auto *s = static_cast<const QPsdImageLayerItem *>(src);
        auto *d = new QPsdImageLayerItem(record);
        d->setImage(s->image());
        d->setDropShadow(s->dropShadow());
        d->setInnerShadow(s->innerShadow());
        d->setLayerBlur(s->layerBlur());
        return d;
    }
    case QPsdAbstractLayerItem::Folder: {
        auto *s = static_cast<const QPsdFolderLayerItem *>(src);
        auto *d = new QPsdFolderLayerItem(record, s->isOpened());
        d->setArtboardRect(s->artboardRect());
        d->setArtboardBackground(s->artboardBackground());
        d->setVectorMask(s->vectorMask());
        d->setDropShadow(s->dropShadow());
        d->setInnerShadow(s->innerShadow());
        d->setLayerBlur(s->layerBlur());
        return d;
    }
    default:
        return nullptr;
    }
}

static void flattenFigmaTree(FigmaLayerTreeItemModel *figmaModel,
                              const QModelIndex &parent,
                              QList<QPsdLayerRecord> &records,
                              QHash<quint32, const QPsdAbstractLayerItem *> &layerItemMap,
                              QPoint absOffset = {})
{
    const int n = figmaModel->rowCount(parent);

    // Detect mask layers among direct children.
    // In Figma, isMask masks all siblings above it (lower indices).
    // We iterate in reverse (back→front = PSD bottom→top), so once we
    // encounter a mask, all subsequently processed children are masked.
    bool masking = false;

    // Iterate in reverse: Figma lists children front-to-back (top first),
    // but PSD records are bottom-to-top (bottom first)
    for (int row = n - 1; row >= 0; --row) {
        const QModelIndex index = figmaModel->index(row, 0, parent);
        auto *item = figmaModel->data(index, FigmaLayerTreeItemModel::LayerItemObjectRole)
                         .value<QPsdAbstractLayerItem *>();
        if (!item) continue;

        const quint32 layerId = figmaModel->data(index, FigmaLayerTreeItemModel::LayerIdRole).value<quint32>();
        const QString name = figmaModel->data(index, FigmaLayerTreeItemModel::NameRole).toString();
        const QRect rect = figmaModel->data(index, FigmaLayerTreeItemModel::RectRole).toRect();
        const auto folderType = figmaModel->data(index, FigmaLayerTreeItemModel::FolderTypeRole)
                                    .value<FigmaLayerTreeItemModel::FolderType>();
        const auto blendMode = static_cast<QPsdBlend::Mode>(
            figmaModel->data(index, FigmaLayerTreeItemModel::BlendModeRole).toInt());
        const bool isMask = figmaModel->data(index, FigmaLayerTreeItemModel::IsMaskRole).toBool();

        // Mask layer itself is Base (default); it starts a new clipping region
        if (isMask)
            masking = true;

        // Layers above the mask (processed after in reverse iteration) are NonBase
        const bool isClipped = masking && !isMask;

        // Convert parent-relative rect to absolute (document) coordinates.
        // QPsdWidgetTreeItemModel expects absolute coords and converts to parent-relative internally.
        const QRect absRect(rect.x() + absOffset.x(), rect.y() + absOffset.y(),
                            rect.width(), rect.height());

        if (folderType != FigmaLayerTreeItemModel::FolderType::NotFolder) {
            QPsdLayerRecord closeRecord;
            closeRecord.setName(QByteArray("</Layer group>"));
            closeRecord.setFlags(0x00);
            QHash<QByteArray, QVariant> closeALI;
            QPsdSectionDividerSetting closeSetting;
            closeSetting.setType(QPsdSectionDividerSetting::BoundingSectionDivider);
            closeALI["lsdk"] = QVariant::fromValue(closeSetting);
            closeRecord.setAdditionalLayerInformation(closeALI);
            records.append(closeRecord);

            flattenFigmaTree(figmaModel, index, records, layerItemMap, absRect.topLeft());

            QPsdLayerRecord folderRecord;
            folderRecord.setRect(absRect);
            folderRecord.setName(name.toUtf8());
            folderRecord.setBlendMode(blendMode);
            folderRecord.setOpacity(qRound(item->opacity() * 255.0));
            folderRecord.setFlags(0x00);
            if (isClipped)
                folderRecord.setClipping(QPsdLayerRecord::NonBase);
            QHash<QByteArray, QVariant> folderALI;
            folderALI["lyid"] = layerId;
            folderALI["luni"] = name;
            QPsdSectionDividerSetting folderSetting;
            folderSetting.setType(folderType == FigmaLayerTreeItemModel::FolderType::OpenFolder
                                      ? QPsdSectionDividerSetting::OpenFolder
                                      : QPsdSectionDividerSetting::ClosedFolder);
            folderALI["lsdk"] = QVariant::fromValue(folderSetting);
            folderRecord.setAdditionalLayerInformation(folderALI);
            records.append(folderRecord);

            layerItemMap.insert(layerId, item);
        } else {
            QPsdLayerRecord leafRecord;
            leafRecord.setRect(absRect);
            leafRecord.setName(name.toUtf8());
            leafRecord.setBlendMode(blendMode == QPsdBlend::PassThrough ? QPsdBlend::Normal : blendMode);
            leafRecord.setOpacity(qRound(item->opacity() * 255.0));
            // In Figma, isMask layers define clipping shape but are not rendered visually.
            // Set the PSD "hidden" flag (0x02) so the mask layer is invisible,
            // while still acting as the clipping base for NonBase siblings.
            leafRecord.setFlags(isMask ? 0x02 : 0x00);
            if (isClipped)
                leafRecord.setClipping(QPsdLayerRecord::NonBase);
            QHash<QByteArray, QVariant> leafALI;
            leafALI["lyid"] = layerId;
            leafALI["luni"] = name;
            if (item->type() == QPsdAbstractLayerItem::Text)
                leafALI["TySh"] = QVariant();
            else if (item->type() == QPsdAbstractLayerItem::Shape)
                leafALI["vscg"] = QVariant();
            leafRecord.setAdditionalLayerInformation(leafALI);
            records.append(leafRecord);

            layerItemMap.insert(layerId, item);
        }
    }
}

static QPsdWidgetTreeItemModel *buildQPsdModel(FigmaLayerTreeItemModel *figmaModel, QObject *parent)
{
    QList<QPsdLayerRecord> records;
    QHash<quint32, const QPsdAbstractLayerItem *> layerItemMap;

    flattenFigmaTree(figmaModel, QModelIndex(), records, layerItemMap);

    QPsdFileHeader header;
    header.setWidth(figmaModel->size().width());
    header.setHeight(figmaModel->size().height());
    header.setChannels(4);
    header.setDepth(8);
    header.setColorMode(QPsdFileHeader::RGB);

    QPsdLayerInfo layerInfo;
    layerInfo.setRecords(records);
    QList<QPsdChannelImageData> channelData;
    for (int i = 0; i < records.size(); ++i)
        channelData.append(QPsdChannelImageData());
    layerInfo.setChannelImageData(channelData);

    QPsdLayerAndMaskInformation lami;
    lami.setLayerInfo(layerInfo);
    lami.setFileHeader(header);

    QPsdParser parser;
    parser.setFileHeader(header);
    parser.setLayerAndMaskInformation(lami);

    auto *model = new QPsdWidgetTreeItemModel(parent);
    model->fromParser(parser);

    std::function<void(const QModelIndex &)> injectItems = [&](const QModelIndex &parentIdx) {
        for (int row = 0; row < model->rowCount(parentIdx); ++row) {
            const QModelIndex index = model->index(row, 0, parentIdx);
            const quint32 lid = model->layerId(index);
            if (layerItemMap.contains(lid)) {
                const QPsdLayerRecord *rec = model->layerRecord(index);
                if (rec) {
                    const auto *src = layerItemMap.value(lid);
                    auto *clone = cloneLayerItem(src, *rec);
                    if (clone) {
                        model->setLayerItem(index, clone);
                    }
                }
            }
            injectItems(index);
        }
    };
    injectItems(QModelIndex());

    return model;
}

// ─── QPsdImporterFigmaPlugin ──────────────────────────────────────────────

class QPsdImporterFigmaPlugin : public QPsdImporterPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPsdImporterFactoryInterface_iid FILE "figma.json")
public:
    explicit QPsdImporterFigmaPlugin(QObject *parent = nullptr)
        : QPsdImporterPlugin(parent)
    {}

    int priority() const override { return 10; }
    QIcon icon() const override { return QIcon(u":/figma/figma.png"_s); }
    QString name() const override { return u"&Figma..."_s; }

    bool canHandleUrl(const QUrl &url) const override {
        static const QRegularExpression re(u"figma\\.com/(?:file|design)/([a-zA-Z0-9]+)"_s);
        if (re.match(url.toString()).hasMatch())
            return true;
        // Local .fig file (URL or bare path)
        const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
        return path.endsWith(".fig"_L1, Qt::CaseInsensitive)
            || path.endsWith(".figjam"_L1, Qt::CaseInsensitive);
    }

    QVariantMap optionsFromUrl(const QUrl &url) const override {
        QVariantMap options;
        options["source"_L1] = url.toString();

        // Resolve API key from QSettings, then environment variables
        QSettings settings;
        settings.beginGroup("Importers/Figma"_L1);
        QString apiKey = settings.value("apiKey"_L1).toString();
        if (apiKey.isEmpty())
            apiKey = qEnvironmentVariable("FIGMA_API_KEY");
        if (apiKey.isEmpty())
            apiKey = qEnvironmentVariable("FIGMA_ACCESS_TOKEN");
        if (!apiKey.isEmpty())
            options["apiKey"_L1] = apiKey;

        options["imageScale"_L1] = settings.value("imageScale"_L1, 2).toInt();
        return options;
    }

    mutable QJsonObject m_cachedFileJson;
    mutable QString m_cachedFileKey;

    QVariantMap execImportDialog(QWidget *parent) const override
    {
        // --- First dialog: URL, API key, scale ---
        QDialog dialog(parent);
        dialog.setWindowTitle(tr("Import from Figma"));
        dialog.setWindowIcon(icon());
        dialog.setMinimumWidth(480);

        auto *layout = new QFormLayout(&dialog);

        // Source mode selector: URL vs local file
        QSettings settings;
        settings.beginGroup("Importers/Figma"_L1);
        const QString savedMode = settings.value("sourceMode"_L1, "url"_L1).toString();
        auto *modeUrl = new QRadioButton(tr("Figma URL"), &dialog);
        auto *modeFile = new QRadioButton(tr("Local .fig file"), &dialog);
        if (savedMode == "file"_L1)
            modeFile->setChecked(true);
        else
            modeUrl->setChecked(true);
        auto *modeGroup = new QButtonGroup(&dialog);
        modeGroup->addButton(modeUrl, 0);
        modeGroup->addButton(modeFile, 1);
        auto *modeRow = new QWidget(&dialog);
        auto *modeRowLayout = new QHBoxLayout(modeRow);
        modeRowLayout->setContentsMargins(0, 0, 0, 0);
        modeRowLayout->addWidget(modeUrl);
        modeRowLayout->addWidget(modeFile);
        modeRowLayout->addStretch();
        layout->addRow(tr("Source:"), modeRow);

        // URL field
        auto *urlEdit = new QLineEdit(&dialog);
        urlEdit->setPlaceholderText(u"https://www.figma.com/design/..."_s);
        const int urlRow = layout->rowCount();
        layout->addRow(tr("URL:"), urlEdit);

        // File field + Browse button
        auto *fileEdit = new QLineEdit(&dialog);
        fileEdit->setPlaceholderText(u"/path/to/file.fig"_s);
        auto *browseBtn = new QPushButton(tr("Browse..."), &dialog);
        auto *fileRowWidget = new QWidget(&dialog);
        auto *fileRowLayout = new QHBoxLayout(fileRowWidget);
        fileRowLayout->setContentsMargins(0, 0, 0, 0);
        fileRowLayout->addWidget(fileEdit, 1);
        fileRowLayout->addWidget(browseBtn);
        const int fileRow = layout->rowCount();
        layout->addRow(tr("File:"), fileRowWidget);
        QObject::connect(browseBtn, &QPushButton::clicked, &dialog, [&]() {
            const QString picked = QFileDialog::getOpenFileName(
                &dialog, tr("Open Figma File"), fileEdit->text(),
                tr("Figma files (*.fig *.figjam);;All files (*)"));
            if (!picked.isEmpty())
                fileEdit->setText(picked);
        });

        auto *apiKeyEdit = new QLineEdit(&dialog);
        apiKeyEdit->setEchoMode(QLineEdit::Password);
        QString savedKey = settings.value("apiKey"_L1).toString();
        if (savedKey.isEmpty()) {
            savedKey = qEnvironmentVariable("FIGMA_API_KEY");
            if (savedKey.isEmpty())
                savedKey = qEnvironmentVariable("FIGMA_ACCESS_TOKEN");
        }
        apiKeyEdit->setText(savedKey);
        const int apiKeyRow = layout->rowCount();
        layout->addRow(tr("API Key:"), apiKeyEdit);

        auto *scaleCombo = new QComboBox(&dialog);
        scaleCombo->addItem(u"1x"_s, 1);
        scaleCombo->addItem(u"2x"_s, 2);
        scaleCombo->addItem(u"3x"_s, 3);
        scaleCombo->addItem(u"4x"_s, 4);
        const int savedScale = settings.value("imageScale"_L1, 2).toInt();
        for (int i = 0; i < scaleCombo->count(); ++i) {
            if (scaleCombo->itemData(i).toInt() == savedScale) {
                scaleCombo->setCurrentIndex(i);
                break;
            }
        }
        const int scaleRow = layout->rowCount();
        layout->addRow(tr("Image Scale:"), scaleCombo);

        auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Import"));
        layout->addRow(buttonBox);

        QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        auto refreshVisibility = [&]() {
            const bool urlMode = modeUrl->isChecked();
            layout->setRowVisible(urlRow, urlMode);
            layout->setRowVisible(apiKeyRow, urlMode);
            layout->setRowVisible(scaleRow, urlMode);
            layout->setRowVisible(fileRow, !urlMode);

            const QString src = urlMode ? urlEdit->text().trimmed()
                                        : fileEdit->text().trimmed();
            const bool okEnabled = urlMode
                ? (!src.isEmpty() && !apiKeyEdit->text().trimmed().isEmpty())
                : !src.isEmpty();
            buttonBox->button(QDialogButtonBox::Ok)->setEnabled(okEnabled);
        };
        QObject::connect(modeUrl, &QRadioButton::toggled, &dialog, refreshVisibility);
        QObject::connect(modeFile, &QRadioButton::toggled, &dialog, refreshVisibility);
        QObject::connect(urlEdit, &QLineEdit::textChanged, &dialog, refreshVisibility);
        QObject::connect(fileEdit, &QLineEdit::textChanged, &dialog, refreshVisibility);
        QObject::connect(apiKeyEdit, &QLineEdit::textChanged, &dialog, refreshVisibility);
        refreshVisibility();

        if (dialog.exec() != QDialog::Accepted)
            return {};

        settings.setValue("sourceMode"_L1, modeUrl->isChecked() ? "url"_L1 : "file"_L1);

        // Local .fig short-circuit: decode once to enumerate pages, then show
        // the page selector (no API key / no network).
        if (modeFile->isChecked()) {
            const QString rawSource = fileEdit->text().trimmed();
            const QString localPath = QUrl(rawSource).isLocalFile()
                ? QUrl(rawSource).toLocalFile() : rawSource;

            QJsonObject fileJson;
            QHash<QString, QImage> embeddedImages;
            QGuiApplication::setOverrideCursor(Qt::BusyCursor);
            const bool ok = loadLocalFigFile(localPath, &fileJson, &embeddedImages);
            QGuiApplication::restoreOverrideCursor();
            if (!ok) {
                QMessageBox::critical(parent, tr("Import from Figma"),
                                      tr("Failed to read .fig file: %1").arg(errorMessage()));
                return {};
            }
            m_cachedFileKey = localPath;
            m_cachedFileJson = fileJson;

            QVariantMap options;
            options["source"_L1] = localPath;
            options["imageScale"_L1] = scaleCombo->currentData().toInt();

            const auto pages = fileJson["document"_L1].toObject()["children"_L1].toArray();
            QVariantList pageIndices;
            if (pages.size() <= 1) {
                pageIndices.append(0);
            } else {
                QDialog pageDlg(parent);
                pageDlg.setWindowTitle(tr("Select Pages"));
                pageDlg.setWindowIcon(icon());
                pageDlg.setMinimumSize(420, 400);
                auto *vLayout = new QVBoxLayout(&pageDlg);
                vLayout->addWidget(new QLabel(tr("Select pages to import:"), &pageDlg));
                auto *list = new QListWidget(&pageDlg);
                for (int i = 0; i < pages.size(); ++i) {
                    auto *it = new QListWidgetItem(pages[i].toObject()["name"_L1].toString(), list);
                    it->setData(Qt::UserRole, i);
                    it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
                    it->setCheckState(i == 0 ? Qt::Checked : Qt::Unchecked);
                }
                vLayout->addWidget(list);
                auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &pageDlg);
                bb->button(QDialogButtonBox::Ok)->setText(tr("Import Selected"));
                vLayout->addWidget(bb);
                QObject::connect(bb, &QDialogButtonBox::accepted, &pageDlg, &QDialog::accept);
                QObject::connect(bb, &QDialogButtonBox::rejected, &pageDlg, &QDialog::reject);
                if (pageDlg.exec() != QDialog::Accepted) return {};
                for (int i = 0; i < list->count(); ++i) {
                    if (list->item(i)->checkState() == Qt::Checked)
                        pageIndices.append(list->item(i)->data(Qt::UserRole).toInt());
                }
                if (pageIndices.isEmpty()) pageIndices.append(0);
            }
            options["pageIndices"_L1] = pageIndices;
            QVariantList pageNames;
            for (const auto &p : pages)
                pageNames.append(p.toObject()["name"_L1].toString());
            options["pageNames"_L1] = pageNames;
            return options;
        }

        // URL mode: proceed with the REST API fetch path below.
        const QString rawSource = urlEdit->text().trimmed();
        settings.setValue("apiKey"_L1, apiKeyEdit->text().trimmed());
        settings.setValue("imageScale"_L1, scaleCombo->currentData().toInt());

        QVariantMap options;
        options["source"_L1] = rawSource;
        options["apiKey"_L1] = apiKeyEdit->text().trimmed();
        options["imageScale"_L1] = scaleCombo->currentData().toInt();

        // Extract fileKey from source URL
        QString source = urlEdit->text().trimmed();
        QString fileKey = source;
        if (source.contains("figma.com"_L1)) {
            static const QRegularExpression reKey(u"figma\\.com/(?:file|design)/([a-zA-Z0-9]+)"_s);
            auto match = reKey.match(source);
            if (match.hasMatch())
                fileKey = match.captured(1);
        }
        if (fileKey.isEmpty()) {
            QMessageBox::critical(parent, tr("Import from Figma"),
                                  tr("No valid Figma file URL or key provided."));
            return {};
        }

        // Fetch file JSON
        FigmaClient client;
        client.setToken(apiKeyEdit->text().trimmed());

        QGuiApplication::setOverrideCursor(Qt::BusyCursor);
        const auto fileJson = client.fetchFile(fileKey);
        QGuiApplication::restoreOverrideCursor();

        if (fileJson.contains("error"_L1)) {
            const auto error = fileJson["error"_L1].toVariant().toString();
            QMessageBox::critical(parent, tr("Import from Figma"),
                                  tr("Figma API error: %1").arg(error));
            return {};
        }

        // Cache the file JSON for importFrom()
        m_cachedFileKey = fileKey;
        m_cachedFileJson = fileJson;

        // Enumerate pages
        const auto pages = fileJson["document"_L1].toObject()["children"_L1].toArray();
        QVariantList pageIndices;

        // Resolve node-id from URL to pre-select the matching page
        int preselectedPage = -1;
        {
            static const QRegularExpression reNodeId(u"[?&]node-id=([^&]+)"_s);
            auto nodeMatch = reNodeId.match(source);
            if (nodeMatch.hasMatch()) {
                const QString nodeId = nodeMatch.captured(1).replace('-'_L1, ':'_L1);
                for (int i = 0; i < pages.size(); ++i) {
                    if (pages[i].toObject()["id"_L1].toString() == nodeId) {
                        preselectedPage = i;
                        break;
                    }
                }
            }
        }

        if (pages.size() <= 1) {
            pageIndices.append(0);
        } else {
            // Fetch page thumbnails in one batch
            QStringList pageNodeIds;
            for (const auto &page : pages)
                pageNodeIds.append(page.toObject()["id"_L1].toString());

            QGuiApplication::setOverrideCursor(Qt::BusyCursor);
            const auto thumbResult = client.fetchImages(fileKey, pageNodeIds, u"png"_s, 1);
            const auto thumbUrls = thumbResult["images"_L1].toObject();

            QHash<QString, QImage> thumbnails;
            for (const auto &nodeId : pageNodeIds) {
                const auto url = thumbUrls[nodeId].toString();
                if (!url.isEmpty()) {
                    QImage img = client.downloadImage(QUrl(url));
                    if (!img.isNull())
                        thumbnails[nodeId] = img;
                }
            }
            QGuiApplication::restoreOverrideCursor();

            // --- Second dialog: Page selector with thumbnails ---
            QDialog pageDlg(parent);
            pageDlg.setWindowTitle(tr("Select Pages"));
            pageDlg.setWindowIcon(icon());
            pageDlg.setMinimumSize(640, 480);

            auto *vLayout = new QVBoxLayout(&pageDlg);
            vLayout->addWidget(new QLabel(tr("Select pages to import:"), &pageDlg));

            auto *listWidget = new QListWidget(&pageDlg);
            listWidget->setViewMode(QListView::IconMode);
            listWidget->setIconSize(QSize(160, 120));
            listWidget->setResizeMode(QListView::Adjust);
            listWidget->setSpacing(8);
            listWidget->setMovement(QListView::Static);

            for (int i = 0; i < pages.size(); ++i) {
                const auto pageObj = pages[i].toObject();
                const QString pageName = pageObj["name"_L1].toString();
                const QString nodeId = pageObj["id"_L1].toString();

                auto *item = new QListWidgetItem(pageName, listWidget);
                item->setData(Qt::UserRole, i);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                // Pre-select only the page matching node-id from URL, or all if no node-id
                item->setCheckState((preselectedPage < 0 || preselectedPage == i)
                                    ? Qt::Checked : Qt::Unchecked);

                if (thumbnails.contains(nodeId)) {
                    const auto &thumb = thumbnails[nodeId];
                    item->setIcon(QIcon(QPixmap::fromImage(
                        thumb.scaled(160, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
                } else {
                    QPixmap placeholder(160, 120);
                    placeholder.fill(Qt::lightGray);
                    item->setIcon(QIcon(placeholder));
                }
            }

            vLayout->addWidget(listWidget);

            auto *btnLayout = new QHBoxLayout();
            auto *selectAllBtn = new QPushButton(tr("Select All"), &pageDlg);
            auto *deselectAllBtn = new QPushButton(tr("Deselect All"), &pageDlg);
            btnLayout->addWidget(selectAllBtn);
            btnLayout->addWidget(deselectAllBtn);
            btnLayout->addStretch();
            vLayout->addLayout(btnLayout);

            auto *pageBtnBox = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &pageDlg);
            pageBtnBox->button(QDialogButtonBox::Ok)->setText(tr("Import Selected"));
            vLayout->addWidget(pageBtnBox);

            auto updateImportButton = [&]() {
                bool anyChecked = false;
                for (int i = 0; i < listWidget->count(); ++i) {
                    if (listWidget->item(i)->checkState() == Qt::Checked) {
                        anyChecked = true;
                        break;
                    }
                }
                pageBtnBox->button(QDialogButtonBox::Ok)->setEnabled(anyChecked);
            };

            QObject::connect(selectAllBtn, &QPushButton::clicked, &pageDlg, [&]() {
                for (int i = 0; i < listWidget->count(); ++i)
                    listWidget->item(i)->setCheckState(Qt::Checked);
                updateImportButton();
            });
            QObject::connect(deselectAllBtn, &QPushButton::clicked, &pageDlg, [&]() {
                for (int i = 0; i < listWidget->count(); ++i)
                    listWidget->item(i)->setCheckState(Qt::Unchecked);
                updateImportButton();
            });
            QObject::connect(listWidget, &QListWidget::itemChanged, &pageDlg, [&]() {
                updateImportButton();
            });

            QObject::connect(pageBtnBox, &QDialogButtonBox::accepted, &pageDlg, &QDialog::accept);
            QObject::connect(pageBtnBox, &QDialogButtonBox::rejected, &pageDlg, &QDialog::reject);

            if (pageDlg.exec() != QDialog::Accepted)
                return {};

            for (int i = 0; i < listWidget->count(); ++i) {
                if (listWidget->item(i)->checkState() == Qt::Checked)
                    pageIndices.append(listWidget->item(i)->data(Qt::UserRole).toInt());
            }
        }

        options["pageIndices"_L1] = pageIndices;

        QVariantList pageNames;
        for (const auto &page : pages)
            pageNames.append(page.toObject()["name"_L1].toString());
        options["pageNames"_L1] = pageNames;

        return options;
    }

    // Decode a local .fig file into a REST-API-shaped QJsonObject, plus a hash of
    // image payloads keyed by `imageRef` hex digest.
    // Returns false (and sets errorMessage) on failure.
    bool loadLocalFigFile(const QString &path, QJsonObject *outFileJson,
                          QHash<QString, QImage> *outImagesByHash) const
    {
        FigArchive::Archive arc;
        QString err;
        if (!FigArchive::readFigFile(path, &arc, &err)) {
            setErrorMessage(tr("Cannot read .fig file: %1").arg(err));
            return false;
        }
        const QByteArray schemaRaw = FigArchive::decompressAuto(arc.schemaChunk, &err);
        if (schemaRaw.isEmpty()) {
            setErrorMessage(tr("Cannot decompress schema chunk: %1").arg(err));
            return false;
        }
        const QByteArray dataRaw = FigArchive::decompressAuto(arc.dataChunk, &err);
        if (dataRaw.isEmpty()) {
            setErrorMessage(tr("Cannot decompress data chunk: %1").arg(err));
            return false;
        }
        FigKiwi::Schema schema;
        if (!FigKiwi::parseSchema(schemaRaw, &schema, &err)) {
            setErrorMessage(tr("Cannot parse kiwi schema: %1").arg(err));
            return false;
        }
        const QCborValue message = FigKiwi::decodeMessage(dataRaw, schema, &err);
        if (!message.isMap()) {
            setErrorMessage(tr("Cannot decode message: %1").arg(err));
            return false;
        }
        if (!FigToRest::convertMessageToFileJson(message, arc.metaJson, outFileJson, &err)) {
            setErrorMessage(tr("Cannot convert to file JSON: %1").arg(err));
            return false;
        }
        // Collect embedded images. Keys are full hex hash strings.
        for (auto it = arc.zipFiles.cbegin(); it != arc.zipFiles.cend(); ++it) {
            const QString &name = it.key();
            if (!name.startsWith("images/"_L1)) continue;
            const QString hash = name.mid(7);
            QImage img;
            if (img.loadFromData(it.value()))
                outImagesByHash->insert(hash, img);
        }
        return true;
    }

    bool importFrom(QPsdExporterTreeItemModel *model,
                    const QVariantMap &options) const override
    {
        resetCancellation();
        const QString source = options.value("source"_L1).toString();
        const int imageScale = options.value("imageScale"_L1, 2).toInt();
        int pageIndex = options.value("pageIndex"_L1, 0).toInt();

        // --- Local .fig file path ---
        const QString localPath = QUrl(source).isLocalFile()
            ? QUrl(source).toLocalFile()
            : (QFile::exists(source) ? source : QString());
        const bool isLocalFig = !localPath.isEmpty()
            && (localPath.endsWith(".fig"_L1, Qt::CaseInsensitive)
                || localPath.endsWith(".figjam"_L1, Qt::CaseInsensitive));

        if (isLocalFig) {
            QJsonObject fileJson;
            QHash<QString, QImage> embeddedImages;
            if (!loadLocalFigFile(localPath, &fileJson, &embeddedImages))
                return false;

            FigmaLayerTreeItemModel figmaModel;
            figmaModel.buildFromFigmaJson(fileJson, {}, pageIndex);

            // Resolve embedded images for IMAGE-fill nodes without any network traffic.
            const auto fillRefs = figmaModel.imageFillRefs();
            for (auto it = fillRefs.cbegin(); it != fillRefs.cend(); ++it) {
                const QString &imageRef = it.key();
                const QImage img = embeddedImages.value(imageRef);
                if (img.isNull()) continue;
                for (const auto &figmaId : it.value())
                    figmaModel.setNodeImage(figmaId, img);
            }
            // Regular image nodes (non-_imgbg) cannot be resolved without the render API;
            // they will render as empty placeholders. This is acceptable for MVP.

            auto *widgetModel = buildQPsdModel(&figmaModel, model);
            model->setSourceModel(widgetModel);
            model->setSize(figmaModel.size());
            model->setCanvasColor(figmaModel.canvasColor());

            const auto pages = fileJson["document"_L1].toObject()["children"_L1].toArray();
            const auto fileName = fileJson["name"_L1].toString();
            if (pages.size() > 1 && pageIndex < pages.size()) {
                const auto pageName = pages[pageIndex].toObject()["name"_L1].toString();
                model->setFileName(u"%1 - %2"_s.arg(fileName, pageName));
            } else {
                model->setFileName(fileName);
            }

            const auto hintFile = options.value("hintFile"_L1).toString();
            if (!hintFile.isEmpty())
                model->loadHints(hintFile);

            return true;
        }

        QString fileKey = source;
        QString nodeIdFromUrl;

        if (source.contains("figma.com"_L1)) {
            static const QRegularExpression reKey(u"figma\\.com/(?:file|design)/([a-zA-Z0-9]+)"_s);
            auto match = reKey.match(source);
            if (match.hasMatch())
                fileKey = match.captured(1);

            // Extract node-id from URL query parameter (e.g., ?node-id=0-1746)
            static const QRegularExpression reNodeId(u"[?&]node-id=([^&]+)"_s);
            auto nodeMatch = reNodeId.match(source);
            if (nodeMatch.hasMatch()) {
                // URL uses '-' separator, Figma API uses ':'
                nodeIdFromUrl = nodeMatch.captured(1).replace('-'_L1, ':'_L1);
            }
        }

        if (fileKey.isEmpty()) {
            qWarning() << "Figma import: fileKey is empty";
            setErrorMessage(tr("No Figma file URL or key provided."));
            return false;
        }

        FigmaClient client;
        // Route cancellation through the client so requestCancel() aborts the
        // reply the nested event loop is currently waiting on.
        QObject::connect(this, &QPsdImporterPlugin::cancelRequested,
                         &client, &FigmaClient::abort);
        const QString token = options.value("apiKey"_L1).toString();
        if (!token.isEmpty())
            client.setToken(token);

        if (client.token().isEmpty()) {
            qWarning() << "Figma import: API token is empty";
            setErrorMessage(tr("No Figma API token configured. Set it in Settings or the FIGMA_API_KEY environment variable."));
            return false;
        }

        auto checkCancelled = [this]() {
            if (isCancelRequested()) {
                setErrorMessage(tr("Import cancelled."));
                return true;
            }
            return false;
        };

        // Use cached file JSON if available for this fileKey
        QJsonObject fileJson;
        if (m_cachedFileKey == fileKey && !m_cachedFileJson.isEmpty()) {
            fileJson = m_cachedFileJson;
        } else {
            fileJson = client.fetchFile(fileKey);
            if (checkCancelled())
                return false;
            if (fileJson.contains("error"_L1)) {
                const auto error = fileJson["error"_L1].toVariant().toString();
                qWarning() << "Figma import: API error:" << error;
                setErrorMessage(tr("Figma API error: %1").arg(error));
                return false;
            }
            m_cachedFileKey = fileKey;
            m_cachedFileJson = fileJson;
        }

        // Resolve node-id from URL to page index (skip when caller explicitly set pageIndex)
        if (!nodeIdFromUrl.isEmpty()
            && !options.value("pageIndexExplicit"_L1, false).toBool()) {
            const auto document = fileJson["document"_L1].toObject();
            const auto pages = document["children"_L1].toArray();
            for (int i = 0; i < pages.size(); ++i) {
                if (pages[i].toObject()["id"_L1].toString() == nodeIdFromUrl) {
                    pageIndex = i;
                    break;
                }
            }
        }

        // Initialize disk image cache with version check
        const auto lastModified = fileJson["lastModified"_L1].toString();
        FigmaImageCache cache(fileKey, lastModified);

        FigmaLayerTreeItemModel figmaModel;
        figmaModel.buildFromFigmaJson(fileJson, {}, pageIndex);

        // Download images before building the widget model (cloning includes images)
        //
        // Two kinds of image downloads:
        // 1. Regular image nodes → Figma render API (renders the node as PNG)
        // 2. _imgbg nodes (IMAGE fill backgrounds) → Figma file images API (actual fill textures)
        const auto allImageIds = figmaModel.imageNodeIds();
        const auto fillRefs = figmaModel.imageFillRefs();  // imageRef → list of _imgbg figmaIds
        // Count total _imgbg nodes across all imageRefs
        int totalFillNodes = 0;
        for (auto it = fillRefs.begin(); it != fillRefs.end(); ++it)
            totalFillNodes += it.value().size();
        const int totalImages = allImageIds.size() + totalFillNodes;
        int downloaded = 0;

        if (totalImages > 0)
            reportProgress(0, totalImages);

        // 1. Download actual fill textures for _imgbg nodes via file images API
        if (!fillRefs.isEmpty()) {
            // Check which imageRefs need downloading (not in cache)
            QStringList uncachedRefs;
            for (auto it = fillRefs.begin(); it != fillRefs.end(); ++it) {
                const auto &imageRef = it.key();
                const auto &figmaIds = it.value();
                QImage cached = cache.fillImage(imageRef);
                if (!cached.isNull()) {
                    for (const auto &fid : figmaIds) {
                        figmaModel.setNodeImage(fid, cached);
                        reportProgress(++downloaded, totalImages);
                    }
                } else {
                    uncachedRefs.append(imageRef);
                }
            }

            // Only call API if there are uncached fills
            if (!uncachedRefs.isEmpty()) {
                const auto fileImagesResult = client.fetchFileImages(fileKey);
                if (checkCancelled())
                    return false;
                const auto meta = fileImagesResult["meta"_L1].toObject();
                const auto fileImages = meta["images"_L1].toObject();
                for (const auto &imageRef : std::as_const(uncachedRefs)) {
                    const auto imageUrl = fileImages[imageRef].toString();
                    if (!imageUrl.isEmpty()) {
                        QImage img = client.downloadImage(QUrl(imageUrl));
                        if (checkCancelled())
                            return false;
                        if (!img.isNull()) {
                            const auto &figmaIds = fillRefs[imageRef];
                            for (const auto &fid : figmaIds) {
                                figmaModel.setNodeImage(fid, img);
                                reportProgress(++downloaded, totalImages);
                            }
                            cache.saveFillImage(imageRef, img);
                            continue;
                        }
                    }
                    // Failed to download — still advance progress for all nodes
                    downloaded += fillRefs[imageRef].size();
                    reportProgress(downloaded, totalImages);
                }
            }
        }

        // 2. Download regular image nodes via render API
        if (!allImageIds.isEmpty()) {
            const int batchSize = 50;
            for (int i = 0; i < allImageIds.size(); i += batchSize) {
                const auto batch = allImageIds.mid(i, batchSize);

                // Check cache for all nodes in this batch
                QStringList uncachedIds;
                for (const auto &nid : batch) {
                    QImage cached = cache.nodeImage(nid, imageScale);
                    if (!cached.isNull()) {
                        figmaModel.setNodeImage(nid, cached);
                        reportProgress(++downloaded, totalImages);
                    } else {
                        uncachedIds.append(nid);
                    }
                }

                // Only call render API if there are uncached nodes in this batch
                if (!uncachedIds.isEmpty()) {
                    const auto imagesResult = client.fetchImages(fileKey, uncachedIds, u"png"_s, imageScale);
                    if (checkCancelled())
                        return false;
                    const auto images = imagesResult["images"_L1].toObject();
                    for (const auto &nid : uncachedIds) {
                        const auto imageUrl = images[nid].toString();
                        if (!imageUrl.isEmpty()) {
                            QImage img = client.downloadImage(QUrl(imageUrl));
                            if (checkCancelled())
                                return false;
                            if (!img.isNull()) {
                                figmaModel.setNodeImage(nid, img);
                                cache.saveNodeImage(nid, imageScale, img);
                            }
                        }
                        reportProgress(++downloaded, totalImages);
                    }
                }
            }
        }

        // Build QPsdWidgetTreeItemModel from the Figma model (clones all layer items)
        auto *widgetModel = buildQPsdModel(&figmaModel, model);

        model->setSourceModel(widgetModel);
        model->setSize(figmaModel.size());
        model->setCanvasColor(figmaModel.canvasColor());

        // Include page name in title for multi-page files
        const auto pages = fileJson["document"_L1].toObject()["children"_L1].toArray();
        const auto fileName = fileJson["name"_L1].toString();
        if (pages.size() > 1 && pageIndex < pages.size()) {
            const auto pageName = pages[pageIndex].toObject()["name"_L1].toString();
            model->setFileName(u"%1 - %2"_s.arg(fileName, pageName));
        } else {
            model->setFileName(fileName);
        }

        const auto hintFile = options.value("hintFile"_L1).toString();
        if (!hintFile.isEmpty())
            model->loadHints(hintFile);

        return true;
    }
};

#include "figma.moc"

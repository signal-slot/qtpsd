// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtCore/QCborMap>
#include <QtCore/QEventLoop>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QSettings>
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
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
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

    QImage downloadImage(const QUrl &url)
    {
        QNetworkRequest request(url);
        auto *reply = m_nam.get(request);
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        QImage image;
        if (reply->error() == QNetworkReply::NoError) {
            image.loadFromData(reply->readAll());
        } else {
            qWarning() << "Image download failed:" << reply->errorString();
        }
        reply->deleteLater();
        return image;
    }

private:
    QJsonObject getJson(const QUrl &url)
    {
        QNetworkRequest request(url);
        request.setRawHeader("X-Figma-Token", m_token.toUtf8());
        auto *reply = m_nam.get(request);

        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

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

    QStringList imageNodeIds() const
    {
        QStringList ids;
        QSet<QString> seen;
        for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
            if (it->layerItem && it->layerItem->type() == QPsdAbstractLayerItem::Image) {
                QString fid = it->figmaId;
                if (fid.endsWith("_imgbg"_L1))
                    fid.chop(6);
                if (!seen.contains(fid)) {
                    ids.append(fid);
                    seen.insert(fid);
                }
            }
        }
        return ids;
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
    };

    quintptr processNode(const QJsonObject &nodeJson, quintptr parentId, qreal inheritedOpacity = 1.0)
    {
        const QString figmaId = nodeJson["id"_L1].toString();
        const QString name = nodeJson["name"_L1].toString();
        const QString nodeType = nodeJson["type"_L1].toString();
        const bool visible = nodeJson.value("visible"_L1).toBool(true);
        const qreal opacity = nodeJson.value("opacity"_L1).toDouble(1.0) * inheritedOpacity;

        if (!visible) return 0;

        quint32 layerId = qHash(figmaId);

        const auto bbox = nodeJson["absoluteBoundingBox"_L1].toObject();
        QRect rect(qRound(bbox["x"_L1].toDouble()) - m_canvasOrigin.x(),
                   qRound(bbox["y"_L1].toDouble()) - m_canvasOrigin.y(),
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
            const auto characters = nodeJson["characters"_L1].toString();
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
                for (const auto &seg : segments) {
                    const auto segObj = seg.toObject();
                    QPsdTextLayerItem::Run run;
                    const auto segFills = segObj["fills"_L1].toArray();
                    run.text = applyTextCase(segObj["characters"_L1].toString(),
                                             segObj["textCase"_L1].toString());
                    run.font = fontFromStyle(segObj);
                    run.originalFontName = segObj["fontFamily"_L1].toString();
                    run.color = !segFills.isEmpty() ? colorFromFills(segFills) : colorFromFills(fills);
                    run.alignment = alignmentFromStyle(style);
                    if (segObj.contains("lineHeightPx"_L1))
                        run.lineHeight = segObj["lineHeightPx"_L1].toDouble() * dpiScale;
                    runs.append(run);
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
                    run.text = applyTextCase(characters, style["textCase"_L1].toString());
                    run.font = fontFromStyle(style);
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

                        run.text = applyTextCase(rawText, effectiveStyle["textCase"_L1].toString());
                        run.font = fontFromStyle(effectiveStyle);
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
                        run.text = applyTextCase(characters.mid(pos), style["textCase"_L1].toString());
                        run.font = fontFromStyle(style);
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
            QFontMetrics fm(adjustedFont);
            const qreal capHeightOffset = fm.ascent() - fm.capHeight();

            const auto textAutoResize = style["textAutoResize"_L1].toString();
            if (textAutoResize == "NONE"_L1 || textAutoResize == "HEIGHT"_L1) {
                // Fixed-width text box: use ParagraphText with word wrapping
                textItem->setTextType(QPsdTextLayerItem::TextType::ParagraphText);
                textItem->setTextFrame(QRectF(rect.x(), rect.y() + capHeightOffset,
                                              rect.width(), rect.height()));
            } else {
                // Auto-resize text: use ParagraphText but with extra width to
                // prevent unwanted wrapping from font metric differences
                textItem->setTextType(QPsdTextLayerItem::TextType::ParagraphText);
                textItem->setTextFrame(QRectF(rect.x(), rect.y() + capHeightOffset,
                                              rect.width() + 100, rect.height()));
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
                && (nodeType == "FRAME"_L1 || nodeType == "COMPONENT"_L1);
            folderItem->setRect(rect);

            // Extract corner radius for frames/components
            const qreal frameCornerRadius = nodeJson["cornerRadius"_L1].toDouble(0);

            if (nodeJson.value("clipsContent"_L1).toBool(false)) {
                QPsdAbstractLayerItem::PathInfo clipMask;
                clipMask.rect = QRectF(0, 0, rect.width(), rect.height());
                if (frameCornerRadius > 0) {
                    clipMask.type = QPsdAbstractLayerItem::PathInfo::RoundedRectangle;
                    clipMask.radius = frameCornerRadius;
                } else {
                    clipMask.type = QPsdAbstractLayerItem::PathInfo::Rectangle;
                }
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

            // Keep folder's own opacity — QPsdScene applies it via QGraphicsOpacityEffect
            // which composites children first, then fades the group as a whole
            folderItem->setOpacity(nodeJson.value("opacity"_L1).toDouble(1.0));
            node.layerItem = folderItem;
            node.folderType = FolderType::OpenFolder;

            const auto children = nodeJson["children"_L1].toArray();
            for (int ci = children.size() - 1; ci >= 0; --ci) {
                quintptr childId = processNode(children.at(ci).toObject(), nodeId, 1.0);
                if (childId != 0)
                    node.childIds.append(childId);
            }

            if (needsGradientBg) {
                const auto frameBrush = brushFromFigmaFills(fills, rect);
                quintptr bgId = m_nextId++;
                Node bgNode;
                bgNode.id = bgId;
                bgNode.parentId = nodeId;
                bgNode.figmaId = figmaId + "_bg"_L1;
                bgNode.name = name + " background"_L1;
                bgNode.layerId = qHash(bgNode.figmaId);
                bgNode.rect = rect;
                bgNode.blendMode = QPsdBlend::Normal;

                auto *bgShape = new QPsdShapeLayerItem();
                bgShape->setId(bgNode.layerId);
                bgShape->setName(bgNode.name);
                bgShape->setVisible(true);
                bgShape->setOpacity(1.0);
                bgShape->setRect(rect);
                bgShape->setBrush(frameBrush);
                QPsdAbstractLayerItem::PathInfo bgPath;
                bgPath.rect = QRectF(0, 0, rect.width(), rect.height());
                if (frameCornerRadius > 0) {
                    bgPath.type = QPsdAbstractLayerItem::PathInfo::RoundedRectangle;
                    bgPath.radius = frameCornerRadius;
                } else {
                    bgPath.type = QPsdAbstractLayerItem::PathInfo::Rectangle;
                }
                bgShape->setPathInfo(bgPath);

                bgNode.layerItem = bgShape;
                bgNode.folderType = FolderType::NotFolder;

                m_nodes.insert(bgId, bgNode);
                m_nodeIdMap.insert(bgNode.figmaId, bgId);
                m_layerItems.append(bgShape);
                node.childIds.append(bgId);
            }

            if (hasImageFill) {
                quintptr imgBgId = m_nextId++;
                Node imgBgNode;
                imgBgNode.id = imgBgId;
                imgBgNode.parentId = nodeId;
                imgBgNode.figmaId = figmaId + "_imgbg"_L1;
                imgBgNode.name = name + " image"_L1;
                imgBgNode.layerId = qHash(imgBgNode.figmaId);
                imgBgNode.rect = rect;
                imgBgNode.blendMode = QPsdBlend::Normal;

                auto *imgItem = new QPsdImageLayerItem();
                imgItem->setId(imgBgNode.layerId);
                imgItem->setName(imgBgNode.name);
                imgItem->setVisible(true);
                imgItem->setOpacity(1.0);
                imgItem->setRect(rect);

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

            const auto brush = brushFromFigmaFills(fills, rect);
            if (brush.style() != Qt::NoBrush)
                shapeItem->setBrush(brush);

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
                    const qreal tl = radiiArray[0].toDouble();
                    const qreal tr = radiiArray[1].toDouble();
                    const qreal br = radiiArray[2].toDouble();
                    const qreal bl = radiiArray[3].toDouble();
                    if (tl != tr || tr != br || br != bl) {
                        pathInfo.type = QPsdAbstractLayerItem::PathInfo::Path;
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
                        pathInfo.path = pp;
                    } else if (tl > 0) {
                        pathInfo.type = QPsdAbstractLayerItem::PathInfo::RoundedRectangle;
                        pathInfo.radius = tl;
                    } else {
                        pathInfo.type = QPsdAbstractLayerItem::PathInfo::Rectangle;
                    }
                } else if (cornerRadius > 0) {
                    pathInfo.type = QPsdAbstractLayerItem::PathInfo::RoundedRectangle;
                    pathInfo.radius = cornerRadius;
                } else {
                    pathInfo.type = QPsdAbstractLayerItem::PathInfo::Rectangle;
                }
                pathInfo.rect = QRectF(0, 0, w, h);
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
            // Apply rotation for shapes built from basic parameters.
            // Skip for fillGeometry-based paths (POLYGON, STAR, VECTOR, etc.)
            // because their geometry already reflects the node's transforms.
            const bool needsRotation = (nodeType == "RECTANGLE"_L1
                                        || nodeType == "ELLIPSE"_L1);
            const qreal rotationRad = nodeJson.value("rotation"_L1).toDouble(0.0);
            const qreal rotation = rotationRad * 180.0 / M_PI; // convert to degrees
            if (needsRotation && std::abs(rotation) > 0.01
                && pathInfo.type != QPsdAbstractLayerItem::PathInfo::None) {
                // Get original (pre-rotation) dimensions from Figma's size property
                const auto sizeObj = nodeJson["size"_L1].toObject();
                qreal origW = sizeObj["x"_L1].toDouble(0);
                qreal origH = sizeObj["y"_L1].toDouble(0);
                if (origW <= 0 || origH <= 0) {
                    // Fallback: compute from bbox and rotation angle
                    const qreal rad = std::abs(rotationRad);
                    const qreal c = std::abs(std::cos(rad));
                    const qreal s = std::abs(std::sin(rad));
                    const qreal denom = c * c - s * s;
                    if (std::abs(denom) > 0.001) {
                        origW = (w * c - h * s) / denom;
                        origH = (h * c - w * s) / denom;
                    } else {
                        // ~45° case, assume square
                        origW = origH = w / std::sqrt(2.0);
                    }
                }

                // Build the original path at pre-rotation size
                QPainterPath pp;
                if (pathInfo.type == QPsdAbstractLayerItem::PathInfo::Rectangle) {
                    pp.addRect(0, 0, origW, origH);
                } else if (pathInfo.type == QPsdAbstractLayerItem::PathInfo::RoundedRectangle) {
                    pp.addRoundedRect(0, 0, origW, origH, pathInfo.radius, pathInfo.radius);
                } else {
                    // Path type (including ellipses already converted to paths)
                    pp = pathInfo.path;
                    origW = w;
                    origH = h;
                }

                // Rotate around shape center, then center in bounding box
                QTransform t;
                t.translate(w / 2.0, h / 2.0);
                t.rotate(-rotation);
                t.translate(-origW / 2.0, -origH / 2.0);

                pathInfo.type = QPsdAbstractLayerItem::PathInfo::Path;
                pathInfo.path = t.map(pp);
                pathInfo.rect = QRectF(0, 0, w, h);
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
        const auto letterSpacing = style["letterSpacing"_L1].toDouble(0);
        if (qAbs(letterSpacing) > 0.01)
            font.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
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
            QLinearGradient gradient(x0, y0, x1, y1);
            gradient.setStops(stops);
            // Figma interpolates gradient colors in straight (non-premultiplied) alpha
            gradient.setInterpolationMode(QGradient::ComponentInterpolation);
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
                                // Straight alpha interpolation (matches Figma)
                                c = QColor::fromRgbF(
                                    c0.redF() + (c1.redF() - c0.redF()) * f,
                                    c0.greenF() + (c1.greenF() - c0.greenF()) * f,
                                    c0.blueF() + (c1.blueF() - c0.blueF()) * f,
                                    c0.alphaF() + (c1.alphaF() - c0.alphaF()) * f);
                                break;
                            }
                        }
                    }
                    line[px] = c.rgba();
                }
            }
            return QBrush(img);
        } else if (type == "GRADIENT_RADIAL"_L1) {
            const qreal radius = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
            QRadialGradient gradient(x0, y0, radius);
            gradient.setStops(stops);
            gradient.setInterpolationMode(QGradient::ComponentInterpolation);
            return QBrush(gradient);
        } else if (type == "GRADIENT_ANGULAR"_L1) {
            const qreal angle = std::atan2(y1 - y0, x1 - x0) * 180.0 / M_PI;
            QConicalGradient gradient(x0, y0, angle);
            // Figma sweeps CW, Qt sweeps CCW: reverse stops to flip direction
            QGradientStops cwStops;
            for (int i = stops.size() - 1; i >= 0; --i) {
                qreal pos = 1.0 - stops[i].first;
                cwStops.append({pos, stops[i].second});
            }
            gradient.setStops(cwStops);
            gradient.setInterpolationMode(QGradient::ComponentInterpolation);
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
                              QHash<quint32, const QPsdAbstractLayerItem *> &layerItemMap)
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

            flattenFigmaTree(figmaModel, index, records, layerItemMap);

            QPsdLayerRecord folderRecord;
            folderRecord.setRect(rect);
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
            leafRecord.setRect(rect);
            leafRecord.setName(name.toUtf8());
            leafRecord.setBlendMode(blendMode == QPsdBlend::PassThrough ? QPsdBlend::Normal : blendMode);
            leafRecord.setOpacity(qRound(item->opacity() * 255.0));
            leafRecord.setFlags(0x00);
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
                    auto *clone = cloneLayerItem(layerItemMap.value(lid), *rec);
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
    QString name() const override { return u"&Figma..."_s; }

    QVariantMap execImportDialog(QWidget *parent) const override
    {
        QDialog dialog(parent);
        dialog.setWindowTitle(tr("Import from Figma"));
        dialog.setMinimumWidth(480);

        auto *layout = new QFormLayout(&dialog);

        auto *urlEdit = new QLineEdit(&dialog);
        urlEdit->setPlaceholderText(u"https://www.figma.com/design/..."_s);
        layout->addRow(tr("Figma URL:"), urlEdit);

        auto *apiKeyEdit = new QLineEdit(&dialog);
        apiKeyEdit->setEchoMode(QLineEdit::Password);

        QSettings settings;
        settings.beginGroup("Importers/Figma"_L1);
        QString savedKey = settings.value("apiKey"_L1).toString();
        if (savedKey.isEmpty()) {
            savedKey = qEnvironmentVariable("FIGMA_API_KEY");
            if (savedKey.isEmpty())
                savedKey = qEnvironmentVariable("FIGMA_ACCESS_TOKEN");
        }
        apiKeyEdit->setText(savedKey);
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
        layout->addRow(tr("Image Scale:"), scaleCombo);

        auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Import"));
        layout->addRow(buttonBox);

        QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        auto updateOkButton = [&]() {
            buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
                !urlEdit->text().trimmed().isEmpty() && !apiKeyEdit->text().trimmed().isEmpty());
        };
        QObject::connect(urlEdit, &QLineEdit::textChanged, &dialog, updateOkButton);
        QObject::connect(apiKeyEdit, &QLineEdit::textChanged, &dialog, updateOkButton);
        updateOkButton();

        if (dialog.exec() != QDialog::Accepted)
            return {};

        settings.setValue("apiKey"_L1, apiKeyEdit->text().trimmed());
        settings.setValue("imageScale"_L1, scaleCombo->currentData().toInt());

        QVariantMap options;
        options["source"_L1] = urlEdit->text().trimmed();
        options["apiKey"_L1] = apiKeyEdit->text().trimmed();
        options["imageScale"_L1] = scaleCombo->currentData().toInt();
        return options;
    }

    bool importFrom(QPsdExporterTreeItemModel *model,
                    const QVariantMap &options) const override
    {
        const QString source = options.value("source"_L1).toString();
        const int imageScale = options.value("imageScale"_L1, 2).toInt();

        QString fileKey = source;
        QString nodeId;

        if (source.contains("figma.com"_L1)) {
            static const QRegularExpression reKey(u"figma\\.com/(?:file|design)/([a-zA-Z0-9]+)"_s);
            auto match = reKey.match(source);
            if (match.hasMatch())
                fileKey = match.captured(1);

            static const QRegularExpression reNode(u"node-id=([0-9]+[-:][0-9]+)"_s);
            auto nodeMatch = reNode.match(source);
            if (nodeMatch.hasMatch())
                nodeId = nodeMatch.captured(1).replace('-'_L1, ':'_L1);
        }

        if (fileKey.isEmpty())
            return false;

        FigmaClient client;
        const QString token = options.value("apiKey"_L1).toString();
        if (!token.isEmpty())
            client.setToken(token);

        if (client.token().isEmpty())
            return false;

        const auto fileJson = client.fetchFile(fileKey);
        if (fileJson.contains("error"_L1))
            return false;

        // Enumerate pages and let user choose if multiple exist
        int pageIndex = 0;
        const auto pages = fileJson["document"_L1].toObject()["children"_L1].toArray();
        if (options.contains("pageIndex"_L1)) {
            pageIndex = options.value("pageIndex"_L1).toInt();
            nodeId.clear();
        } else if (pages.size() > 1) {
            QDialog pageDlg;
            pageDlg.setWindowTitle(tr("Select Page"));
            auto *pageLayout = new QFormLayout(&pageDlg);
            auto *pageCombo = new QComboBox(&pageDlg);
            for (int i = 0; i < pages.size(); ++i)
                pageCombo->addItem(pages[i].toObject()["name"_L1].toString(), i);
            pageLayout->addRow(tr("Page:"), pageCombo);
            auto *pageBtnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &pageDlg);
            pageLayout->addRow(pageBtnBox);
            QObject::connect(pageBtnBox, &QDialogButtonBox::accepted, &pageDlg, &QDialog::accept);
            QObject::connect(pageBtnBox, &QDialogButtonBox::rejected, &pageDlg, &QDialog::reject);
            if (pageDlg.exec() != QDialog::Accepted)
                return false;
            pageIndex = pageCombo->currentData().toInt();
            nodeId.clear();  // Page selected explicitly — import all frames
        }

        FigmaLayerTreeItemModel figmaModel;
        figmaModel.buildFromFigmaJson(fileJson, nodeId, pageIndex);

        // Download images before building the widget model (cloning includes images)
        const auto allImageIds = figmaModel.imageNodeIds();
        if (!allImageIds.isEmpty()) {
            const int batchSize = 50;
            for (int i = 0; i < allImageIds.size(); i += batchSize) {
                const auto batch = allImageIds.mid(i, batchSize);
                const auto imagesResult = client.fetchImages(fileKey, batch, u"png"_s, imageScale);
                const auto images = imagesResult["images"_L1].toObject();
                for (const auto &nid : batch) {
                    const auto imageUrl = images[nid].toString();
                    if (!imageUrl.isEmpty()) {
                        QImage img = client.downloadImage(QUrl(imageUrl));
                        if (!img.isNull())
                            figmaModel.setNodeImage(nid, img);
                    }
                }
            }
        }

        // Build QPsdWidgetTreeItemModel from the Figma model (clones all layer items)
        auto *widgetModel = buildQPsdModel(&figmaModel, model);

        model->setSourceModel(widgetModel);
        model->setSize(figmaModel.size());

        const auto fileName = fileJson["name"_L1].toString();
        model->setFileName(fileName);

        const auto hintFile = options.value("hintFile"_L1).toString();
        if (!hintFile.isEmpty())
            model->loadHints(hintFile);

        return true;
    }
};

#include "figma.moc"

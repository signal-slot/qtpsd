// Minimal test to exercise the shape injection + rendering pipeline
// Tests QPsdWidgetTreeItemModel with manually-created records and injected shapes.

#include <QtTest/QtTest>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

#include <QtPsdCore/QPsdParser>
#include <QtPsdCore/QPsdFileHeader>
#include <QtPsdCore/QPsdLayerInfo>
#include <QtPsdCore/QPsdLayerAndMaskInformation>
#include <QtPsdCore/QPsdChannelImageData>
#include <QtPsdCore/QPsdSectionDividerSetting>
#include <QtPsdCore/QPsdLayerRecord>

#include <QtPsdGui/QPsdAbstractLayerItem>
#include <QtPsdGui/QPsdShapeLayerItem>
#include <QtPsdGui/QPsdFolderLayerItem>

#include <QtPsdWidget/QPsdView>
#include <QtPsdWidget/QPsdWidgetTreeItemModel>

using namespace Qt::StringLiterals;

class tst_QPsdShapeInject : public QObject
{
    Q_OBJECT

private slots:
    void testRectangle();
    void testRoundedRectangle();
    void testEllipse();
    void testAllShapes();
    void testFolderWrappedShapes();
    void testMultipleArtboards();

private:
    struct ShapeSpec {
        quint32 layerId;
        QString name;
        QRect rect;
        QPsdAbstractLayerItem::PathInfo pathInfo;
        QBrush brush;
        QPen pen;
    };

    struct FolderSpec {
        quint32 layerId;
        QString name;
        QRect rect;
        QList<ShapeSpec> children;
    };

    QImage buildAndRender(const QSize &canvasSize, const QList<ShapeSpec> &shapes);
    QImage buildAndRenderWithFolders(const QSize &canvasSize, const QList<FolderSpec> &folders);
    bool hasNonWhitePixels(const QImage &img) const;
};

QImage tst_QPsdShapeInject::buildAndRender(const QSize &canvasSize, const QList<ShapeSpec> &shapes)
{
    // Step 1: Create PSD records for the shapes
    QList<QPsdLayerRecord> records;
    QHash<quint32, QPsdAbstractLayerItem *> layerItems;

    for (const auto &spec : shapes) {
        QPsdLayerRecord rec;
        rec.setRect(QRect(spec.rect));
        rec.setName(spec.name.toUtf8());
        rec.setBlendMode(QPsdBlend::Normal);
        rec.setOpacity(255);
        rec.setFlags(0x00);

        QHash<QByteArray, QVariant> ali;
        ali["lyid"] = spec.layerId;
        ali["luni"] = spec.name;
        ali["vscg"] = QVariant();
        rec.setAdditionalLayerInformation(ali);
        records.append(rec);

        auto *item = new QPsdShapeLayerItem();
        item->setId(spec.layerId);
        item->setName(spec.name);
        item->setVisible(true);
        item->setOpacity(1.0);
        item->setRect(spec.rect);
        item->setBrush(spec.brush);
        item->setPen(spec.pen);
        item->setPathInfo(spec.pathInfo);
        layerItems.insert(spec.layerId, item);
    }

    // Step 2: Build parser data
    QPsdFileHeader header;
    header.setWidth(canvasSize.width());
    header.setHeight(canvasSize.height());
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

    // Step 3: Create model from parser
    auto *model = new QPsdWidgetTreeItemModel(this);
    model->fromParser(parser);

    qDebug() << "Model rowCount:" << model->rowCount();

    // Step 4: Inject shape layer items (same as buildQPsdModel)
    std::function<void(const QModelIndex &)> injectItems = [&](const QModelIndex &parentIdx) {
        for (int row = 0; row < model->rowCount(parentIdx); ++row) {
            const QModelIndex index = model->index(row, 0, parentIdx);
            const quint32 lid = model->layerId(index);
            qDebug() << "  Row" << row << "lid=" << lid << "name=" << model->layerName(index);
            if (layerItems.contains(lid)) {
                const QPsdLayerRecord *rec = model->layerRecord(index);
                if (rec) {
                    auto *src = layerItems.value(lid);
                    auto *clone = new QPsdShapeLayerItem(*rec);
                    clone->setBrush(static_cast<QPsdShapeLayerItem *>(src)->brush());
                    clone->setPen(static_cast<QPsdShapeLayerItem *>(src)->pen());
                    clone->setPathInfo(static_cast<QPsdShapeLayerItem *>(src)->pathInfo());
                    model->setLayerItem(index, clone);

                    qDebug() << "    INJECTED pathInfo.type=" << clone->pathInfo().type
                             << "brush=" << clone->brush().style()
                             << "rect=" << clone->rect();
                }
            }
            injectItems(index);
        }
    };
    injectItems(QModelIndex());

    // Step 5: Render via QPsdView
    QPsdView view;
    view.setAttribute(Qt::WA_TranslucentBackground);
    view.resize(canvasSize.grownBy({1, 1, 1, 1}));
    view.viewport()->resize(canvasSize);
    view.setModel(model);

    QImage rendered(canvasSize, QImage::Format_ARGB32);
    rendered.fill(Qt::white);

    QPainter painter(&rendered);
    view.render(&painter);
    painter.end();

    // Cleanup
    qDeleteAll(layerItems);
    delete model;

    return rendered;
}

QImage tst_QPsdShapeInject::buildAndRenderWithFolders(const QSize &canvasSize, const QList<FolderSpec> &folders)
{
    // Build records in PSD order (like flattenFigmaTree):
    // For each folder: closeRecord, children records, folderRecord
    QList<QPsdLayerRecord> records;
    QHash<quint32, QPsdAbstractLayerItem *> layerItems;

    for (const auto &folder : folders) {
        // Close record (BoundingSectionDivider)
        {
            QPsdLayerRecord closeRec;
            closeRec.setName(QByteArray("</Layer group>"));
            closeRec.setFlags(0x00);
            QHash<QByteArray, QVariant> closeALI;
            QPsdSectionDividerSetting closeSetting;
            closeSetting.setType(QPsdSectionDividerSetting::BoundingSectionDivider);
            closeALI["lsdk"] = QVariant::fromValue(closeSetting);
            closeRec.setAdditionalLayerInformation(closeALI);
            records.append(closeRec);
        }

        // Child shape records (reverse order like flattenFigmaTree)
        for (int i = folder.children.size() - 1; i >= 0; --i) {
            const auto &spec = folder.children.at(i);
            QPsdLayerRecord leafRec;
            leafRec.setRect(spec.rect);
            leafRec.setName(spec.name.toUtf8());
            leafRec.setBlendMode(QPsdBlend::Normal);
            leafRec.setOpacity(255);
            leafRec.setFlags(0x00);
            QHash<QByteArray, QVariant> leafALI;
            leafALI["lyid"] = spec.layerId;
            leafALI["luni"] = spec.name;
            leafALI["vscg"] = QVariant();
            leafRec.setAdditionalLayerInformation(leafALI);
            records.append(leafRec);

            auto *item = new QPsdShapeLayerItem();
            item->setId(spec.layerId);
            item->setName(spec.name);
            item->setVisible(true);
            item->setOpacity(1.0);
            item->setRect(spec.rect);
            item->setBrush(spec.brush);
            item->setPen(spec.pen);
            item->setPathInfo(spec.pathInfo);
            layerItems.insert(spec.layerId, item);
        }

        // Folder record (OpenFolder)
        {
            QPsdLayerRecord folderRec;
            folderRec.setRect(folder.rect);
            folderRec.setName(folder.name.toUtf8());
            folderRec.setBlendMode(QPsdBlend::Normal);
            folderRec.setOpacity(255);
            folderRec.setFlags(0x00);
            QHash<QByteArray, QVariant> folderALI;
            folderALI["lyid"] = folder.layerId;
            folderALI["luni"] = folder.name;
            QPsdSectionDividerSetting folderSetting;
            folderSetting.setType(QPsdSectionDividerSetting::OpenFolder);
            folderALI["lsdk"] = QVariant::fromValue(folderSetting);
            folderRec.setAdditionalLayerInformation(folderALI);
            records.append(folderRec);

            auto *folderItem = new QPsdFolderLayerItem();
            folderItem->setId(folder.layerId);
            folderItem->setName(folder.name);
            folderItem->setVisible(true);
            folderItem->setOpacity(1.0);
            folderItem->setRect(folder.rect);
            folderItem->setIsOpened(true);
            folderItem->setArtboardRect(folder.rect);
            layerItems.insert(folder.layerId, folderItem);
        }
    }

    // Build parser data
    QPsdFileHeader header;
    header.setWidth(canvasSize.width());
    header.setHeight(canvasSize.height());
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

    // Create model from parser
    auto *model = new QPsdWidgetTreeItemModel(this);
    model->fromParser(parser);

    qDebug() << "FolderModel rowCount:" << model->rowCount();

    // Inject layer items (same as buildQPsdModel)
    std::function<void(const QModelIndex &)> injectItems = [&](const QModelIndex &parentIdx) {
        for (int row = 0; row < model->rowCount(parentIdx); ++row) {
            const QModelIndex index = model->index(row, 0, parentIdx);
            const quint32 lid = model->layerId(index);
            const auto folderType = model->folderType(index);
            qDebug() << "  Row" << row << "lid=" << lid << "name=" << model->layerName(index)
                     << "folderType=" << int(folderType);
            if (layerItems.contains(lid)) {
                const QPsdLayerRecord *rec = model->layerRecord(index);
                if (rec) {
                    auto *src = layerItems.value(lid);
                    QPsdAbstractLayerItem *clone = nullptr;

                    if (src->type() == QPsdAbstractLayerItem::Shape) {
                        auto *s = static_cast<QPsdShapeLayerItem *>(src);
                        auto *d = new QPsdShapeLayerItem(*rec);
                        d->setBrush(s->brush());
                        d->setPen(s->pen());
                        d->setPathInfo(s->pathInfo());
                        clone = d;
                        qDebug() << "    INJECTED shape pathInfo.type=" << d->pathInfo().type
                                 << "brush=" << d->brush().style()
                                 << "rect=" << d->rect();
                    } else if (src->type() == QPsdAbstractLayerItem::Folder) {
                        auto *s = static_cast<QPsdFolderLayerItem *>(src);
                        auto *d = new QPsdFolderLayerItem(*rec, s->isOpened());
                        d->setArtboardRect(s->artboardRect());
                        d->setArtboardBackground(s->artboardBackground());
                        d->setVectorMask(s->vectorMask());
                        clone = d;
                        qDebug() << "    INJECTED folder rect=" << d->rect()
                                 << "artboardRect=" << d->artboardRect();
                    }

                    if (clone)
                        model->setLayerItem(index, clone);
                }
            }
            injectItems(index);
        }
    };
    injectItems(QModelIndex());

    // Verify: dump all layer items
    std::function<void(const QModelIndex &, int)> dumpTree = [&](const QModelIndex &parentIdx, int depth) {
        for (int row = 0; row < model->rowCount(parentIdx); ++row) {
            const auto index = model->index(row, 0, parentIdx);
            const auto *layer = model->layerItem(index);
            if (layer) {
                qDebug().noquote() << QString(depth * 2, ' ')
                         << "VERIFY" << layer->name() << "type=" << layer->type()
                         << "visible=" << layer->isVisible() << "rect=" << layer->rect();
                if (layer->type() == QPsdAbstractLayerItem::Shape) {
                    auto *sl = static_cast<const QPsdShapeLayerItem *>(layer);
                    qDebug().noquote() << QString(depth * 2 + 2, ' ')
                             << "pathInfo.type=" << sl->pathInfo().type
                             << "brush=" << sl->brush().style();
                }
            }
            dumpTree(index, depth + 1);
        }
    };
    dumpTree(QModelIndex(), 0);

    // Render via QPsdView
    QPsdView view;
    view.setAttribute(Qt::WA_TranslucentBackground);
    view.resize(canvasSize.grownBy({1, 1, 1, 1}));
    view.viewport()->resize(canvasSize);
    view.setModel(model);

    QImage rendered(canvasSize, QImage::Format_ARGB32);
    rendered.fill(Qt::white);

    QPainter painter(&rendered);
    view.render(&painter);
    painter.end();

    // Cleanup
    qDeleteAll(layerItems);
    delete model;

    return rendered;
}

bool tst_QPsdShapeInject::hasNonWhitePixels(const QImage &img) const
{
    const QImage a = img.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < a.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(a.constScanLine(y));
        for (int x = 0; x < a.width(); ++x) {
            int r = qRed(line[x]), g = qGreen(line[x]), b = qBlue(line[x]);
            if (r < 240 || g < 240 || b < 240)
                return true;
        }
    }
    return false;
}

void tst_QPsdShapeInject::testRectangle()
{
    QPsdAbstractLayerItem::PathInfo pathInfo;
    pathInfo.type = QPsdAbstractLayerItem::PathInfo::Rectangle;
    pathInfo.rect = QRectF(0, 0, 100, 100);

    ShapeSpec spec;
    spec.layerId = 1001;
    spec.name = "TestRect"_L1;
    spec.rect = QRect(50, 50, 100, 100);
    spec.pathInfo = pathInfo;
    spec.brush = QBrush(Qt::red);
    spec.pen = Qt::NoPen;

    QImage img = buildAndRender(QSize(200, 200), {spec});
    img.save("/tmp/test_rectangle.png"_L1);
    QVERIFY2(hasNonWhitePixels(img), "Rectangle should render non-white pixels");
}

void tst_QPsdShapeInject::testRoundedRectangle()
{
    QPsdAbstractLayerItem::PathInfo pathInfo;
    pathInfo.type = QPsdAbstractLayerItem::PathInfo::RoundedRectangle;
    pathInfo.rect = QRectF(0, 0, 100, 100);
    pathInfo.radius = 20;

    ShapeSpec spec;
    spec.layerId = 2001;
    spec.name = "TestRoundedRect"_L1;
    spec.rect = QRect(50, 50, 100, 100);
    spec.pathInfo = pathInfo;
    spec.brush = QBrush(Qt::green);
    spec.pen = Qt::NoPen;

    QImage img = buildAndRender(QSize(200, 200), {spec});
    img.save("/tmp/test_rounded_rect.png"_L1);
    QVERIFY2(hasNonWhitePixels(img), "RoundedRectangle should render non-white pixels");
}

void tst_QPsdShapeInject::testEllipse()
{
    QPsdAbstractLayerItem::PathInfo pathInfo;
    pathInfo.type = QPsdAbstractLayerItem::PathInfo::Path;
    QPainterPath pp;
    pp.addEllipse(QRectF(0, 0, 100, 100));
    pathInfo.path = pp;
    pathInfo.rect = QRectF(0, 0, 100, 100);

    ShapeSpec spec;
    spec.layerId = 3001;
    spec.name = "TestEllipse"_L1;
    spec.rect = QRect(50, 50, 100, 100);
    spec.pathInfo = pathInfo;
    spec.brush = QBrush(Qt::blue);
    spec.pen = Qt::NoPen;

    QImage img = buildAndRender(QSize(200, 200), {spec});
    img.save("/tmp/test_ellipse.png"_L1);
    QVERIFY2(hasNonWhitePixels(img), "Ellipse (Path) should render non-white pixels");
}

void tst_QPsdShapeInject::testAllShapes()
{
    QList<ShapeSpec> specs;

    // Rectangle
    {
        QPsdAbstractLayerItem::PathInfo pi;
        pi.type = QPsdAbstractLayerItem::PathInfo::Rectangle;
        pi.rect = QRectF(0, 0, 80, 80);
        specs.append({4001, "Rect"_L1, QRect(10, 10, 80, 80), pi, QBrush(Qt::red), Qt::NoPen});
    }
    // Rounded rectangle
    {
        QPsdAbstractLayerItem::PathInfo pi;
        pi.type = QPsdAbstractLayerItem::PathInfo::RoundedRectangle;
        pi.rect = QRectF(0, 0, 80, 80);
        pi.radius = 15;
        specs.append({4002, "RoundRect"_L1, QRect(110, 10, 80, 80), pi, QBrush(Qt::green), Qt::NoPen});
    }
    // Ellipse
    {
        QPsdAbstractLayerItem::PathInfo pi;
        pi.type = QPsdAbstractLayerItem::PathInfo::Path;
        QPainterPath pp;
        pp.addEllipse(QRectF(0, 0, 80, 80));
        pi.path = pp;
        pi.rect = QRectF(0, 0, 80, 80);
        specs.append({4003, "Ellipse"_L1, QRect(210, 10, 80, 80), pi, QBrush(Qt::blue), Qt::NoPen});
    }

    QImage img = buildAndRender(QSize(300, 100), specs);
    img.save("/tmp/test_all_shapes.png"_L1);

    // Check that each shape region has non-white pixels
    QImage rectRegion = img.copy(10, 10, 80, 80);
    QImage roundRegion = img.copy(110, 10, 80, 80);
    QImage ellipseRegion = img.copy(210, 10, 80, 80);

    QVERIFY2(hasNonWhitePixels(rectRegion), "Rectangle region should have non-white pixels");
    QVERIFY2(hasNonWhitePixels(roundRegion), "RoundedRect region should have non-white pixels");
    QVERIFY2(hasNonWhitePixels(ellipseRegion), "Ellipse region should have non-white pixels");
}

void tst_QPsdShapeInject::testFolderWrappedShapes()
{
    // Simulate Figma artboard: shapes wrapped in a folder (like flattenFigmaTree)
    FolderSpec artboard;
    artboard.layerId = 5000;
    artboard.name = "TestArtboard"_L1;
    artboard.rect = QRect(0, 0, 300, 300);

    // Rectangle inside artboard at position (50,50)
    {
        QPsdAbstractLayerItem::PathInfo pi;
        pi.type = QPsdAbstractLayerItem::PathInfo::Rectangle;
        pi.rect = QRectF(0, 0, 80, 80);
        artboard.children.append({5001, "FolderRect"_L1, QRect(50, 50, 80, 80), pi, QBrush(Qt::red), Qt::NoPen});
    }
    // Rounded rectangle inside artboard at position (150,50)
    {
        QPsdAbstractLayerItem::PathInfo pi;
        pi.type = QPsdAbstractLayerItem::PathInfo::RoundedRectangle;
        pi.rect = QRectF(0, 0, 80, 80);
        pi.radius = 15;
        artboard.children.append({5002, "FolderRoundRect"_L1, QRect(150, 50, 80, 80), pi, QBrush(Qt::green), Qt::NoPen});
    }
    // Ellipse inside artboard at position (50,150)
    {
        QPsdAbstractLayerItem::PathInfo pi;
        pi.type = QPsdAbstractLayerItem::PathInfo::Path;
        QPainterPath pp;
        pp.addEllipse(QRectF(0, 0, 80, 80));
        pi.path = pp;
        pi.rect = QRectF(0, 0, 80, 80);
        artboard.children.append({5003, "FolderEllipse"_L1, QRect(50, 150, 80, 80), pi, QBrush(Qt::blue), Qt::NoPen});
    }

    QImage img = buildAndRenderWithFolders(QSize(300, 300), {artboard});
    img.save("/tmp/test_folder_wrapped.png"_L1);

    // Check regions
    QImage rectRegion = img.copy(50, 50, 80, 80);
    QImage roundRegion = img.copy(150, 50, 80, 80);
    QImage ellipseRegion = img.copy(50, 150, 80, 80);

    QVERIFY2(hasNonWhitePixels(rectRegion), "Folder-wrapped rectangle should render");
    QVERIFY2(hasNonWhitePixels(roundRegion), "Folder-wrapped rounded rect should render");
    QVERIFY2(hasNonWhitePixels(ellipseRegion), "Folder-wrapped ellipse should render");
}

void tst_QPsdShapeInject::testMultipleArtboards()
{
    // Simulate multiple Figma artboards on one canvas (like page import)
    QList<FolderSpec> artboards;

    // Artboard 1: Rectangle at canvas position (0,0)
    {
        FolderSpec ab;
        ab.layerId = 6000;
        ab.name = "Artboard1"_L1;
        ab.rect = QRect(0, 0, 150, 150);
        QPsdAbstractLayerItem::PathInfo pi;
        pi.type = QPsdAbstractLayerItem::PathInfo::Rectangle;
        pi.rect = QRectF(0, 0, 80, 80);
        ab.children.append({6001, "AB1_Rect"_L1, QRect(35, 35, 80, 80), pi, QBrush(Qt::red), Qt::NoPen});
        artboards.append(ab);
    }
    // Artboard 2: RoundedRect at canvas position (170,0)
    {
        FolderSpec ab;
        ab.layerId = 7000;
        ab.name = "Artboard2"_L1;
        ab.rect = QRect(170, 0, 150, 150);
        QPsdAbstractLayerItem::PathInfo pi;
        pi.type = QPsdAbstractLayerItem::PathInfo::RoundedRectangle;
        pi.rect = QRectF(0, 0, 80, 80);
        pi.radius = 15;
        ab.children.append({7001, "AB2_Round"_L1, QRect(205, 35, 80, 80), pi, QBrush(Qt::green), Qt::NoPen});
        artboards.append(ab);
    }
    // Artboard 3: Ellipse at canvas position (340,0)
    {
        FolderSpec ab;
        ab.layerId = 8000;
        ab.name = "Artboard3"_L1;
        ab.rect = QRect(340, 0, 150, 150);
        QPsdAbstractLayerItem::PathInfo pi;
        pi.type = QPsdAbstractLayerItem::PathInfo::Path;
        QPainterPath pp;
        pp.addEllipse(QRectF(0, 0, 80, 80));
        pi.path = pp;
        pi.rect = QRectF(0, 0, 80, 80);
        ab.children.append({8001, "AB3_Ellipse"_L1, QRect(375, 35, 80, 80), pi, QBrush(Qt::blue), Qt::NoPen});
        artboards.append(ab);
    }

    QImage img = buildAndRenderWithFolders(QSize(500, 150), artboards);
    img.save("/tmp/test_multi_artboard.png"_L1);

    // Crop each artboard region (like the Figma test does)
    QImage ab1 = img.copy(0, 0, 150, 150);
    QImage ab2 = img.copy(170, 0, 150, 150);
    QImage ab3 = img.copy(340, 0, 150, 150);

    ab1.save("/tmp/test_multi_ab1.png"_L1);
    ab2.save("/tmp/test_multi_ab2.png"_L1);
    ab3.save("/tmp/test_multi_ab3.png"_L1);

    QVERIFY2(hasNonWhitePixels(ab1), "Artboard 1 (rectangle) should render");
    QVERIFY2(hasNonWhitePixels(ab2), "Artboard 2 (rounded rect) should render");
    QVERIFY2(hasNonWhitePixels(ab3), "Artboard 3 (ellipse) should render");
}

QTEST_MAIN(tst_QPsdShapeInject)
#include "tst_qpsdshapeinject.moc"

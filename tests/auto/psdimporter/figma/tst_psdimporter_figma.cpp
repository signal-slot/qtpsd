// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtTest/QtTest>
#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtWidgets/QApplication>
#include <QtPsdGui/QPsdAbstractLayerItem>
#include <QtPsdGui/QPsdFolderLayerItem>
#include <QtPsdWidget/QPsdView>
#include <QtPsdWidget/QPsdWidgetTreeItemModel>
#include <QtPsdExporter/QPsdExporterTreeItemModel>
#include <QtPsdImporter/QPsdImporterPlugin>

using namespace Qt::StringLiterals;

static const auto defaultFigmaUrl = u"https://www.figma.com/design/a2Q6nPHU1XaIfiI4eJ259D/Figma-Primitives"_s;

class tst_PsdImporterFigma : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void generateReport_data();
    void generateReport();
    void cleanupTestCase();

private:
    QJsonObject fetchJson(const QUrl &url);
    QImage downloadImage(const QUrl &url);
    QImage renderModel(QPsdWidgetTreeItemModel *model);
    double compareImages(const QImage &img1, const QImage &img2) const;
    QImage createDiffImage(const QImage &img1, const QImage &img2) const;

    static QString sanitize(const QString &name) {
        QString s = name;
        s.replace('/'_L1, '_'_L1);
        s.replace('\\'_L1, '_'_L1);
        s.replace(':'_L1, '_'_L1);
        return s;
    }

    QNetworkAccessManager m_nam;
    QString m_apiKey;
    QString m_figmaUrl;
    QString m_fileKey;
    QString m_outputPath;
    QString m_designName;

    struct ArtboardEntry {
        QString pageName;
        int pageIndex;
        QString artboardId;
        QString artboardName;
    };
    QList<ArtboardEntry> m_artboards;

    // Per-page cache: import once, render once, reuse for all artboards
    struct PageCache {
        int pageIndex = -1;
        QImage fullRender;
        QPsdExporterTreeItemModel *exporterModel = nullptr;
        QPsdWidgetTreeItemModel *widgetModel = nullptr;
        QHash<QString, QRect> artboardRects; // artboardName -> rect
    };
    PageCache m_pageCache;

    // Per-page batch of reference images: artboardId -> QImage
    int m_refBatchPageIndex = -1;
    QHash<QString, QImage> m_refImages;

    void ensurePageImported(int pageIndex);
    void ensureRefImagesBatched(int pageIndex);

    struct ReportRow {
        QString pageName;
        QString artboardName;
        double similarity;
        QString refPath;
        QString renderPath;
        QString diffPath;
    };
    QList<ReportRow> m_reportRows;
};

void tst_PsdImporterFigma::initTestCase()
{
    m_apiKey = qEnvironmentVariable("FIGMA_API_KEY");
    if (m_apiKey.isEmpty())
        QSKIP("FIGMA_API_KEY not set — skipping Figma import similarity test");

    m_figmaUrl = qEnvironmentVariable("FIGMA_TEST_FILE_URL", defaultFigmaUrl);
    m_outputPath = qEnvironmentVariable("FIGMA_TEST_OUTPUT_PATH");

    // Extract file key
    static const QRegularExpression reKey(u"figma\\.com/(?:file|design)/([a-zA-Z0-9]+)"_s);
    auto match = reKey.match(m_figmaUrl);
    QVERIFY2(match.hasMatch(), "Could not extract file key from FIGMA_TEST_FILE_URL");
    m_fileKey = match.captured(1);

    // Fetch Figma file JSON (needed to enumerate pages/artboards)
    QUrl apiUrl(u"https://api.figma.com/v1/files/%1?depth=2"_s.arg(m_fileKey));
    const auto fileJson = fetchJson(apiUrl);
    QVERIFY2(!fileJson.contains("error"_L1),
             qPrintable(u"Figma API error: "_s + fileJson["error"_L1].toString()));

    m_designName = fileJson["name"_L1].toString();
    if (m_outputPath.isEmpty())
        m_outputPath = u"docs/images/%1"_s.arg(m_designName);

    // Enumerate pages and artboards
    const auto pages = fileJson["document"_L1].toObject()["children"_L1].toArray();
    for (int pi = 0; pi < pages.size(); ++pi) {
        const auto page = pages[pi].toObject();
        const auto pageName = page["name"_L1].toString();
        const auto children = page["children"_L1].toArray();
        for (const auto &child : children) {
            const auto obj = child.toObject();
            const auto type = obj["type"_L1].toString();
            if (type == "FRAME"_L1 || type == "COMPONENT"_L1 || type == "COMPONENT_SET"_L1) {
                ArtboardEntry entry;
                entry.pageName = pageName;
                entry.pageIndex = pi;
                entry.artboardId = obj["id"_L1].toString();
                entry.artboardName = obj["name"_L1].toString();
                m_artboards.append(entry);
            }
        }
    }

    QVERIFY2(!m_artboards.isEmpty(), "No artboards found in the Figma file");
    qDebug() << "Found" << m_artboards.size() << "artboards in" << m_designName;

    // Create output directories
    QDir().mkpath(m_outputPath + "/reference"_L1);
    QDir().mkpath(m_outputPath + "/psdview"_L1);
    QDir().mkpath(m_outputPath + "/diff"_L1);
}

void tst_PsdImporterFigma::ensurePageImported(int pageIndex)
{
    if (m_pageCache.pageIndex == pageIndex)
        return;

    // Clean up previous cache
    delete m_pageCache.exporterModel;
    m_pageCache = PageCache();

    auto *importer = QPsdImporterPlugin::plugin("figma");
    Q_ASSERT(importer);

    auto *exporterModel = new QPsdExporterTreeItemModel(this);
    QVariantMap options;
    options["source"_L1] = m_figmaUrl;
    options["apiKey"_L1] = m_apiKey;
    options["pageIndex"_L1] = pageIndex;
    options["imageScale"_L1] = 1;

    if (!importer->importFrom(exporterModel, options)) {
        delete exporterModel;
        return;
    }

    auto *widgetModel = qobject_cast<QPsdWidgetTreeItemModel *>(exporterModel->sourceModel());
    if (!widgetModel) {
        delete exporterModel;
        return;
    }

    m_pageCache.pageIndex = pageIndex;
    m_pageCache.exporterModel = exporterModel;
    m_pageCache.widgetModel = widgetModel;
    m_pageCache.fullRender = renderModel(widgetModel);

    // Build artboard rect map
    for (int row = 0; row < widgetModel->rowCount(); ++row) {
        const auto index = widgetModel->index(row, 0);
        const auto *layerItem = widgetModel->layerItem(index);
        if (!layerItem || layerItem->type() != QPsdAbstractLayerItem::Folder)
            continue;
        const auto *folder = static_cast<const QPsdFolderLayerItem *>(layerItem);
        const auto abRect = folder->artboardRect();
        if (!abRect.isEmpty())
            m_pageCache.artboardRects.insert(layerItem->name(), abRect);
    }

    qDebug() << "Imported page" << pageIndex
             << "- render:" << m_pageCache.fullRender.size()
             << "- artboards:" << m_pageCache.artboardRects.size();
}

void tst_PsdImporterFigma::ensureRefImagesBatched(int pageIndex)
{
    if (m_refBatchPageIndex == pageIndex)
        return;

    m_refImages.clear();
    m_refBatchPageIndex = pageIndex;

    // Collect all artboard IDs for this page
    QStringList ids;
    for (const auto &entry : m_artboards) {
        if (entry.pageIndex == pageIndex)
            ids.append(entry.artboardId);
    }
    if (ids.isEmpty())
        return;

    // Figma images API supports batch requests (comma-separated IDs)
    const int batchSize = 50;
    for (int i = 0; i < ids.size(); i += batchSize) {
        const auto batch = ids.mid(i, batchSize);
        const auto idsParam = batch.join(","_L1);
        QUrl imgApiUrl(u"https://api.figma.com/v1/images/%1?ids=%2&format=png&scale=1"_s
                           .arg(m_fileKey, idsParam));
        const auto imgJson = fetchJson(imgApiUrl);
        const auto images = imgJson["images"_L1].toObject();

        for (const auto &id : batch) {
            const auto imageUrl = images[id].toString();
            if (!imageUrl.isEmpty()) {
                QImage img = downloadImage(QUrl(imageUrl));
                if (!img.isNull())
                    m_refImages.insert(id, img);
            }
        }
    }

    qDebug() << "Fetched" << m_refImages.size() << "reference images for page" << pageIndex;
}

void tst_PsdImporterFigma::generateReport_data()
{
    QTest::addColumn<int>("artboardIndex");

    for (int i = 0; i < m_artboards.size(); ++i) {
        const auto &entry = m_artboards[i];
        const auto label = u"%1/%2"_s.arg(entry.pageName, entry.artboardName);
        QTest::newRow(label.toUtf8().constData()) << i;
    }
}

void tst_PsdImporterFigma::generateReport()
{
    QFETCH(int, artboardIndex);
    const auto &entry = m_artboards[artboardIndex];

    const auto subDir = u"%1_%2"_s.arg(entry.pageIndex + 1).arg(sanitize(entry.pageName));
    const auto artboardFile = sanitize(entry.artboardName) + ".png"_L1;

    QDir().mkpath(m_outputPath + "/reference/"_L1 + subDir);
    QDir().mkpath(m_outputPath + "/psdview/"_L1 + subDir);
    QDir().mkpath(m_outputPath + "/diff/"_L1 + subDir);

    // --- Step 1: Ensure page is imported (cached per page) ---
    ensurePageImported(entry.pageIndex);
    QVERIFY2(m_pageCache.pageIndex == entry.pageIndex, "Failed to import page");
    QVERIFY2(!m_pageCache.fullRender.isNull(), "Page render is null");

    // --- Step 2: Crop artboard from full render ---
    QImage artboardRender;
    if (m_pageCache.artboardRects.contains(entry.artboardName)) {
        const auto rect = m_pageCache.artboardRects.value(entry.artboardName);
        artboardRender = m_pageCache.fullRender.copy(rect);
    }

    if (artboardRender.isNull()) {
        qWarning() << "Could not find artboard rect for" << entry.artboardName;
        artboardRender = m_pageCache.fullRender;
    }

    // Save rendered image
    const auto renderPath = u"%1/psdview/%2/%3"_s.arg(m_outputPath, subDir, artboardFile);
    artboardRender.save(renderPath);

    // --- Step 3: Get reference image (batch-fetched per page) ---
    ensureRefImagesBatched(entry.pageIndex);

    QImage refImage = m_refImages.value(entry.artboardId);
    if (refImage.isNull()) {
        qWarning() << "No reference image for" << entry.artboardName;
        ReportRow row;
        row.pageName = entry.pageName;
        row.artboardName = entry.artboardName;
        row.similarity = -1.0;
        row.renderPath = u"psdview/%1/%2"_s.arg(subDir, artboardFile);
        m_reportRows.append(row);
        return;
    }

    // Save reference image
    const auto refPath = u"%1/reference/%2/%3"_s.arg(m_outputPath, subDir, artboardFile);
    refImage.save(refPath);

    // --- Step 4: Scale to match if needed ---
    if (artboardRender.size() != refImage.size()) {
        qDebug() << "Size mismatch: render=" << artboardRender.size()
                 << "ref=" << refImage.size() << "- scaling reference to match";
        refImage = refImage.scaled(artboardRender.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    // --- Step 5: Compare and generate diff ---
    const double similarity = compareImages(artboardRender, refImage);
    const QImage diffImg = createDiffImage(artboardRender, refImage);

    const auto diffPath = u"%1/diff/%2/%3"_s.arg(m_outputPath, subDir, artboardFile);
    if (!diffImg.isNull())
        diffImg.save(diffPath);

    qDebug().noquote() << u"[%1%%] %2/%3"_s
                              .arg(similarity, 0, 'f', 1)
                              .arg(entry.pageName, entry.artboardName);

    ReportRow row;
    row.pageName = entry.pageName;
    row.artboardName = entry.artboardName;
    row.similarity = similarity;
    row.refPath = u"reference/%1/%2"_s.arg(subDir, artboardFile);
    row.renderPath = u"psdview/%1/%2"_s.arg(subDir, artboardFile);
    row.diffPath = u"diff/%1/%2"_s.arg(subDir, artboardFile);
    m_reportRows.append(row);
}

void tst_PsdImporterFigma::cleanupTestCase()
{
    // Clean up page cache
    delete m_pageCache.exporterModel;
    m_pageCache = PageCache();

    if (m_reportRows.isEmpty())
        return;

    auto urlEncode = [](const QString &path) -> QString {
        QString encoded = path;
        encoded.replace(' '_L1, "%20"_L1);
        encoded.replace('&'_L1, "%26"_L1);
        return encoded;
    };

    const auto safeDesignName = sanitize(m_designName);
    const auto reportPath = u"%1/similarity_report.md"_s.arg(m_outputPath);

    QFile file(reportPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << u"# Figma Import Similarity Report: %1\n\n"_s.arg(m_designName);
    out << u"Generated: %1\n\n"_s.arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    out << u"Source: [`%1`](%1)\n\n"_s.arg(m_figmaUrl);

    // Overall summary
    double total = 0.0;
    int count = 0;
    for (const auto &row : m_reportRows) {
        if (row.similarity >= 0.0) {
            total += row.similarity;
            ++count;
        }
    }
    if (count > 0)
        out << u"**Overall average: %1%** (%2 artboards)\n\n"_s.arg(total / count, 0, 'f', 1).arg(count);

    // Per-page summary table
    out << u"## Summary by Page\n\n"_s;
    out << u"| Page | Avg Similarity | Artboards |\n"_s;
    out << u"|------|---------------|----------|\n"_s;

    QString currentPage;
    double pageTotal = 0.0;
    int pageCount = 0;
    auto flushPage = [&]() {
        if (!currentPage.isEmpty() && pageCount > 0)
            out << u"| %1 | %2% | %3 |\n"_s
                       .arg(currentPage)
                       .arg(pageTotal / pageCount, 0, 'f', 1)
                       .arg(pageCount);
    };
    for (const auto &row : m_reportRows) {
        if (row.pageName != currentPage) {
            flushPage();
            currentPage = row.pageName;
            pageTotal = 0.0;
            pageCount = 0;
        }
        if (row.similarity >= 0.0) {
            pageTotal += row.similarity;
            ++pageCount;
        }
    }
    flushPage();

    // Detailed results per page
    out << u"\n## Detailed Results\n"_s;

    currentPage.clear();
    for (const auto &row : m_reportRows) {
        if (row.pageName != currentPage) {
            currentPage = row.pageName;
            out << u"\n### %1\n\n"_s.arg(currentPage);
            out << u"| Artboard | Similarity | Reference | QPsdView | Diff |\n"_s;
            out << u"|----------|------------|-----------|----------|------|\n"_s;
        }

        const auto sim = row.similarity >= 0.0
                             ? u"%1%"_s.arg(row.similarity, 0, 'f', 1)
                             : u"N/A"_s;
        const auto ref = row.refPath.isEmpty()
                             ? u"-"_s
                             : u"![ref](%1)"_s.arg(urlEncode(row.refPath));
        const auto render = u"![render](%1)"_s.arg(urlEncode(row.renderPath));
        const auto diff = row.diffPath.isEmpty()
                              ? u"-"_s
                              : u"![diff](%1)"_s.arg(urlEncode(row.diffPath));
        out << u"| %1 | %2 | %3 | %4 | %5 |\n"_s
                   .arg(row.artboardName, sim, ref, render, diff);
    }

    out.flush();
    file.close();
    qDebug().noquote() << "Report written to:" << reportPath;
}

// ─── Helpers ────────────────────────────────────────────────────────────────

QJsonObject tst_PsdImporterFigma::fetchJson(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setRawHeader("X-Figma-Token", m_apiKey.toUtf8());
    auto *reply = m_nam.get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QJsonObject result;
    if (reply->error() == QNetworkReply::NoError) {
        result = QJsonDocument::fromJson(reply->readAll()).object();
    } else {
        result["error"_L1] = reply->errorString();
    }
    reply->deleteLater();
    return result;
}

QImage tst_PsdImporterFigma::downloadImage(const QUrl &url)
{
    QNetworkRequest request(url);
    auto *reply = m_nam.get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QImage image;
    if (reply->error() == QNetworkReply::NoError)
        image.loadFromData(reply->readAll());
    else
        qWarning() << "Image download failed:" << reply->errorString();
    reply->deleteLater();
    return image;
}

QImage tst_PsdImporterFigma::renderModel(QPsdWidgetTreeItemModel *model)
{
    QPsdView view;
    const QSize canvasSize = model->size();
    if (canvasSize.isEmpty())
        return QImage();

    view.setAttribute(Qt::WA_TranslucentBackground);
    view.resize(canvasSize.grownBy({1, 1, 1, 1}));
    view.viewport()->resize(canvasSize);
    view.setModel(model);

    QImage rendered(canvasSize, QImage::Format_ARGB32);
    rendered.fill(Qt::white);

    QPainter painter(&rendered);
    view.render(&painter);

    return rendered;
}

double tst_PsdImporterFigma::compareImages(const QImage &img1, const QImage &img2) const
{
    if (img1.size() != img2.size() || img1.isNull() || img2.isNull())
        return 0.0;

    QImage a = img1.convertToFormat(QImage::Format_ARGB32);
    QImage b = img2.convertToFormat(QImage::Format_ARGB32);
    if (a.isNull() || b.isNull())
        return 0.0;

    auto compositeOverWhite = [](int r, int g, int b, int alpha) -> std::tuple<int, int, int> {
        const int rr = (r * alpha + 255 * (255 - alpha)) / 255;
        const int gg = (g * alpha + 255 * (255 - alpha)) / 255;
        const int bb = (b * alpha + 255 * (255 - alpha)) / 255;
        return {rr, gg, bb};
    };

    auto rgbToHsv = [](int r, int g, int b) -> std::tuple<double, double, double> {
        const double rf = r / 255.0;
        const double gf = g / 255.0;
        const double bf = b / 255.0;

        const double cmax = qMax(qMax(rf, gf), bf);
        const double cmin = qMin(qMin(rf, gf), bf);
        const double diff = cmax - cmin;

        double h = 0.0;
        if (diff > 0.0) {
            if (cmax == rf)
                h = 60.0 * fmod((gf - bf) / diff, 6.0);
            else if (cmax == gf)
                h = 60.0 * ((bf - rf) / diff + 2.0);
            else
                h = 60.0 * ((rf - gf) / diff + 4.0);
        }
        if (h < 0.0)
            h += 360.0;

        const double s = cmax > 0.0 ? (diff / cmax) : 0.0;
        const double v = cmax;
        return {h, s, v};
    };

    double totalDiff = 0.0;
    qint64 pixelsWithContent = 0;
    qint64 pixelsDifferent = 0;

    for (int y = 0; y < a.height(); ++y) {
        const QRgb *lineA = reinterpret_cast<const QRgb *>(a.scanLine(y));
        const QRgb *lineB = reinterpret_cast<const QRgb *>(b.scanLine(y));

        for (int x = 0; x < a.width(); ++x) {
            const QRgb p1 = lineA[x];
            const QRgb p2 = lineB[x];

            const auto [cr1, cg1, cb1] = compositeOverWhite(qRed(p1), qGreen(p1), qBlue(p1), qAlpha(p1));
            const auto [cr2, cg2, cb2] = compositeOverWhite(qRed(p2), qGreen(p2), qBlue(p2), qAlpha(p2));

            if (cr1 >= 250 && cg1 >= 250 && cb1 >= 250 &&
                cr2 >= 250 && cg2 >= 250 && cb2 >= 250)
                continue;

            ++pixelsWithContent;

            const auto [h1, s1, v1] = rgbToHsv(cr1, cg1, cb1);
            const auto [h2, s2, v2] = rgbToHsv(cr2, cg2, cb2);

            double hDiff = qAbs(h1 - h2);
            if (hDiff > 180.0)
                hDiff = 360.0 - hDiff;
            hDiff /= 180.0;

            const double sDiff = qAbs(s1 - s2);
            const double vDiff = qAbs(v1 - v2);
            const double pixelDiff = hDiff * 0.3 + sDiff * 0.3 + vDiff * 0.4;

            totalDiff += pixelDiff;
            if (pixelDiff > 0.1)
                ++pixelsDifferent;
        }
    }

    if (pixelsWithContent == 0)
        return 100.0;

    const double contentBasedSimilarity = 100.0 * (1.0 - static_cast<double>(pixelsDifferent) / static_cast<double>(pixelsWithContent));
    const double hsvBasedSimilarity = 100.0 * (1.0 - totalDiff / static_cast<double>(pixelsWithContent));
    return qMin(contentBasedSimilarity, hsvBasedSimilarity);
}

QImage tst_PsdImporterFigma::createDiffImage(const QImage &img1, const QImage &img2) const
{
    if (img1.size() != img2.size() || img1.isNull() || img2.isNull())
        return QImage();

    QImage diff(img1.size(), QImage::Format_ARGB32);
    diff.fill(Qt::transparent);

    auto compositeOverWhite = [](int r, int g, int b, int alpha) -> std::tuple<int, int, int> {
        const int rr = (r * alpha + 255 * (255 - alpha)) / 255;
        const int gg = (g * alpha + 255 * (255 - alpha)) / 255;
        const int bb = (b * alpha + 255 * (255 - alpha)) / 255;
        return {rr, gg, bb};
    };

    for (int y = 0; y < diff.height(); ++y) {
        for (int x = 0; x < diff.width(); ++x) {
            const QRgb p1 = img1.pixel(x, y);
            const QRgb p2 = img2.pixel(x, y);

            const auto [cr1, cg1, cb1] = compositeOverWhite(qRed(p1), qGreen(p1), qBlue(p1), qAlpha(p1));
            const auto [cr2, cg2, cb2] = compositeOverWhite(qRed(p2), qGreen(p2), qBlue(p2), qAlpha(p2));

            const bool p1HasContent = (cr1 < 240 || cg1 < 240 || cb1 < 240);
            const bool p2HasContent = (cr2 < 240 || cg2 < 240 || cb2 < 240);

            if (p1HasContent && !p2HasContent) {
                diff.setPixel(x, y, qRgba(255, 0, 0, 255));
            } else if (!p1HasContent && p2HasContent) {
                diff.setPixel(x, y, qRgba(0, 0, 255, 255));
            } else if (p1HasContent && p2HasContent) {
                const int colorDiff = qAbs(cr1 - cr2) + qAbs(cg1 - cg2) + qAbs(cb1 - cb2);
                if (colorDiff > 30)
                    diff.setPixel(x, y, qRgba(255, 255, 0, 255));
                else
                    diff.setPixel(x, y, qRgba(128, 128, 128, 255));
            }
        }
    }

    return diff;
}

QTEST_MAIN(tst_PsdImporterFigma)
#include "tst_psdimporter_figma.moc"

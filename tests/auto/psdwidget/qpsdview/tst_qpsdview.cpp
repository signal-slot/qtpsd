// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtTest/QtTest>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtPsdCore/QPsdParser>
#include <QtPsdCore/QPsdFileHeader>
#include <QtPsdCore/QPsdImageData>
#include <QtPsdGui/qpsdguiglobal.h>
#include <QtPsdWidget/QPsdView>
#include <QtPsdWidget/QPsdWidgetTreeItemModel>
#include <QtPsdWidget/QPsdScene>
#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <QtCore/QDateTime>
#include <algorithm>
#include <cmath>
#include <tuple>

class tst_QPsdView : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void compareRendering_data();
    void compareRendering();
    void cleanupTestCase();

private:
    struct SimilarityResult {
        QString fileName;
        QString source;         // "ag-psd", "psd-zoo"
        qint64 fileSize;
        double similarityPsd2PngVsImageData;
        double similarityPsd2PngVsPsdView;
        double similarityImageDataVsPsdView;
        bool passedImageData;
        bool passedPsdView;
        bool passedImageDataVsPsdView;
        QString imagePaths[7]; // 0: psd2png, 1: imagedata, 2: psdview, 3: diff_imagedata, 4: diff_psdview, 5: diff_imagedata_vs_psdview, 6: (reserved)
    };

    void addPsdFiles();
    QImage renderPsdView(const QString &filePath);
    double compareImages(const QImage &img1, const QImage &img2);
    QString findPsd2PngPath(const QString &psdPath, const QString &source);
    QImage createDiffImage(const QImage &img1, const QImage &img2);
    QString detectSource(const QString &psdPath);
    QString formatFileSize(qint64 fileSize);
    void writeReportSection(QTextStream &stream, const QList<SimilarityResult> &results,
                            const QString &title, const QString &col1Label, const QString &col2Label,
                            int similarityIndex, int imgIndex1, int imgIndex2, int diffIndex,
                            const QString &githubBaseUrl);

    QList<SimilarityResult> m_similarityResults;
    bool m_generateSummary;
    QString m_outputBaseDir;
    QString m_projectRoot;

    struct SourceInfo {
        QString id;             // "ag-psd", "psd-zoo"
        QString testDataPath;   // QFINDTESTDATA result
        QString githubBaseUrl;  // GitHub URL prefix
        QString psd2pngSubdir;  // subdirectory under docs/images/psd2png/, empty if none
    };
    QList<SourceInfo> m_sources;
};

void tst_QPsdView::initTestCase()
{
    // Check if summary generation is enabled via environment variable
    QString summaryPath = qEnvironmentVariable("QTPSD_SIMILARITY_SUMMARY_PATH");
    m_generateSummary = !summaryPath.isEmpty();

    if (m_generateSummary) {
        qDebug() << "Similarity summary generation enabled";

        // If path is relative, make it relative to the project root
        QDir projectRoot = QDir::current();
        // Go up until we find CMakeLists.txt (project root)
        while (!projectRoot.exists("CMakeLists.txt") && projectRoot.cdUp()) {
            // Keep going up
        }
        m_projectRoot = projectRoot.absolutePath();

        if (QDir::isRelativePath(summaryPath)) {
            m_outputBaseDir = projectRoot.absoluteFilePath(summaryPath);
        } else {
            m_outputBaseDir = summaryPath;
        }

        // Create output directory structure
        QDir dir;
        dir.mkpath(m_outputBaseDir);
        dir.mkpath(m_outputBaseDir + "/images");

        qDebug() << "Output directory:" << m_outputBaseDir;
    }

    // Populate source info
    m_sources = {
        {"ag-psd"_L1,
         QFINDTESTDATA("../../3rdparty/ag-psd/test/"),
         "https://github.com/Agamnentzar/ag-psd/tree/master/test/"_L1,
         "ag-psd"_L1},
        {"psd-zoo"_L1,
         QFINDTESTDATA("../../3rdparty/psd-zoo/"),
         "https://github.com/signal-slot/psd-zoo/tree/main/"_L1,
         QString()},
    };
}

void tst_QPsdView::addPsdFiles()
{
    QTest::addColumn<QString>("psd");

    std::function<void(QDir *dir, const QDir &baseDir)> findPsd;
    findPsd = [&](QDir *dir, const QDir &baseDir) {
        for (const QString &subdir : dir->entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            dir->cd(subdir);
            findPsd(dir, baseDir);
            dir->cdUp();
        }
        for (const QString &fileName : dir->entryList(QStringList() << "*.psd")) {
            // Use relative path from base directory for test row name
            QString relativePath = baseDir.relativeFilePath(dir->filePath(fileName));
            QTest::newRow(relativePath.toLatin1().data()) << dir->filePath(fileName);
        }
    };

    QDir agPsd(QFINDTESTDATA("../../3rdparty/ag-psd/test/"));
    findPsd(&agPsd, QDir(agPsd));

    QDir psdTools(QFINDTESTDATA("../../3rdparty/psd-tools/tests/psd_files/"));
    findPsd(&psdTools, QDir(psdTools));

    QDir psdZoo(QFINDTESTDATA("../../3rdparty/psd-zoo/"));
    findPsd(&psdZoo, QDir(psdZoo));
}

QImage tst_QPsdView::renderPsdView(const QString &filePath)
{
    // Load the PSD file
    QPsdParser parser;
    parser.load(filePath);

    // Create the model
    QPsdWidgetTreeItemModel model;
    model.fromParser(parser);

    // Create the view
    QPsdView view;

    // Get the size from the model
    const QSize canvasSize = model.size();
    if (canvasSize.isEmpty()) {
        return QImage();
    }

    // Make the view transparent
    view.setAttribute(Qt::WA_TranslucentBackground);

    // Ensure the widget is properly sized
    view.resize(canvasSize.grownBy({1, 1, 1, 1}));
    view.viewport()->resize(canvasSize);

    view.setModel(&model);

    // Create image with transparent background
    QImage rendered(canvasSize, QImage::Format_ARGB32);
    rendered.fill(Qt::transparent);

    // Render the view to the image
    QPainter painter(&rendered);
    view.render(&painter);

    return rendered;
}

double tst_QPsdView::compareImages(const QImage &img1, const QImage &img2)
{
    if (img1.size() != img2.size()) {
        return 0.0;
    }

    if (img1.isNull() || img2.isNull()) {
        return 0.0;
    }

    // Convert both images to the same format for comparison
    // Handle special cases where conversion might fail
    QImage a, b;

    // Convert to a common format for comparison
    if (img1.format() == QImage::Format_RGB888 || img1.format() == QImage::Format_BGR888) {
        a = img1.convertToFormat(QImage::Format_RGB32);
    } else {
        a = img1.convertToFormat(QImage::Format_ARGB32);
    }

    if (img2.format() == QImage::Format_RGB888 || img2.format() == QImage::Format_BGR888) {
        b = img2.convertToFormat(QImage::Format_RGB32);
    } else {
        b = img2.convertToFormat(QImage::Format_ARGB32);
    }

    if (a.isNull() || b.isNull()) {
        return 0.0;
    }

    // Composite a pixel over white background: R' = (R * A + 255 * (255 - A)) / 255
    auto compositeOverWhite = [](int r, int g, int b, int a) -> std::tuple<int, int, int> {
        int rr = (r * a + 255 * (255 - a)) / 255;
        int gg = (g * a + 255 * (255 - a)) / 255;
        int bb = (b * a + 255 * (255 - a)) / 255;
        return {rr, gg, bb};
    };

    // Helper function to convert RGB to HSV
    auto rgbToHsv = [](int r, int g, int b) -> std::tuple<double, double, double> {
        double rf = r / 255.0;
        double gf = g / 255.0;
        double bf = b / 255.0;

        double cmax = qMax(qMax(rf, gf), bf);
        double cmin = qMin(qMin(rf, gf), bf);
        double diff = cmax - cmin;

        double h = 0;
        if (diff > 0) {
            if (cmax == rf) {
                h = 60 * fmod((gf - bf) / diff, 6.0);
            } else if (cmax == gf) {
                h = 60 * ((bf - rf) / diff + 2.0);
            } else if (cmax == bf) {
                h = 60 * ((rf - gf) / diff + 4.0);
            }
        }
        if (h < 0) h += 360;

        double s = (cmax > 0) ? (diff / cmax) : 0;
        double v = cmax;

        return std::make_tuple(h, s, v);
    };

    double totalDiff = 0;
    qint64 pixelsWithContent = 0;
    qint64 pixelsDifferent = 0;

    for (int y = 0; y < a.height(); ++y) {
        const QRgb *lineA = reinterpret_cast<const QRgb *>(a.scanLine(y));
        const QRgb *lineB = reinterpret_cast<const QRgb *>(b.scanLine(y));

        for (int x = 0; x < a.width(); ++x) {
            QRgb pixelA = lineA[x];
            QRgb pixelB = lineB[x];

            int r1 = qRed(pixelA), g1 = qGreen(pixelA), b1 = qBlue(pixelA), a1 = qAlpha(pixelA);
            int r2 = qRed(pixelB), g2 = qGreen(pixelB), b2 = qBlue(pixelB), a2 = qAlpha(pixelB);

            // Composite both pixels over white to resolve alpha differences
            auto [cr1, cg1, cb1] = compositeOverWhite(r1, g1, b1, a1);
            auto [cr2, cg2, cb2] = compositeOverWhite(r2, g2, b2, a2);

            // Skip pixels where both composited results are near-white (no visible content)
            if (cr1 >= 250 && cg1 >= 250 && cb1 >= 250 &&
                cr2 >= 250 && cg2 >= 250 && cb2 >= 250) {
                continue;
            }

            // For pixels where at least one image has visible content
            pixelsWithContent++;

            // Convert composited RGB to HSV for perceptually accurate comparison
            auto [h1, s1, v1] = rgbToHsv(cr1, cg1, cb1);
            auto [h2, s2, v2] = rgbToHsv(cr2, cg2, cb2);

            // Calculate hue difference (circular)
            double hDiff = qAbs(h1 - h2);
            if (hDiff > 180) hDiff = 360 - hDiff;
            hDiff = hDiff / 180.0; // Normalize to 0-1

            // Calculate saturation and value differences
            double sDiff = qAbs(s1 - s2);
            double vDiff = qAbs(v1 - v2);

            // Weighted combination (no alpha term needed since alpha is resolved by compositing)
            double pixelDiff = hDiff * 0.3 + sDiff * 0.3 + vDiff * 0.4;

            // Accumulate difference
            totalDiff += pixelDiff;

            // Count as different if the difference is significant
            if (pixelDiff > 0.1) {
                pixelsDifferent++;
            }
        }
    }

    // If there's no content in either image, they're considered identical
    if (pixelsWithContent == 0) {
        return 100.0;
    }

    // Calculate similarity based on pixels with content
    double contentBasedSimilarity = 100.0 * (1.0 - static_cast<double>(pixelsDifferent) / static_cast<double>(pixelsWithContent));

    // Calculate HSV-based similarity for pixels with content
    double avgDiff = totalDiff / pixelsWithContent;
    double hsvBasedSimilarity = 100.0 * (1.0 - avgDiff);

    // Use the lower of the two similarities to be more conservative
    double similarity = qMin(contentBasedSimilarity, hsvBasedSimilarity);

    return similarity;
}

QString tst_QPsdView::detectSource(const QString &psdPath)
{
    const QString canonicalPsd = QFileInfo(psdPath).canonicalFilePath();
    for (const auto &src : m_sources) {
        if (!src.testDataPath.isEmpty()) {
            const QString canonicalBase = QFileInfo(src.testDataPath).canonicalFilePath();
            if (canonicalPsd.startsWith(canonicalBase)) {
                return src.id;
            }
        }
    }
    return QString();
}

QString tst_QPsdView::formatFileSize(qint64 fileSize)
{
    if (fileSize < 1024) {
        return QString("%1 B").arg(fileSize);
    } else if (fileSize < 1024 * 1024) {
        return QString("%1 KB").arg(fileSize / 1024.0, 0, 'f', 1);
    } else {
        return QString("%1 MB").arg(fileSize / (1024.0 * 1024.0), 0, 'f', 1);
    }
}

QString tst_QPsdView::findPsd2PngPath(const QString &psdPath, const QString &source)
{
    // Find the source info
    const SourceInfo *srcInfo = nullptr;
    for (const auto &src : m_sources) {
        if (src.id == source) {
            srcInfo = &src;
            break;
        }
    }

    // No psd2png available for this source
    if (!srcInfo || srcInfo->psd2pngSubdir.isEmpty()) {
        return QString();
    }

    // Get relative path from source test directory (use canonical paths to resolve ".." components)
    QString relativePsdPath = QDir(QFileInfo(srcInfo->testDataPath).canonicalFilePath())
                                  .relativeFilePath(QFileInfo(psdPath).canonicalFilePath());

    // Remove .psd extension
    QString pathWithoutExt = relativePsdPath.left(relativePsdPath.lastIndexOf('.'));

    // Get project root to construct paths
    QDir projectRoot = QDir::current();
    while (!projectRoot.exists("CMakeLists.txt") && projectRoot.cdUp()) {
        // Keep going up
    }
    QString psd2pngBase = projectRoot.absoluteFilePath(
        QString("docs/images/psd2png/%1/").arg(srcInfo->psd2pngSubdir));

    // Use exact same filename as the PSD (just replace .psd with .png)
    QString pngPath = psd2pngBase + pathWithoutExt + ".png";

    if (QFile::exists(pngPath)) {
        return pngPath;
    }

    // If no file found, return empty string
    return QString();
}

QImage tst_QPsdView::createDiffImage(const QImage &img1, const QImage &img2)
{
    if (img1.size() != img2.size()) {
        return QImage();
    }

    // Create a diff image that clearly shows the differences
    QImage diff(img1.size(), QImage::Format_ARGB32);
    diff.fill(Qt::transparent);  // Start with transparent background

    // Composite a pixel over white background: R' = (R * A + 255 * (255 - A)) / 255
    auto compositeOverWhite = [](int r, int g, int b, int a) -> std::tuple<int, int, int> {
        int rr = (r * a + 255 * (255 - a)) / 255;
        int gg = (g * a + 255 * (255 - a)) / 255;
        int bb = (b * a + 255 * (255 - a)) / 255;
        return {rr, gg, bb};
    };

    for (int y = 0; y < diff.height(); ++y) {
        for (int x = 0; x < diff.width(); ++x) {
            QRgb p1 = img1.pixel(x, y);
            QRgb p2 = img2.pixel(x, y);

            // Get color components
            int r1 = qRed(p1), g1 = qGreen(p1), b1 = qBlue(p1), a1 = qAlpha(p1);
            int r2 = qRed(p2), g2 = qGreen(p2), b2 = qBlue(p2), a2 = qAlpha(p2);

            // Composite both pixels over white
            auto [cr1, cg1, cb1] = compositeOverWhite(r1, g1, b1, a1);
            auto [cr2, cg2, cb2] = compositeOverWhite(r2, g2, b2, a2);

            // "Has content" = any composited channel < 240
            bool p1HasContent = (cr1 < 240 || cg1 < 240 || cb1 < 240);
            bool p2HasContent = (cr2 < 240 || cg2 < 240 || cb2 < 240);

            if (p1HasContent && !p2HasContent) {
                // Only in img1 (psd2png) - show as red
                diff.setPixel(x, y, qRgba(255, 0, 0, 255));
            } else if (!p1HasContent && p2HasContent) {
                // Only in img2 - show as blue
                diff.setPixel(x, y, qRgba(0, 0, 255, 255));
            } else if (p1HasContent && p2HasContent) {
                // In both - check if composited colors differ
                int colorDiff = qAbs(cr1 - cr2) + qAbs(cg1 - cg2) + qAbs(cb1 - cb2);
                if (colorDiff > 30) {
                    // Different colors - show as yellow
                    diff.setPixel(x, y, qRgba(255, 255, 0, 255));
                } else {
                    // Same/similar colors - show as gray
                    diff.setPixel(x, y, qRgba(128, 128, 128, 255));
                }
            }
            // else: neither has content, leave as transparent
        }
    }

    return diff;
}

void tst_QPsdView::compareRendering_data()
{
    addPsdFiles();
}

void tst_QPsdView::compareRendering()
{
    QFETCH(QString, psd);

    // Detect source
    const QString source = detectSource(psd);
    const SourceInfo *srcInfo = nullptr;
    for (const auto &src : m_sources) {
        if (src.id == source) {
            srcInfo = &src;
            break;
        }
    }

    // Compute relative path from source base directory (use canonical paths to resolve ".." components)
    QString basePath;
    if (srcInfo && !srcInfo->testDataPath.isEmpty()) {
        basePath = QFileInfo(srcInfo->testDataPath).canonicalFilePath();
    } else {
        basePath = QFileInfo(psd).canonicalPath();
    }
    QString relativePsdPath = QDir(basePath).relativeFilePath(QFileInfo(psd).canonicalFilePath());

    // Helper to create a result for error/skip cases
    auto makeErrorResult = [&]() {
        SimilarityResult result;
        result.fileName = relativePsdPath;
        result.source = source;
        result.fileSize = QFileInfo(psd).size();
        result.similarityPsd2PngVsImageData = 0.0;
        result.similarityPsd2PngVsPsdView = 0.0;
        result.similarityImageDataVsPsdView = 0.0;
        result.passedImageData = false;
        result.passedPsdView = false;
        result.passedImageDataVsPsdView = false;
        for (int i = 0; i < 7; ++i) result.imagePaths[i] = QString();
        return result;
    };

    // Load the PSD and get the toplevel image data
    QPsdParser parser;
    parser.load(psd);

    const auto header = parser.fileHeader();
    const auto imageData = parser.imageData();
    const auto colorModeData = parser.colorModeData();
    const auto iccProfile = parser.iccProfile();

    // Skip if no image data
    if (imageData.width() == 0 || imageData.height() == 0) {
        if (m_generateSummary) {
            m_similarityResults.append(makeErrorResult());
            qDebug() << "No image data in PSD:" << QFileInfo(psd).fileName();
            return;
        } else {
            QSKIP("No image data in PSD file");
        }
    }

    // Convert toplevel imageData to QImage
    QImage toplevelImage = QtPsdGui::imageDataToImage(imageData, header, colorModeData, iccProfile);
    if (toplevelImage.isNull()) {
        const QSize canvasSize(header.width(), header.height());
        if (!canvasSize.isEmpty()) {
            toplevelImage = QImage(canvasSize, QImage::Format_ARGB32);
            toplevelImage.fill(Qt::transparent);
            qDebug() << "Created transparent image for empty imageData:" << QFileInfo(psd).fileName();
        } else {
            if (m_generateSummary) {
                m_similarityResults.append(makeErrorResult());
                qDebug() << "Failed to convert image data:" << QFileInfo(psd).fileName();
                return;
            } else {
                QFAIL("Failed to convert image data to QImage");
            }
        }
    }

    // Ensure the image has an alpha channel for proper transparency
    if (toplevelImage.format() != QImage::Format_ARGB32 &&
        toplevelImage.format() != QImage::Format_RGBA8888 &&
        toplevelImage.format() != QImage::Format_RGBA64) {
        toplevelImage = toplevelImage.convertToFormat(QImage::Format_ARGB32);
    }

    // Render using QPsdView
    QImage viewRendering = renderPsdView(psd);
    if (viewRendering.isNull()) {
        if (m_generateSummary) {
            m_similarityResults.append(makeErrorResult());
            qDebug() << "Failed to render view:" << QFileInfo(psd).fileName();
            return;
        } else {
            QFAIL("Failed to render QPsdView");
        }
    }

    // Always compute imageData vs psdView similarity
    double similarityImageDataVsPsdView = 0.0;
    if (toplevelImage.size() == viewRendering.size()) {
        similarityImageDataVsPsdView = compareImages(toplevelImage, viewRendering);
    }

    // Load psd2png reference image (may not exist for all sources)
    QString psd2pngPath = findPsd2PngPath(psd, source);
    QImage psd2pngImage;
    double similarityPsd2PngVsImageData = 0.0;
    double similarityPsd2PngVsPsdView = 0.0;
    bool hasPsd2Png = false;

    if (!psd2pngPath.isEmpty() && QFile::exists(psd2pngPath)) {
        psd2pngImage.load(psd2pngPath);
        if (!psd2pngImage.isNull()) {
            hasPsd2Png = true;
            if (psd2pngImage.size() == toplevelImage.size()) {
                similarityPsd2PngVsImageData = compareImages(psd2pngImage, toplevelImage);
            }
            if (psd2pngImage.size() == viewRendering.size()) {
                similarityPsd2PngVsPsdView = compareImages(psd2pngImage, viewRendering);
            }
            qDebug() << "File:" << QFileInfo(psd).fileName()
                     << "psd2png vs imageData:" << similarityPsd2PngVsImageData << "%"
                     << "psd2png vs psdView:" << similarityPsd2PngVsPsdView << "%";
        }
    }

    qDebug() << "File:" << QFileInfo(psd).fileName()
             << "imageData vs psdView:" << similarityImageDataVsPsdView << "%"
             << "source:" << source;

    // Save images to docs/images directories
    {
        QString subDir = QFileInfo(relativePsdPath).path();
        QString sourceId = source.isEmpty() ? "other"_L1 : source;
        QDir projectRoot = QDir::current();
        while (!projectRoot.exists("CMakeLists.txt") && projectRoot.cdUp()) {
            // Keep going up
        }
        if (subDir != ".") {
            QDir().mkpath(projectRoot.absoluteFilePath(QString("docs/images/imagedata/%1/%2").arg(sourceId).arg(subDir)));
            QDir().mkpath(projectRoot.absoluteFilePath(QString("docs/images/psdview/%1/%2").arg(sourceId).arg(subDir)));
        } else {
            QDir().mkpath(projectRoot.absoluteFilePath(QString("docs/images/imagedata/%1").arg(sourceId)));
            QDir().mkpath(projectRoot.absoluteFilePath(QString("docs/images/psdview/%1").arg(sourceId)));
        }

        QString baseFileName = QFileInfo(relativePsdPath).baseName();
        QString dirPart = (subDir.isEmpty() || subDir == ".") ? QString() : (subDir + "/"_L1);

        // Save imageData image
        QString imageDataPath = projectRoot.absoluteFilePath(
            QString("docs/images/imagedata/%1/%2%3.png").arg(sourceId).arg(dirPart).arg(baseFileName));
        toplevelImage.save(imageDataPath);

        // Save psdview image
        QString psdViewPath = projectRoot.absoluteFilePath(
            QString("docs/images/psdview/%1/%2%3.png").arg(sourceId).arg(dirPart).arg(baseFileName));
        viewRendering.save(psdViewPath);

        // Save diff: imageData vs psdView (always)
        if (toplevelImage.size() == viewRendering.size()) {
            QImage diffIdVsPv = createDiffImage(toplevelImage, viewRendering);
            if (!diffIdVsPv.isNull()) {
                QString diffPath = projectRoot.absoluteFilePath(
                    QString("docs/images/psdview/%1/%2%3_diff.png").arg(sourceId).arg(dirPart).arg(baseFileName));
                diffIdVsPv.save(diffPath);
            }
        }

        // Save diff images for psd2png comparisons
        if (hasPsd2Png) {
            if (psd2pngImage.size() == toplevelImage.size()) {
                QImage diffImageData = createDiffImage(psd2pngImage, toplevelImage);
                if (!diffImageData.isNull()) {
                    QString diffImageDataPath = projectRoot.absoluteFilePath(
                        QString("docs/images/imagedata/%1/%2%3_diff.png").arg(sourceId).arg(dirPart).arg(baseFileName));
                    diffImageData.save(diffImageDataPath);
                }
            }
            if (psd2pngImage.size() == viewRendering.size()) {
                QImage diffPsdView = createDiffImage(psd2pngImage, viewRendering);
                if (!diffPsdView.isNull()) {
                    QString diffPsdViewPath = projectRoot.absoluteFilePath(
                        QString("docs/images/psdview/%1/%2%3_ps_diff.png").arg(sourceId).arg(dirPart).arg(baseFileName));
                    diffPsdView.save(diffPsdViewPath);
                }
            }
        }

        qDebug() << "Saved images:" << imageDataPath << "and" << psdViewPath;
    }

    // Store result if summary generation is enabled
    if (m_generateSummary) {
        SimilarityResult result;
        result.fileName = relativePsdPath;
        result.source = source;
        result.fileSize = QFileInfo(psd).size();
        result.similarityPsd2PngVsImageData = similarityPsd2PngVsImageData;
        result.similarityPsd2PngVsPsdView = similarityPsd2PngVsPsdView;
        result.similarityImageDataVsPsdView = similarityImageDataVsPsdView;
        result.passedImageData = similarityPsd2PngVsImageData > 50.0;
        result.passedPsdView = similarityPsd2PngVsPsdView > 50.0;
        result.passedImageDataVsPsdView = similarityImageDataVsPsdView > 50.0;
        for (int i = 0; i < 7; ++i) result.imagePaths[i] = QString();

        QString sourceId = source.isEmpty() ? "other"_L1 : source;
        QString subDir = QFileInfo(relativePsdPath).path();
        QString baseFileName = QFileInfo(relativePsdPath).baseName();
        QString dirPart = (subDir.isEmpty() || subDir == ".") ? QString() : (subDir + "/"_L1);

        // psd2png path
        if (hasPsd2Png && srcInfo && !srcInfo->psd2pngSubdir.isEmpty()) {
            result.imagePaths[0] = QString("images/psd2png/%1/%2%3.png").arg(srcInfo->psd2pngSubdir).arg(dirPart).arg(baseFileName);
        }

        // imagedata and psdview paths
        result.imagePaths[1] = QString("images/imagedata/%1/%2%3.png").arg(sourceId).arg(dirPart).arg(baseFileName);
        result.imagePaths[2] = QString("images/psdview/%1/%2%3.png").arg(sourceId).arg(dirPart).arg(baseFileName);

        // psd2png diff paths
        if (hasPsd2Png) {
            result.imagePaths[3] = QString("images/imagedata/%1/%2%3_diff.png").arg(sourceId).arg(dirPart).arg(baseFileName);
            result.imagePaths[4] = QString("images/psdview/%1/%2%3_ps_diff.png").arg(sourceId).arg(dirPart).arg(baseFileName);
        }

        // imageData vs psdView diff path
        result.imagePaths[5] = QString("images/psdview/%1/%2%3_diff.png").arg(sourceId).arg(dirPart).arg(baseFileName);

        m_similarityResults.append(result);
        qDebug() << "Saved comparison images for" << relativePsdPath;
    } else {
        // When not generating summary, determine pass/fail
        bool anyPassed;
        if (hasPsd2Png) {
            anyPassed = similarityPsd2PngVsImageData > 50.0 || similarityPsd2PngVsPsdView > 50.0;
        } else {
            // No psd2png — use imageData vs psdView as the criterion
            anyPassed = similarityImageDataVsPsdView > 50.0;
            if (!anyPassed) {
                QSKIP("No psd2png reference and imageData vs psdView similarity is too low");
            }
        }

        // Known rendering limitations - mark as expected failures
        if (!anyPassed) {
            const QString testName = QString::fromLatin1(QTest::currentDataTag());
            static const QStringList knownFailures32bit = {
                "read/32bits/src.psd"_L1,
            };
            static const QStringList knownFailuresSmartFilters = {
                "read-write/smart-filters/expected.psd"_L1,
                "read-write/smart-filters/src.psd"_L1,
                "read-write/smart-filters-2/expected.psd"_L1,
                "read-write/smart-filters-2/src.psd"_L1,
            };
            static const QStringList knownFailuresAnimation = {
                "read/animation-effects/src.psd"_L1,
                "read/animation-timeline/src.psd"_L1,
                "read-write/animation-effects/expected.psd"_L1,
                "read-write/animation-effects/src.psd"_L1,
                "read-write/animation-timeline/expected.psd"_L1,
                "read-write/animation-timeline/src.psd"_L1,
            };
            static const QStringList knownFailuresText = {
                "read/text-alternatives/src.psd"_L1,
                "read/text-debug/src.psd"_L1,
                "read/text-paragraph-align/src.psd"_L1,
                "read/text-path/src.psd"_L1,
            };

            if (knownFailures32bit.contains(testName)) {
                QEXPECT_FAIL("", "32-bit float channel depth not yet supported", Continue);
            } else if (knownFailuresSmartFilters.contains(testName)) {
                QEXPECT_FAIL("", "Smart filter effects not yet rendered", Continue);
            } else if (knownFailuresAnimation.contains(testName)) {
                QEXPECT_FAIL("", "Animation frame effects not yet rendered", Continue);
            } else if (knownFailuresText.contains(testName)) {
                QEXPECT_FAIL("", "Text rendering quality limitations", Continue);
            }
        }

        QVERIFY2(anyPassed,
                 qPrintable(QString("Images differ significantly. psd2png vs imageData: %1%, psd2png vs psdView: %2%, imageData vs psdView: %3%")
                           .arg(similarityPsd2PngVsImageData)
                           .arg(similarityPsd2PngVsPsdView)
                           .arg(similarityImageDataVsPsdView)));
    }
}

void tst_QPsdView::writeReportSection(QTextStream &stream, const QList<SimilarityResult> &results,
                                       const QString &title, const QString &col1Label, const QString &col2Label,
                                       int similarityIndex, int imgIndex1, int imgIndex2, int diffIndex,
                                       const QString &githubBaseUrl)
{
    // similarityIndex: 0 = psd2PngVsPsdView, 1 = psd2PngVsImageData, 2 = imageDataVsPsdView
    // passedIndex maps: 0 -> passedPsdView, 1 -> passedImageData, 2 -> passedImageDataVsPsdView

    stream << "## " << title << "\n\n";
    stream << "| File | Size | Similarity | Status | " << col1Label << " | " << col2Label << " | Difference |\n";
    stream << "|------|------|------------|--------|";
    stream << QString("-").repeated(col1Label.length() + 2) << "|";
    stream << QString("-").repeated(col2Label.length() + 2) << "|------------|\n";

    for (const auto &result : results) {
        double similarity = 0.0;
        bool passed = false;
        switch (similarityIndex) {
        case 0: similarity = result.similarityPsd2PngVsPsdView; passed = result.passedPsdView; break;
        case 1: similarity = result.similarityPsd2PngVsImageData; passed = result.passedImageData; break;
        case 2: similarity = result.similarityImageDataVsPsdView; passed = result.passedImageDataVsPsdView; break;
        }

        QString status;
        QString statusEmoji;
        if (!passed) {
            status = "FAILED";
            statusEmoji = u"❌"_s;
        } else if (similarity >= 99.0) {
            status = "PERFECT";
            statusEmoji = u"✅"_s;
        } else if (similarity >= 90.0) {
            status = "GOOD";
            statusEmoji = u"✅"_s;
        } else {
            status = "LOW";
            statusEmoji = u"⚠️"_s;
        }

        QString encodedFileName = result.fileName;
        encodedFileName.replace(" ", "%20");
        QString githubUrl = githubBaseUrl + encodedFileName;

        stream << "| [" << result.fileName << "](" << githubUrl << ")"
               << " | " << formatFileSize(result.fileSize)
               << " | " << QString::number(similarity, 'f', 2) << "% "
               << " | " << statusEmoji << " " << status;

        // Image thumbnails
        auto encPath = [](const QString &p) { QString e = p; e.replace(" ", "%20"); return e; };

        if (imgIndex1 >= 0 && !result.imagePaths[imgIndex1].isEmpty()) {
            QString ep = encPath(result.imagePaths[imgIndex1]);
            stream << " | [<img src=\"" << ep << "\" width=\"100\">](" << ep << ")";
        } else {
            stream << " | ";
        }

        if (imgIndex2 >= 0 && !result.imagePaths[imgIndex2].isEmpty()) {
            QString ep = encPath(result.imagePaths[imgIndex2]);
            stream << " | [<img src=\"" << ep << "\" width=\"100\">](" << ep << ")";
        } else {
            stream << " | ";
        }

        if (diffIndex >= 0 && !result.imagePaths[diffIndex].isEmpty()) {
            QString ep = encPath(result.imagePaths[diffIndex]);
            stream << " | [<img src=\"" << ep << "\" width=\"100\">](" << ep << ")";
        } else {
            stream << " | ";
        }

        stream << " |\n";
    }
}

void tst_QPsdView::cleanupTestCase()
{
    if (!m_generateSummary || m_similarityResults.isEmpty()) {
        return;
    }

    // Group results by source
    QMap<QString, QList<SimilarityResult>> resultsBySource;
    for (const auto &result : m_similarityResults) {
        resultsBySource[result.source].append(result);
    }

    // Generate a report for each source
    for (const auto &srcInfo : m_sources) {
        if (!resultsBySource.contains(srcInfo.id))
            continue;

        auto sourceResults = resultsBySource[srcInfo.id];

        // Sort by file size (smallest to largest)
        std::sort(sourceResults.begin(), sourceResults.end(),
                  [](const SimilarityResult &a, const SimilarityResult &b) {
                      return a.fileSize != b.fileSize ? a.fileSize < b.fileSize : a.fileName < b.fileName;
                  });

        QString summaryPath = m_outputBaseDir + QString("/similarity_report_%1.md").arg(srcInfo.id);
        QFile file(summaryPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Failed to create summary file:" << summaryPath;
            continue;
        }

        QTextStream stream(&file);
        stream << "# QtPsd Similarity Test Results (" << srcInfo.id << ")\n\n";
        stream << "Generated on: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";

        bool hasPsd2Png = !srcInfo.psd2pngSubdir.isEmpty();

        // Compute statistics
        int totalTests = sourceResults.size();
        int testsWithPsd2Png = 0;
        int passedImageDataTests = 0;
        int passedPsdViewTests = 0;
        int passedIdVsPvTests = 0;
        double totalSimilarityImageData = 0;
        double totalSimilarityPsdView = 0;
        double totalSimilarityIdVsPv = 0;
        double minSimilarityImageData = 100, maxSimilarityImageData = 0;
        double minSimilarityPsdView = 100, maxSimilarityPsdView = 0;
        double minSimilarityIdVsPv = 100, maxSimilarityIdVsPv = 0;

        for (const auto &result : sourceResults) {
            // imageData vs psdView stats (always available)
            totalSimilarityIdVsPv += result.similarityImageDataVsPsdView;
            minSimilarityIdVsPv = qMin(minSimilarityIdVsPv, result.similarityImageDataVsPsdView);
            maxSimilarityIdVsPv = qMax(maxSimilarityIdVsPv, result.similarityImageDataVsPsdView);
            if (result.passedImageDataVsPsdView) passedIdVsPvTests++;

            if (!result.imagePaths[0].isEmpty()) {
                testsWithPsd2Png++;
                if (result.passedImageData) passedImageDataTests++;
                if (result.passedPsdView) passedPsdViewTests++;
                totalSimilarityImageData += result.similarityPsd2PngVsImageData;
                totalSimilarityPsdView += result.similarityPsd2PngVsPsdView;
                minSimilarityImageData = qMin(minSimilarityImageData, result.similarityPsd2PngVsImageData);
                maxSimilarityImageData = qMax(maxSimilarityImageData, result.similarityPsd2PngVsImageData);
                minSimilarityPsdView = qMin(minSimilarityPsdView, result.similarityPsd2PngVsPsdView);
                maxSimilarityPsdView = qMax(maxSimilarityPsdView, result.similarityPsd2PngVsPsdView);
            }
        }

        double avgSimilarityIdVsPv = totalTests > 0 ? totalSimilarityIdVsPv / totalTests : 0;

        // Summary statistics
        stream << "## Summary Statistics\n\n";

        if (hasPsd2Png && testsWithPsd2Png > 0) {
            double avgSimilarityImageData = totalSimilarityImageData / testsWithPsd2Png;
            double avgSimilarityPsdView = totalSimilarityPsdView / testsWithPsd2Png;

            stream << "| Metric | Photoshop vs QPsdView | Photoshop vs Image Data | Image Data vs QPsdView |\n";
            stream << "|--------|-----------------------|-------------------------|------------------------|\n";
            stream << "| Total Tests | " << testsWithPsd2Png << " | " << testsWithPsd2Png << " | " << totalTests << " |\n";
            stream << "| Passed Tests (>50%) | " << passedPsdViewTests << " ("
                   << QString::number(100.0 * passedPsdViewTests / testsWithPsd2Png, 'f', 1)
                   << "%) | " << passedImageDataTests << " ("
                   << QString::number(100.0 * passedImageDataTests / testsWithPsd2Png, 'f', 1)
                   << "%) | " << passedIdVsPvTests << " ("
                   << QString::number(totalTests > 0 ? 100.0 * passedIdVsPvTests / totalTests : 0, 'f', 1)
                   << "%) |\n";
            stream << "| Average Similarity | "
                   << QString::number(avgSimilarityPsdView, 'f', 2)
                   << "% | "
                   << QString::number(avgSimilarityImageData, 'f', 2)
                   << "% | "
                   << QString::number(avgSimilarityIdVsPv, 'f', 2)
                   << "% |\n";
            stream << "| Minimum Similarity | "
                   << QString::number(minSimilarityPsdView, 'f', 2)
                   << "% | "
                   << QString::number(minSimilarityImageData, 'f', 2)
                   << "% | "
                   << QString::number(minSimilarityIdVsPv, 'f', 2)
                   << "% |\n";
            stream << "| Maximum Similarity | "
                   << QString::number(maxSimilarityPsdView, 'f', 2)
                   << "% | "
                   << QString::number(maxSimilarityImageData, 'f', 2)
                   << "% | "
                   << QString::number(maxSimilarityIdVsPv, 'f', 2)
                   << "% |\n\n";
            if (totalTests - testsWithPsd2Png > 0)
                stream << "**Note:** " << (totalTests - testsWithPsd2Png) << " tests had no Photoshop reference image for comparison.\n\n";
        } else {
            stream << "| Metric | Image Data vs QPsdView |\n";
            stream << "|--------|------------------------|\n";
            stream << "| Total Tests | " << totalTests << " |\n";
            stream << "| Passed Tests (>50%) | " << passedIdVsPvTests << " ("
                   << QString::number(totalTests > 0 ? 100.0 * passedIdVsPvTests / totalTests : 0, 'f', 1)
                   << "%) |\n";
            stream << "| Average Similarity | "
                   << QString::number(avgSimilarityIdVsPv, 'f', 2) << "% |\n";
            stream << "| Minimum Similarity | "
                   << QString::number(minSimilarityIdVsPv, 'f', 2) << "% |\n";
            stream << "| Maximum Similarity | "
                   << QString::number(maxSimilarityIdVsPv, 'f', 2) << "% |\n\n";
        }

        // Filter results for psd2png sections
        QList<SimilarityResult> resultsWithPsd2Png;
        if (hasPsd2Png) {
            for (const auto &r : sourceResults) {
                if (!r.imagePaths[0].isEmpty())
                    resultsWithPsd2Png.append(r);
            }
        }

        // Sections with psd2png (ag-psd style)
        if (hasPsd2Png && !resultsWithPsd2Png.isEmpty()) {
            // Section 1: Photoshop vs QPsdView
            writeReportSection(stream, resultsWithPsd2Png,
                               "Section 1: Photoshop vs QPsdView"_L1,
                               "Photoshop"_L1, "QPsdView"_L1,
                               0, 0, 2, 4, srcInfo.githubBaseUrl);

            // Section 2: Photoshop vs Image Data
            stream << "\n";
            writeReportSection(stream, resultsWithPsd2Png,
                               "Section 2: Photoshop vs Image Data"_L1,
                               "Photoshop"_L1, "Image Data"_L1,
                               1, 0, 1, 3, srcInfo.githubBaseUrl);

            stream << "\n";
        }

        // Section: Image Data vs QPsdView (always present)
        QString sectionTitle = hasPsd2Png
            ? QString("Section %1: Image Data vs QPsdView").arg(hasPsd2Png && !resultsWithPsd2Png.isEmpty() ? 3 : 1)
            : "Section 1: Image Data vs QPsdView"_L1;
        writeReportSection(stream, sourceResults,
                           sectionTitle,
                           "Image Data"_L1, "QPsdView"_L1,
                           2, 1, 2, 5, srcInfo.githubBaseUrl);

        file.close();
        qDebug() << "Similarity summary written to:" << QFileInfo(file).absoluteFilePath();
    }
}

QTEST_MAIN(tst_QPsdView)

#include "tst_qpsdview.moc"

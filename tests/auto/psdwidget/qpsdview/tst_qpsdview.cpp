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
    void addPsdFiles();
    QImage renderPsdView(const QString &filePath);
    double compareImages(const QImage &img1, const QImage &img2);
    QString findPsd2PngPath(const QString &psdPath);
    QImage createDiffImage(const QImage &img1, const QImage &img2);

    struct SimilarityResult {
        QString fileName;
        double similarityPsd2PngVsImageData;
        double similarityPsd2PngVsPsdView;
        bool passedImageData;
        bool passedPsdView;
        QString imagePaths[5]; // 0: psd2png, 1: imagedata, 2: psdview, 3: diff_imagedata, 4: diff_psdview
    };
    QList<SimilarityResult> m_similarityResults;
    bool m_generateSummary;
    QString m_outputBaseDir;
    QString m_projectRoot;
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
}

void tst_QPsdView::addPsdFiles()
{
    QTest::addColumn<QString>("psd");

    const QString basePath = QFINDTESTDATA("../../psdcore/ag-psd/ag-psd/test/");
    QDir baseDir(basePath);
    QDir dir(basePath);

    std::function<void(QDir *dir)> findPsd;
    findPsd = [&](QDir *dir) {
        for (const QString &subdir : dir->entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            dir->cd(subdir);
            findPsd(dir);
            dir->cdUp();
        }
        for (const QString &fileName : dir->entryList(QStringList() << "*.psd")) {
            // Use relative path from base directory for test row name
            QString relativePath = baseDir.relativeFilePath(dir->filePath(fileName));
            QTest::newRow(relativePath.toLatin1().data()) << dir->filePath(fileName);
        }
    };

    findPsd(&dir);
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
    view.setModel(&model);
    view.setShowChecker(false);
    view.reset();

    // Get the size from the model
    const QSize canvasSize = model.size();
    if (canvasSize.isEmpty()) {
        return QImage();
    }

    // Set widget attributes for offscreen rendering
    view.setAttribute(Qt::WA_DontShowOnScreen);

    // Ensure the widget is properly sized and laid out
    view.resize(canvasSize);
    view.show(); // Still need to "show" for layout to work, but WA_DontShowOnScreen prevents actual display
    QApplication::processEvents();

    // Render to image using grab()
    QPixmap pixmap = view.grab();
    QImage rendered = pixmap.toImage();

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
            
            // Check if either pixel has actual content (not white/transparent)
            bool aHasContent = (a1 > 128) && (r1 < 250 || g1 < 250 || b1 < 250);
            bool bHasContent = (a2 > 128) && (r2 < 250 || g2 < 250 || b2 < 250);
            
            if (aHasContent || bHasContent) {
                pixelsWithContent++;
                
                // Convert to HSV for perceptually accurate comparison
                auto [h1, s1, v1] = rgbToHsv(r1, g1, b1);
                auto [h2, s2, v2] = rgbToHsv(r2, g2, b2);
                
                // Calculate hue difference (circular)
                double hDiff = qAbs(h1 - h2);
                if (hDiff > 180) hDiff = 360 - hDiff;
                hDiff = hDiff / 180.0; // Normalize to 0-1
                
                // Calculate saturation and value differences
                double sDiff = qAbs(s1 - s2);
                double vDiff = qAbs(v1 - v2);
                
                // Alpha difference (normalized to 0-1)
                double aDiff = qAbs(a1 - a2) / 255.0;
                
                // Weighted combination (Value and Alpha are most important for perception)
                double pixelDiff = hDiff * 0.2 + sDiff * 0.2 + vDiff * 0.4 + aDiff * 0.2;
                
                // Accumulate difference
                totalDiff += pixelDiff;
                
                // Count as different if the difference is significant
                if (pixelDiff > 0.1) {
                    pixelsDifferent++;
                }
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
    
    // Debug for smart-filters-2
    static int debugCount = 0;
    if (debugCount < 5 && similarity > 50 && pixelsDifferent > 1000) {
        qDebug() << "Similarity calculation:"
                 << "pixelsWithContent=" << pixelsWithContent
                 << "pixelsDifferent=" << pixelsDifferent
                 << "contentBasedSimilarity=" << contentBasedSimilarity << "%"
                 << "hsvBasedSimilarity=" << hsvBasedSimilarity << "%"
                 << "final similarity=" << similarity << "%";
        debugCount++;
    }
    
    return similarity;
}

QString tst_QPsdView::findPsd2PngPath(const QString &psdPath)
{
    // Get relative path from ag-psd test directory
    QString basePath = QFINDTESTDATA("../../psdcore/ag-psd/ag-psd/test/");
    QString relativePsdPath = QDir(basePath).relativeFilePath(psdPath);

    // Remove .psd extension
    QString pathWithoutExt = relativePsdPath.left(relativePsdPath.lastIndexOf('.'));

    // Get project root to construct paths
    QDir projectRoot = QDir::current();
    while (!projectRoot.exists("CMakeLists.txt") && projectRoot.cdUp()) {
        // Keep going up
    }
    QString psd2pngBase = projectRoot.absoluteFilePath("docs/images/psd2png/ag-psd/");
    
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
    diff.fill(Qt::white);  // Start with white background

    for (int y = 0; y < diff.height(); ++y) {
        for (int x = 0; x < diff.width(); ++x) {
            QRgb p1 = img1.pixel(x, y);
            QRgb p2 = img2.pixel(x, y);

            // Get color components
            int r1 = qRed(p1), g1 = qGreen(p1), b1 = qBlue(p1), a1 = qAlpha(p1);
            int r2 = qRed(p2), g2 = qGreen(p2), b2 = qBlue(p2), a2 = qAlpha(p2);
            
            // Check if pixels are effectively the same (considering alpha)
            bool p1HasContent = (a1 > 128) && (r1 < 128 || g1 < 128 || b1 < 128); // Non-transparent and dark
            bool p2HasContent = (a2 > 128) && (r2 < 128 || g2 < 128 || b2 < 128); // Non-transparent and dark
            
            if (p1HasContent && !p2HasContent) {
                // Only in img1 (psd2png) - show as red
                diff.setPixel(x, y, qRgba(255, 0, 0, 255));
            } else if (!p1HasContent && p2HasContent) {
                // Only in img2 - show as blue
                diff.setPixel(x, y, qRgba(0, 0, 255, 255));
            } else if (p1HasContent && p2HasContent) {
                // In both - check if they're actually different
                int colorDiff = qAbs(r1 - r2) + qAbs(g1 - g2) + qAbs(b1 - b2);
                if (colorDiff > 30) {
                    // Different colors - show as yellow
                    diff.setPixel(x, y, qRgba(255, 255, 0, 255));
                } else {
                    // Same/similar colors - show as gray
                    diff.setPixel(x, y, qRgba(128, 128, 128, 255));
                }
            }
            // else: neither has content, leave as white
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

    // Load the PSD and get the toplevel image data
    QPsdParser parser;
    parser.load(psd);

    const auto header = parser.fileHeader();
    const auto imageData = parser.imageData();

    // Skip if no image data
    if (imageData.width() == 0 || imageData.height() == 0) {
        if (m_generateSummary) {
            // Record as skipped with 0% similarity
            SimilarityResult result;
            result.fileName = QDir(QFINDTESTDATA("../../psdcore/ag-psd/ag-psd/test/")).relativeFilePath(psd);
            result.similarityPsd2PngVsImageData = 0.0;
            result.similarityPsd2PngVsPsdView = 0.0;
            result.passedImageData = false;
            result.passedPsdView = false;
            for (int i = 0; i < 5; ++i) result.imagePaths[i] = QString();
            m_similarityResults.append(result);
            qDebug() << "No image data in PSD:" << QFileInfo(psd).fileName();
            return;
        } else {
            QSKIP("No image data in PSD file");
        }
    }

    // Convert toplevel imageData to QImage
    QImage toplevelImage = QtPsdGui::imageDataToImage(imageData, header);
    if (toplevelImage.isNull()) {
        if (m_generateSummary) {
            SimilarityResult result;
            result.fileName = QDir(QFINDTESTDATA("../../psdcore/ag-psd/ag-psd/test/")).relativeFilePath(psd);
            result.similarityPsd2PngVsImageData = 0.0;
            result.similarityPsd2PngVsPsdView = 0.0;
            result.passedImageData = false;
            result.passedPsdView = false;
            for (int i = 0; i < 5; ++i) result.imagePaths[i] = QString();
            m_similarityResults.append(result);
            qDebug() << "Failed to convert image data:" << QFileInfo(psd).fileName();
            return;
        } else {
            QFAIL("Failed to convert image data to QImage");
        }
    }

    // Render using QPsdView
    QImage viewRendering = renderPsdView(psd);
    if (viewRendering.isNull()) {
        if (m_generateSummary) {
            SimilarityResult result;
            result.fileName = QDir(QFINDTESTDATA("../../psdcore/ag-psd/ag-psd/test/")).relativeFilePath(psd);
            result.similarityPsd2PngVsImageData = 0.0;
            result.similarityPsd2PngVsPsdView = 0.0;
            result.passedImageData = false;
            result.passedPsdView = false;
            for (int i = 0; i < 5; ++i) result.imagePaths[i] = QString();
            m_similarityResults.append(result);
            qDebug() << "Failed to render view:" << QFileInfo(psd).fileName();
            return;
        } else {
            QFAIL("Failed to render QPsdView");
        }
    }

    // Load psd2png reference image
    QString psd2pngPath = findPsd2PngPath(psd);
    QImage psd2pngImage;
    double similarityPsd2PngVsImageData = 0.0;
    double similarityPsd2PngVsPsdView = 0.0;

    if (psd2pngPath.isEmpty() || !QFile::exists(psd2pngPath)) {
        // Skip test if no corresponding psd2png file exists
        if (m_generateSummary) {
            // Don't add to results - just skip silently
            return;
        } else {
            QSKIP("No corresponding psd2png reference image found");
        }
    }

    psd2pngImage.load(psd2pngPath);
    if (!psd2pngImage.isNull()) {
        // Compare psd2png with imageData
        if (psd2pngImage.size() == toplevelImage.size()) {
            similarityPsd2PngVsImageData = compareImages(psd2pngImage, toplevelImage);
        }

        // Compare psd2png with psdView
        if (psd2pngImage.size() == viewRendering.size()) {
            similarityPsd2PngVsPsdView = compareImages(psd2pngImage, viewRendering);
        }

        qDebug() << "File:" << QFileInfo(psd).fileName()
                 << "psd2png vs imageData:" << similarityPsd2PngVsImageData << "%"
                 << "psd2png vs psdView:" << similarityPsd2PngVsPsdView << "%";
    } else {
        qDebug() << "Failed to load psd2png reference image:" << psd2pngPath;
    }

    // Save images to docs/images directories
    {
        // Get relative path from ag-psd test directory
        QString basePath = QFINDTESTDATA("../../psdcore/ag-psd/ag-psd/test/");
        QString relativePsdPath = QDir(basePath).relativeFilePath(psd);

        // Create subdirectory structure if needed
        QString subDir = QFileInfo(relativePsdPath).path();
        // Get project root
        QDir projectRoot = QDir::current();
        while (!projectRoot.exists("CMakeLists.txt") && projectRoot.cdUp()) {
            // Keep going up
        }
        if (subDir != ".") {
            QDir().mkpath(projectRoot.absoluteFilePath(QString("docs/images/imagedata/ag-psd/%1").arg(subDir)));
            QDir().mkpath(projectRoot.absoluteFilePath(QString("docs/images/psdview/ag-psd/%1").arg(subDir)));
        }

        // Create filename without extension
        QString baseFileName = QFileInfo(relativePsdPath).baseName();

        // Save imageData image
        QString imageDataPath = projectRoot.absoluteFilePath(QString("docs/images/imagedata/ag-psd/%1/%2.png")
                                .arg(subDir.isEmpty() ? "." : subDir)
                                .arg(baseFileName));
        toplevelImage.save(imageDataPath);

        // Save psdview image
        QString psdViewPath = projectRoot.absoluteFilePath(QString("docs/images/psdview/ag-psd/%1/%2.png")
                              .arg(subDir.isEmpty() ? "." : subDir)
                              .arg(baseFileName));
        viewRendering.save(psdViewPath);

        // Save diff images alongside the main images if psd2png is available
        if (!psd2pngImage.isNull()) {
            // Diff for psd2png vs imageData
            if (psd2pngImage.size() == toplevelImage.size()) {
                QImage diffImageData = createDiffImage(psd2pngImage, toplevelImage);
                if (!diffImageData.isNull()) {
                    QString diffImageDataPath = projectRoot.absoluteFilePath(QString("docs/images/imagedata/ag-psd/%1/%2_diff.png")
                                                .arg(subDir.isEmpty() ? "." : subDir)
                                                .arg(baseFileName));
                    diffImageData.save(diffImageDataPath);
                }
            }

            // Diff for psd2png vs psdView
            if (psd2pngImage.size() == viewRendering.size()) {
                QImage diffPsdView = createDiffImage(psd2pngImage, viewRendering);
                if (!diffPsdView.isNull()) {
                    QString diffPsdViewPath = projectRoot.absoluteFilePath(QString("docs/images/psdview/ag-psd/%1/%2_diff.png")
                                              .arg(subDir.isEmpty() ? "." : subDir)
                                              .arg(baseFileName));
                    diffPsdView.save(diffPsdViewPath);
                }
            }
        }

        qDebug() << "Saved images:" << imageDataPath << "and" << psdViewPath;
    }

    // Store result if summary generation is enabled
    if (m_generateSummary) {
        SimilarityResult result;
        result.fileName = QDir(QFINDTESTDATA("../../psdcore/ag-psd/ag-psd/test/")).relativeFilePath(psd);
        result.similarityPsd2PngVsImageData = similarityPsd2PngVsImageData;
        result.similarityPsd2PngVsPsdView = similarityPsd2PngVsPsdView;
        result.passedImageData = similarityPsd2PngVsImageData > 50.0;
        result.passedPsdView = similarityPsd2PngVsPsdView > 50.0;
        // Initialize image paths
        for (int i = 0; i < 5; ++i) result.imagePaths[i] = QString();
        m_similarityResults.append(result);

        // Create a safe filename from the PSD path
        QString relativePsdPath = QDir(QFINDTESTDATA("../../psdcore/ag-psd/ag-psd/test/")).relativeFilePath(psd);
        QString safeFileName = relativePsdPath;
        safeFileName.replace('/', '_');
        safeFileName.replace('\\', '_');
        safeFileName = safeFileName.left(safeFileName.lastIndexOf('.')); // Remove .psd extension

        QString imageDir = m_generateSummary ? (m_outputBaseDir + "/images") : QDir::current().absolutePath();

        // Images are already saved in the docs/images directories above
        // No need to save them again here

        // Store relative paths for the summary if enabled
        if (m_generateSummary && m_similarityResults.size() > 0) {
            // Find the last result (the one we just added)
            auto& lastResult = m_similarityResults.last();

            // Get the relative path components for proper directory structure
            QString basePath = QFINDTESTDATA("../../psdcore/ag-psd/ag-psd/test/");
            QString relativePath = QDir(basePath).relativeFilePath(psd);
            QString subDir = QFileInfo(relativePath).path();
            QString baseFileName = QFileInfo(relativePath).baseName();

            // Construct paths matching the actual saved locations
            if (!psd2pngImage.isNull()) {
                if (subDir.isEmpty() || subDir == ".") {
                    lastResult.imagePaths[0] = QString("images/psd2png/ag-psd/%1.png").arg(baseFileName);
                } else {
                    lastResult.imagePaths[0] = QString("images/psd2png/ag-psd/%1/%2.png").arg(subDir).arg(baseFileName);
                }
            }

            if (subDir.isEmpty() || subDir == ".") {
                lastResult.imagePaths[1] = QString("images/imagedata/ag-psd/%1.png").arg(baseFileName);
                lastResult.imagePaths[2] = QString("images/psdview/ag-psd/%1.png").arg(baseFileName);
            } else {
                lastResult.imagePaths[1] = QString("images/imagedata/ag-psd/%1/%2.png").arg(subDir).arg(baseFileName);
                lastResult.imagePaths[2] = QString("images/psdview/ag-psd/%1/%2.png").arg(subDir).arg(baseFileName);
            }

            if (!psd2pngImage.isNull()) {
                if (subDir.isEmpty() || subDir == ".") {
                    lastResult.imagePaths[3] = QString("images/imagedata/ag-psd/%1_diff.png").arg(baseFileName);
                    lastResult.imagePaths[4] = QString("images/psdview/ag-psd/%1_diff.png").arg(baseFileName);
                } else {
                    lastResult.imagePaths[3] = QString("images/imagedata/ag-psd/%1/%2_diff.png").arg(subDir).arg(baseFileName);
                    lastResult.imagePaths[4] = QString("images/psdview/ag-psd/%1/%2_diff.png").arg(subDir).arg(baseFileName);
                }
            }
        }

        qDebug() << "Saved comparison images for" << relativePsdPath;
    } else {
        // When not generating summary, verify at least one comparison passes
        bool anyPassed = similarityPsd2PngVsImageData > 50.0 || similarityPsd2PngVsPsdView > 50.0;
        QVERIFY2(anyPassed,
                 qPrintable(QString("Images differ significantly. psd2png vs imageData: %1%, psd2png vs psdView: %2%")
                           .arg(similarityPsd2PngVsImageData)
                           .arg(similarityPsd2PngVsPsdView)));
    }
}

void tst_QPsdView::cleanupTestCase()
{
    if (!m_generateSummary || m_similarityResults.isEmpty()) {
        return;
    }

    // Sort results by average similarity (lowest first)
    std::sort(m_similarityResults.begin(), m_similarityResults.end(),
              [](const SimilarityResult &a, const SimilarityResult &b) {
                  double avgA = (a.similarityPsd2PngVsImageData + a.similarityPsd2PngVsPsdView) / 2.0;
                  double avgB = (b.similarityPsd2PngVsImageData + b.similarityPsd2PngVsPsdView) / 2.0;
                  return avgA < avgB;
              });

    // Generate markdown summary
    QString summaryPath = m_outputBaseDir + "/similarity_report.md";
    QFile file(summaryPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create summary file:" << summaryPath;
        return;
    }

    QTextStream stream(&file);
    stream << "# QPsdView Similarity Test Results\n\n";
    stream << "Generated on: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";

    // Summary statistics
    int totalTests = m_similarityResults.size();
    int passedImageDataTests = 0;
    int passedPsdViewTests = 0;
    int testsWithPsd2Png = 0;
    double totalSimilarityImageData = 0;
    double totalSimilarityPsdView = 0;
    double minSimilarityImageData = 100;
    double maxSimilarityImageData = 0;
    double minSimilarityPsdView = 100;
    double maxSimilarityPsdView = 0;

    for (const auto &result : m_similarityResults) {
        if (!result.imagePaths[0].isEmpty()) { // Has psd2png reference
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

    double avgSimilarityImageData = testsWithPsd2Png > 0 ? totalSimilarityImageData / testsWithPsd2Png : 0;
    double avgSimilarityPsdView = testsWithPsd2Png > 0 ? totalSimilarityPsdView / testsWithPsd2Png : 0;

    stream << "## Summary Statistics\n\n";
    stream << "| Metric | psd2png vs imageData | psd2png vs psdView |\n";
    stream << "|--------|---------------------|-------------------|\n";
    stream << "| Total Tests | " << testsWithPsd2Png << " | " << testsWithPsd2Png << " |\n";
    stream << "| Passed Tests (>50%) | " << passedImageDataTests << " ("
           << QString::number(testsWithPsd2Png > 0 ? 100.0 * passedImageDataTests / testsWithPsd2Png : 0, 'f', 1)
           << "%) | " << passedPsdViewTests << " ("
           << QString::number(testsWithPsd2Png > 0 ? 100.0 * passedPsdViewTests / testsWithPsd2Png : 0, 'f', 1) << "%) |\n";
    stream << "| Average Similarity | " << QString::number(avgSimilarityImageData, 'f', 2)
           << "% | " << QString::number(avgSimilarityPsdView, 'f', 2) << "% |\n";
    stream << "| Minimum Similarity | " << QString::number(minSimilarityImageData, 'f', 2)
           << "% | " << QString::number(minSimilarityPsdView, 'f', 2) << "% |\n";
    stream << "| Maximum Similarity | " << QString::number(maxSimilarityImageData, 'f', 2)
           << "% | " << QString::number(maxSimilarityPsdView, 'f', 2) << "% |\n\n";
    stream << "**Note:** " << (totalTests - testsWithPsd2Png) << " tests had no psd2png reference image for comparison.\n\n";

    // Section 1: psd2png vs imageData
    stream << "## Section 1: psd2png vs imageData\n\n";
    stream << "| File | Similarity | Status | psd2png | imageData | Difference |\n";
    stream << "|------|------------|--------|---------|-----------|------------|\n";

    // Sort by psd2png vs imageData similarity for section 1
    auto sortedByImageData = m_similarityResults;
    std::sort(sortedByImageData.begin(), sortedByImageData.end(),
              [](const SimilarityResult &a, const SimilarityResult &b) {
                  return a.similarityPsd2PngVsImageData < b.similarityPsd2PngVsImageData;
              });

    for (const auto &result : sortedByImageData) {
        if (result.imagePaths[0].isEmpty()) continue; // Skip if no psd2png reference

        QString status;
        QString statusEmoji;
        if (!result.passedImageData) {
            status = "FAILED";
            statusEmoji = "❌";
        } else if (result.similarityPsd2PngVsImageData >= 99.0) {
            status = "PERFECT";
            statusEmoji = "✅";
        } else if (result.similarityPsd2PngVsImageData >= 90.0) {
            status = "GOOD";
            statusEmoji = "✅";
        } else {
            status = "LOW";
            statusEmoji = "⚠️";
        }

        // Create GitHub link to the PSD file
        QString encodedFileName = result.fileName;
        encodedFileName.replace(" ", "%20");
        QString githubUrl = QString("https://github.com/Agamnentzar/ag-psd/tree/master/test/%1").arg(encodedFileName);

        stream << "| [" << result.fileName << "](" << githubUrl << ")"
               << " | " << QString::number(result.similarityPsd2PngVsImageData, 'f', 2) << "% "
               << " | " << statusEmoji << " " << status;

        // Add image thumbnails with links
        // URL-encode spaces in image paths
        QString encodedPath0 = result.imagePaths[0];
        QString encodedPath1 = result.imagePaths[1];
        QString encodedPath3 = result.imagePaths[3];
        encodedPath0.replace(" ", "%20");
        encodedPath1.replace(" ", "%20");
        encodedPath3.replace(" ", "%20");

        stream << " | [<img src=\"" << encodedPath0
               << "\" width=\"100\">](" << encodedPath0 << ")";
        stream << " | [<img src=\"" << encodedPath1
               << "\" width=\"100\">](" << encodedPath1 << ")";
        stream << " | [<img src=\"" << encodedPath3
               << "\" width=\"100\">](" << encodedPath3 << ")";

        stream << " |\n";
    }

    // Section 2: psd2png vs psdView
    stream << "\n## Section 2: psd2png vs psdView\n\n";
    stream << "| File | Similarity | Status | psd2png | psdView | Difference |\n";
    stream << "|------|------------|--------|---------|---------|------------|\n";

    // Sort by psd2png vs psdView similarity for section 2
    auto sortedByPsdView = m_similarityResults;
    std::sort(sortedByPsdView.begin(), sortedByPsdView.end(),
              [](const SimilarityResult &a, const SimilarityResult &b) {
                  return a.similarityPsd2PngVsPsdView < b.similarityPsd2PngVsPsdView;
              });

    for (const auto &result : sortedByPsdView) {
        if (result.imagePaths[0].isEmpty()) continue; // Skip if no psd2png reference

        QString status;
        QString statusEmoji;
        if (!result.passedPsdView) {
            status = "FAILED";
            statusEmoji = "❌";
        } else if (result.similarityPsd2PngVsPsdView >= 99.0) {
            status = "PERFECT";
            statusEmoji = "✅";
        } else if (result.similarityPsd2PngVsPsdView >= 90.0) {
            status = "GOOD";
            statusEmoji = "✅";
        } else {
            status = "LOW";
            statusEmoji = "⚠️";
        }

        // Create GitHub link to the PSD file
        QString encodedFileName = result.fileName;
        encodedFileName.replace(" ", "%20");
        QString githubUrl = QString("https://github.com/Agamnentzar/ag-psd/tree/master/test/%1").arg(encodedFileName);

        stream << "| [" << result.fileName << "](" << githubUrl << ")"
               << " | " << QString::number(result.similarityPsd2PngVsPsdView, 'f', 2) << "% "
               << " | " << statusEmoji << " " << status;

        // Add image thumbnails with links
        // URL-encode spaces in image paths
        QString encodedPath0 = result.imagePaths[0];
        QString encodedPath2 = result.imagePaths[2];
        QString encodedPath4 = result.imagePaths[4];
        encodedPath0.replace(" ", "%20");
        encodedPath2.replace(" ", "%20");
        encodedPath4.replace(" ", "%20");

        stream << " | [<img src=\"" << encodedPath0
               << "\" width=\"100\">](" << encodedPath0 << ")";
        stream << " | [<img src=\"" << encodedPath2
               << "\" width=\"100\">](" << encodedPath2 << ")";
        stream << " | [<img src=\"" << encodedPath4
               << "\" width=\"100\">](" << encodedPath4 << ")";

        stream << " |\n";
    }

    file.close();
    qDebug() << "Similarity summary written to:" << QFileInfo(file).absoluteFilePath();
}

QTEST_MAIN(tst_QPsdView)

#include "tst_qpsdview.moc"

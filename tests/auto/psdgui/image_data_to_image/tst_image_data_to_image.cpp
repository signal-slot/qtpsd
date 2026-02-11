// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtGui/QImage>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>
#include <QtPsdCore/QPsdFileHeader>
#include <QtPsdCore/QPsdImageData>
#include <QtPsdCore/QPsdLayerRecord>
#include <QtPsdCore/QPsdParser>
#include <QtPsdGui/qpsdguiglobal.h>
#include <QtTest/QtTest>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

class tst_ImageDataToImage : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void imageData_data();
    void imageData();
    void layerImageData_data();
    void layerImageData();
    void generateImages_data();
    void generateImages();

private:
    struct SourceInfo {
        QString id;
        QString rootPath;
    };

    void addPsdFiles();
    void addPsdFiles(const QList<SourceInfo> &sources, int limit);
    QList<SourceInfo> allSources() const;
    QList<SourceInfo> imageSources() const;
    QString imageOutputDir() const;
    int imageLimit() const;
    QString detectSource(const QString &psdPath, const QList<SourceInfo> &sources) const;
    QString sourceRelativePath(const QString &psdPath, const QString &sourceId, const QList<SourceInfo> &sources) const;

    bool m_generateImagesEnabled = false;
    QString m_generateImagesOutputDir;
    QList<SourceInfo> m_generateImagesSources;
    int m_generateImagesLimit = 0;
};

void tst_ImageDataToImage::addPsdFiles()
{
    const QList<SourceInfo> sources = {
        {"ag-psd"_L1, QFINDTESTDATA("../../3rdparty/ag-psd/test/")},
        {"psd-tools"_L1, QFINDTESTDATA("../../3rdparty/psd-tools/tests/psd_files/")},
    };
    addPsdFiles(sources, 0);
}

void tst_ImageDataToImage::addPsdFiles(const QList<SourceInfo> &sources, int limit)
{
    QTest::addColumn<QString>("psd");

    struct RowData {
        QString name;
        QString path;
    };

    QList<RowData> rows;
    for (const auto &source : sources) {
        if (source.rootPath.isEmpty() || !QDir(source.rootPath).exists()) {
            continue;
        }
        QDirIterator it(source.rootPath, QStringList() << "*.psd", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString filePath = it.next();
            const QString relativePath = QDir(source.rootPath).relativeFilePath(filePath);
            rows.append({source.id + "/"_L1 + relativePath, filePath});
        }
    }

    std::sort(rows.begin(), rows.end(), [](const RowData &a, const RowData &b) {
        return a.name < b.name;
    });

    int added = 0;
    for (const auto &row : rows) {
        if (limit > 0 && added >= limit) {
            break;
        }
        const QByteArray rowName = row.name.toUtf8();
        QTest::newRow(rowName.constData()) << row.path;
        ++added;
    }
}

QList<tst_ImageDataToImage::SourceInfo> tst_ImageDataToImage::allSources() const
{
    return {
        {"ag-psd"_L1, QFINDTESTDATA("../../3rdparty/ag-psd/test/")},
        {"psd-zoo"_L1, QFINDTESTDATA("../../3rdparty/psd-zoo/")},
        {"psd-tools"_L1, QFINDTESTDATA("../../3rdparty/psd-tools/tests/psd_files/")},
    };
}

QList<tst_ImageDataToImage::SourceInfo> tst_ImageDataToImage::imageSources() const
{
    QList<SourceInfo> filtered;
    const QList<SourceInfo> sources = allSources();
    QString sourceList = qEnvironmentVariable("QTPSD_IMAGE_SOURCES");
    if (sourceList.isEmpty()) {
        sourceList = QStringLiteral("psd-zoo");
    }
    const QStringList wanted = sourceList.split(',', Qt::SkipEmptyParts);

    for (const auto &source : sources) {
        for (const auto &id : wanted) {
            if (source.id.compare(id.trimmed(), Qt::CaseInsensitive) == 0) {
                filtered.append(source);
                break;
            }
        }
    }

    return filtered;
}

QString tst_ImageDataToImage::imageOutputDir() const
{
    QString outputPath = qEnvironmentVariable("QTPSD_IMAGE_OUTPUT_PATH");
    if (outputPath.isEmpty()) {
        return QString();
    }

    if (!QDir::isRelativePath(outputPath)) {
        return outputPath;
    }

    QDir projectRoot = QDir::current();
    while (!projectRoot.exists("CMakeLists.txt") && projectRoot.cdUp()) {
        // Keep going up until project root is found.
    }
    return projectRoot.absoluteFilePath(outputPath);
}

int tst_ImageDataToImage::imageLimit() const
{
    bool ok = false;
    const int limit = qEnvironmentVariable("QTPSD_IMAGE_LIMIT").toInt(&ok);
    return (ok && limit > 0) ? limit : 0;
}

QString tst_ImageDataToImage::detectSource(const QString &psdPath, const QList<SourceInfo> &sources) const
{
    const QString canonicalPsd = QFileInfo(psdPath).canonicalFilePath();
    for (const auto &source : sources) {
        if (source.rootPath.isEmpty()) {
            continue;
        }
        const QString canonicalBase = QFileInfo(source.rootPath).canonicalFilePath();
        if (!canonicalBase.isEmpty() && canonicalPsd.startsWith(canonicalBase)) {
            return source.id;
        }
    }
    return QString();
}

QString tst_ImageDataToImage::sourceRelativePath(const QString &psdPath, const QString &sourceId, const QList<SourceInfo> &sources) const
{
    for (const auto &source : sources) {
        if (source.id != sourceId) {
            continue;
        }
        const QString canonicalBase = QFileInfo(source.rootPath).canonicalFilePath();
        const QString canonicalPsd = QFileInfo(psdPath).canonicalFilePath();
        if (!canonicalBase.isEmpty() && !canonicalPsd.isEmpty()) {
            return QDir(canonicalBase).relativeFilePath(canonicalPsd);
        }
    }
    return QFileInfo(psdPath).fileName();
}

void tst_ImageDataToImage::initTestCase()
{
    m_generateImagesOutputDir = imageOutputDir();
    m_generateImagesEnabled = !m_generateImagesOutputDir.isEmpty();
    m_generateImagesSources = imageSources();
    m_generateImagesLimit = imageLimit();
}

void tst_ImageDataToImage::imageData_data()
{
    addPsdFiles();
}

void tst_ImageDataToImage::imageData()
{
    QFETCH(QString, psd);

    QPsdParser parser;
    parser.load(psd);

    const auto header = parser.fileHeader();
    const auto colorModeData = parser.colorModeData();
    const auto iccProfile = parser.iccProfile();

    const auto imageData = parser.imageData();
    if (imageData.width() > 0 && imageData.height() > 0) {
        const QImage image = QtPsdGui::imageDataToImage(imageData, header, colorModeData, iccProfile);
        QVERIFY(!image.isNull());
        QCOMPARE(image.width(), imageData.width());
        QCOMPARE(image.height(), imageData.height());
    }
}

void tst_ImageDataToImage::layerImageData_data()
{
    addPsdFiles();
}

void tst_ImageDataToImage::layerImageData()
{
    QFETCH(QString, psd);

    QPsdParser parser;
    parser.load(psd);

    const auto header = parser.fileHeader();
    const auto colorModeData = parser.colorModeData();

    const auto layerAndMaskInfo = parser.layerAndMaskInformation();
    const auto layerInfo = layerAndMaskInfo.layerInfo();
    const auto layers = layerInfo.records();

    for (int i = 0; i < layers.size(); ++i) {
        const auto &layer = layers[i];
        const auto imageData = layer.imageData();

        // Skip empty layers
        if (imageData.width() == 0 || imageData.height() == 0) {
            continue;
        }

        const QImage image = QtPsdGui::imageDataToImage(imageData, header, colorModeData);
        const auto depth = imageData.depth();
        QVERIFY(!image.isNull());
        QCOMPARE(image.width(), imageData.width());
        QCOMPARE(image.height(), imageData.height());
        // Additional validation based on format
        if (header.colorMode() == QPsdFileHeader::RGB && imageData.hasAlpha() && depth == 8) {
            QVERIFY(image.hasAlphaChannel());
        }
    }
}

void tst_ImageDataToImage::generateImages_data()
{
    if (!m_generateImagesEnabled) {
        QTest::addColumn<QString>("psd");
        QTest::newRow("disabled") << QString();
        return;
    }
    addPsdFiles(m_generateImagesSources, m_generateImagesLimit);
}

void tst_ImageDataToImage::generateImages()
{
    if (!m_generateImagesEnabled) {
        QSKIP("Set QTPSD_IMAGE_OUTPUT_PATH to enable Image Data PNG generation.");
    }
    const QString outputDir = m_generateImagesOutputDir;

    QFETCH(QString, psd);

    QPsdParser parser;
    parser.load(psd);

    const auto header = parser.fileHeader();
    const auto colorModeData = parser.colorModeData();
    const auto iccProfile = parser.iccProfile();
    const auto imageData = parser.imageData();

    QImage image;
    if (imageData.width() > 0 && imageData.height() > 0) {
        image = QtPsdGui::imageDataToImage(imageData, header, colorModeData, iccProfile);
    }

    if (image.isNull()) {
        const QSize canvasSize(header.width(), header.height());
        if (!canvasSize.isEmpty()) {
            image = QImage(canvasSize, QImage::Format_ARGB32);
            image.fill(Qt::transparent);
        }
    }

    QVERIFY2(!image.isNull(), qPrintable(QString("Failed to produce Image Data image: %1").arg(psd)));

    if (image.format() != QImage::Format_ARGB32 &&
        image.format() != QImage::Format_RGBA8888 &&
        image.format() != QImage::Format_RGBA64) {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }

    const QList<SourceInfo> sources = allSources();
    const QString source = detectSource(psd, sources);
    const QString sourceId = source.isEmpty() ? "other"_L1 : source;
    const QString relativePsdPath = sourceRelativePath(psd, sourceId, sources);
    const QString subDir = QFileInfo(relativePsdPath).path();
    const QString baseName = QFileInfo(relativePsdPath).baseName();
    const QString dirPart = (subDir.isEmpty() || subDir == ".") ? QString() : (subDir + "/"_L1);

    if (subDir != ".") {
        QDir().mkpath(outputDir + "/images/imagedata/" + sourceId + "/" + subDir);
    } else {
        QDir().mkpath(outputDir + "/images/imagedata/" + sourceId);
    }

    const QString outputPath = outputDir + "/images/imagedata/" + sourceId + "/" + dirPart + baseName + ".png";
    QVERIFY2(image.save(outputPath), qPrintable(QString("Failed to save Image Data image: %1").arg(outputPath)));
}

QTEST_MAIN(tst_ImageDataToImage)
#include "tst_image_data_to_image.moc"

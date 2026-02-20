// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdCore/QPsdFileHeader>
#include <QtPsdCore/QPsdColorModeData>
#include <QtPsdCore/QPsdImageResources>
#include <QtPsdCore/QPsdLayerAndMaskInformation>
#include <QtPsdCore/QPsdLayerInfo>
#include <QtPsdCore/QPsdLayerRecord>
#include <QtPsdCore/QPsdChannelInfo>
#include <QtPsdCore/QPsdChannelImageData>
#include <QtPsdCore/QPsdImageData>
#include <QtPsdCore/QPsdParser>
#include <QtPsdCore/QPsdAdditionalLayerInformationPlugin>
#include <QtPsdWidget/QPsdView>
#include <QtPsdWidget/QPsdWidgetTreeItemModel>
#include <QtPsdWriter/QPsdWriter>
#include <QtTest/QtTest>

#include <QtCore/QBuffer>
#include <QtCore/QDateTime>
#include <QtCore/QDirIterator>
#include <QtCore/QTemporaryFile>
#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <cmath>

class tst_QPsdWriter : public QObject
{
    Q_OBJECT
private slots:
    void writeFileHeader();
    void writeEmptyPsd();
    void writeMinimalPsd();
    void roundTrip_data();
    void roundTrip();
    void roundTripFull_data();
    void roundTripFull();
    void aliCoverage_data();
    void aliCoverage();
    void binaryRoundTrip_data();
    void binaryRoundTrip();
    void visualRoundTrip_data();
    void visualRoundTrip();
    void photoshopSimilarity_data();
    void photoshopSimilarity();
    void writeReport_data();
    void writeReport();
    void cleanupTestCase();

private:
    static QImage renderPsd(const QString &filePath);
    static double compareImages(const QImage &img1, const QImage &img2);

    struct WriteResult {
        QString category;
        QString fileName;
        qint64 fileSize = 0;
        bool pass = false;
        QString failSection;
        qint64 sizeDelta = 0; // written - original (negative = shorter)
        QString details;
    };
    QList<WriteResult> m_results;

    struct VisualResult {
        QString category;
        QString fileName;
        qint64 fileSize = 0;
        double similarity = 0.0;
        bool originalRendered = false;
        bool writtenRendered = false;
        QString error;
    };
    QList<VisualResult> m_visualResults;

    struct PhotoshopResult {
        QString category;
        QString fileName;
        qint64 fileSize = 0;
        double similarity = 0.0;
        bool originalRendered = false;
        bool photoshopAvailable = false;
        bool sizeMismatch = false;
        QSize originalSize;
        QSize photoshopSize;
    };
    QList<PhotoshopResult> m_photoshopResults;
};

void tst_QPsdWriter::writeFileHeader()
{
    QPsdFileHeader header;
    header.setChannels(3);
    header.setHeight(2);
    header.setWidth(2);
    header.setDepth(8);
    header.setColorMode(QPsdFileHeader::RGB);

    QPsdWriter writer;
    writer.setFileHeader(header);

    QBuffer buf;
    buf.open(QIODevice::ReadWrite);
    QVERIFY(writer.write(&buf));

    // File header is always 26 bytes
    buf.seek(0);
    QByteArray data = buf.readAll();
    QVERIFY(data.size() >= 26);

    // Verify signature
    QCOMPARE(data.left(4), QByteArray("8BPS"));

    // Verify version = 1
    QCOMPARE(static_cast<quint8>(data[4]), quint8(0));
    QCOMPARE(static_cast<quint8>(data[5]), quint8(1));

    // Verify reserved bytes are zero
    for (int i = 6; i < 12; ++i)
        QCOMPARE(static_cast<quint8>(data[i]), quint8(0));

    // Verify channels = 3 (big-endian)
    QCOMPARE(static_cast<quint8>(data[12]), quint8(0));
    QCOMPARE(static_cast<quint8>(data[13]), quint8(3));

    // Verify height = 2
    QCOMPARE(static_cast<quint8>(data[16]), quint8(0));
    QCOMPARE(static_cast<quint8>(data[17]), quint8(2));

    // Verify width = 2
    QCOMPARE(static_cast<quint8>(data[20]), quint8(0));
    QCOMPARE(static_cast<quint8>(data[21]), quint8(2));

    // Verify depth = 8
    QCOMPARE(static_cast<quint8>(data[22]), quint8(0));
    QCOMPARE(static_cast<quint8>(data[23]), quint8(8));

    // Verify color mode = RGB (3)
    QCOMPARE(static_cast<quint8>(data[24]), quint8(0));
    QCOMPARE(static_cast<quint8>(data[25]), quint8(3));
}

void tst_QPsdWriter::writeEmptyPsd()
{
    // Create a minimal PSD with no layers and empty sections
    QPsdFileHeader header;
    header.setChannels(3);
    header.setHeight(1);
    header.setWidth(1);
    header.setDepth(8);
    header.setColorMode(QPsdFileHeader::RGB);

    QPsdWriter writer;
    writer.setFileHeader(header);

    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    QVERIFY(writer.write(tmpPath));

    // Re-parse the written file
    QPsdParser parser;
    parser.load(tmpPath);

    QCOMPARE(parser.fileHeader().channels(), quint16(3));
    QCOMPARE(parser.fileHeader().height(), quint32(1));
    QCOMPARE(parser.fileHeader().width(), quint32(1));
    QCOMPARE(parser.fileHeader().depth(), quint16(8));
    QCOMPARE(parser.fileHeader().colorMode(), QPsdFileHeader::RGB);
}

void tst_QPsdWriter::writeMinimalPsd()
{
    // Create a 2x2 RGB PSD with composite image data
    QPsdFileHeader header;
    header.setChannels(3);
    header.setHeight(2);
    header.setWidth(2);
    header.setDepth(8);
    header.setColorMode(QPsdFileHeader::RGB);

    // Composite image data: 3 channels * 4 pixels = 12 bytes
    // All red (R=255, G=0, B=0)
    QByteArray compositeData;
    compositeData.append(QByteArray(4, '\xff'));  // R channel
    compositeData.append(QByteArray(4, '\x00'));  // G channel
    compositeData.append(QByteArray(4, '\x00'));  // B channel

    QPsdImageData imageData;
    imageData.setWidth(2);
    imageData.setHeight(2);
    imageData.setOpacity(255);
    imageData.setImageData(compositeData);

    QPsdWriter writer;
    writer.setFileHeader(header);
    writer.setImageData(imageData);

    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    QVERIFY(writer.write(tmpPath));

    // Re-parse and verify
    QPsdParser parser;
    parser.load(tmpPath);

    QCOMPARE(parser.fileHeader().channels(), quint16(3));
    QCOMPARE(parser.fileHeader().height(), quint32(2));
    QCOMPARE(parser.fileHeader().width(), quint32(2));
    QCOMPARE(parser.fileHeader().depth(), quint16(8));
    QCOMPARE(parser.fileHeader().colorMode(), QPsdFileHeader::RGB);

    // Verify image data was preserved
    QCOMPARE(parser.imageData().imageData(), compositeData);
}

void tst_QPsdWriter::roundTrip_data()
{
    QTest::addColumn<QString>("psd");

    // Use the layer tree item model test data which has known small PSD files
    QDir dataDir(QFINDTESTDATA("../../psdcore/qpsdlayertreeitemmodel/data/"));
    if (dataDir.exists()) {
        for (const QString &fileName : dataDir.entryList(QStringList() << "*.psd"_L1)) {
            QTest::newRow(fileName.toLatin1().data()) << dataDir.filePath(fileName);
        }
    }
}

void tst_QPsdWriter::roundTrip()
{
    QFETCH(QString, psd);

    // Load the original PSD
    QPsdParser original;
    original.load(psd);

    // Build a writer with all sections including layer and mask information
    QPsdWriter writer;
    writer.setFileHeader(original.fileHeader());
    writer.setColorModeData(original.colorModeData());
    writer.setImageResources(original.imageResources());
    writer.setLayerAndMaskInformation(original.layerAndMaskInformation());
    writer.setImageData(original.imageData());

    // Write to a temporary file
    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    QVERIFY2(writer.write(tmpPath), qPrintable(writer.errorString()));

    // Full re-parse to verify structural integrity (no signature mismatch)
    QPsdParser reparsed;
    reparsed.load(tmpPath);

    // Verify header fields match
    QCOMPARE(reparsed.fileHeader().channels(), original.fileHeader().channels());
    QCOMPARE(reparsed.fileHeader().height(), original.fileHeader().height());
    QCOMPARE(reparsed.fileHeader().width(), original.fileHeader().width());
    QCOMPARE(reparsed.fileHeader().depth(), original.fileHeader().depth());
    QCOMPARE(reparsed.fileHeader().colorMode(), original.fileHeader().colorMode());

    // Verify layer count matches
    const auto &originalRecords = original.layerAndMaskInformation().layerInfo().records();
    const auto &reparsedRecords = reparsed.layerAndMaskInformation().layerInfo().records();
    QCOMPARE(reparsedRecords.size(), originalRecords.size());
}

void tst_QPsdWriter::roundTripFull_data()
{
    QTest::addColumn<QString>("psd");

    const QString zooPath = QFINDTESTDATA("../../3rdparty/psd-zoo/");
    if (zooPath.isEmpty() || !QDir(zooPath).exists()) {
        QSKIP("psd-zoo submodule not available");
        return;
    }
    QDirIterator it(zooPath, QStringList() << "*.psd"_L1, QDir::Files, QDirIterator::Subdirectories);
    QList<QPair<QString, QString>> rows;
    while (it.hasNext()) {
        const QString filePath = it.next();
        const QString relativePath = QDir(zooPath).relativeFilePath(filePath);
        rows.append({relativePath, filePath});
    }
    std::sort(rows.begin(), rows.end());
    if (rows.isEmpty()) {
        QSKIP("No PSD files found in psd-zoo");
        return;
    }
    for (const auto &row : rows)
        QTest::newRow(row.first.toLatin1().data()) << row.second;
}

void tst_QPsdWriter::roundTripFull()
{
    QFETCH(QString, psd);

    // Parse the original PSD
    QPsdParser original;
    original.load(psd);

    // Build writer with ALL sections
    QPsdWriter writer;
    writer.setFileHeader(original.fileHeader());
    writer.setColorModeData(original.colorModeData());
    writer.setImageResources(original.imageResources());
    writer.setLayerAndMaskInformation(original.layerAndMaskInformation());
    writer.setImageData(original.imageData());

    // Write to a temporary file
    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    QVERIFY2(writer.write(tmpPath), qPrintable(writer.errorString()));

    // Re-parse the written file
    QPsdParser reparsed;
    reparsed.load(tmpPath);

    // Compare header fields
    QCOMPARE(reparsed.fileHeader().channels(), original.fileHeader().channels());
    QCOMPARE(reparsed.fileHeader().height(), original.fileHeader().height());
    QCOMPARE(reparsed.fileHeader().width(), original.fileHeader().width());
    QCOMPARE(reparsed.fileHeader().depth(), original.fileHeader().depth());
    QCOMPARE(reparsed.fileHeader().colorMode(), original.fileHeader().colorMode());

    // Compare color mode data
    QCOMPARE(reparsed.colorModeData().colorData(), original.colorModeData().colorData());

    // Compare image resource block count
    QCOMPARE(reparsed.imageResources().imageResourceBlocks().size(),
             original.imageResources().imageResourceBlocks().size());

    // Compare layer count
    const auto &originalRecords = original.layerAndMaskInformation().layerInfo().records();
    const auto &reparsedRecords = reparsed.layerAndMaskInformation().layerInfo().records();
    QCOMPARE(reparsedRecords.size(), originalRecords.size());

    // Compare per-layer properties
    for (qsizetype i = 0; i < originalRecords.size(); ++i) {
        QCOMPARE(reparsedRecords.at(i).name(), originalRecords.at(i).name());
        QCOMPARE(reparsedRecords.at(i).blendMode(), originalRecords.at(i).blendMode());
        QCOMPARE(reparsedRecords.at(i).opacity(), originalRecords.at(i).opacity());
        QCOMPARE(reparsedRecords.at(i).channelInfo().size(),
                 originalRecords.at(i).channelInfo().size());
    }
}

void tst_QPsdWriter::aliCoverage_data()
{
    QTest::addColumn<QString>("psd");

    const QString zooPath = QFINDTESTDATA("../../3rdparty/psd-zoo/");
    if (zooPath.isEmpty() || !QDir(zooPath).exists()) {
        QSKIP("psd-zoo submodule not available");
        return;
    }
    QDirIterator it(zooPath, QStringList() << "*.psd"_L1, QDir::Files, QDirIterator::Subdirectories);
    QList<QPair<QString, QString>> rows;
    while (it.hasNext()) {
        const QString filePath = it.next();
        const QString relativePath = QDir(zooPath).relativeFilePath(filePath);
        rows.append({relativePath, filePath});
    }
    std::sort(rows.begin(), rows.end());
    if (rows.isEmpty()) {
        QSKIP("No PSD files found in psd-zoo");
        return;
    }
    for (const auto &row : rows)
        QTest::newRow(row.first.toLatin1().data()) << row.second;
}

void tst_QPsdWriter::aliCoverage()
{
    QFETCH(QString, psd);

    QPsdParser parser;
    parser.load(psd);

    int totalEntries = 0;
    int rawByteArray = 0;
    int serializable = 0;
    int notSerializable = 0;
    QMap<QByteArray, int> failedKeys;

    // Helper lambda to process an ALI hash
    const auto processAli = [&](const QHash<QByteArray, QVariant> &ali) {
        for (auto it = ali.cbegin(); it != ali.cend(); ++it) {
            ++totalEntries;
            const QByteArray &key = it.key();
            const QVariant &value = it.value();

            if (value.typeId() == QMetaType::QByteArray) {
                ++rawByteArray;
            } else {
                auto *plugin = QPsdAdditionalLayerInformationPlugin::plugin(key);
                if (plugin) {
                    const QByteArray payload = plugin->serialize(value);
                    if (!payload.isEmpty()) {
                        ++serializable;
                    } else {
                        ++notSerializable;
                        failedKeys[key]++;
                    }
                } else {
                    ++notSerializable;
                    failedKeys[key]++;
                }
            }
        }
    };

    // Per-layer ALI
    const auto &records = parser.layerAndMaskInformation().layerInfo().records();
    for (const auto &record : records)
        processAli(record.additionalLayerInformation());

    // Top-level ALI
    processAli(parser.layerAndMaskInformation().additionalLayerInformation());

    const int totalSerializable = rawByteArray + serializable;
    const double pct = totalEntries > 0
        ? 100.0 * totalSerializable / totalEntries
        : 100.0;

    qDebug() << QFileInfo(psd).fileName()
             << "total:" << totalEntries
             << "raw:" << rawByteArray
             << "serialized:" << serializable
             << "failed:" << notSerializable
             << QString("(%1%)"_L1).arg(pct, 0, 'f', 1);

    if (!failedKeys.isEmpty()) {
        QStringList failList;
        for (auto it = failedKeys.cbegin(); it != failedKeys.cend(); ++it)
            failList << "%1(%2)"_L1.arg(QString::fromLatin1(it.key())).arg(it.value());
        qDebug() << "  failed keys:" << failList.join(", "_L1);
    }
}

QImage tst_QPsdWriter::renderPsd(const QString &filePath)
{
    QPsdParser parser;
    parser.load(filePath);

    QPsdWidgetTreeItemModel model;
    model.fromParser(parser);

    QPsdView view;
    const QSize canvasSize = model.size();
    if (canvasSize.isEmpty())
        return QImage();

    view.setAttribute(Qt::WA_TranslucentBackground);
    view.resize(canvasSize.grownBy({1, 1, 1, 1}));
    view.viewport()->resize(canvasSize);
    view.setModel(&model);

    QImage rendered(canvasSize, QImage::Format_ARGB32);
    rendered.fill(Qt::transparent);

    QPainter painter(&rendered);
    view.render(&painter);

    return rendered;
}

double tst_QPsdWriter::compareImages(const QImage &img1, const QImage &img2)
{
    if (img1.size() != img2.size() || img1.isNull() || img2.isNull())
        return 0.0;

    const QImage a = img1.convertToFormat(QImage::Format_ARGB32);
    const QImage b = img2.convertToFormat(QImage::Format_ARGB32);
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

            if (cr1 >= 250 && cg1 >= 250 && cb1 >= 250
                && cr2 >= 250 && cg2 >= 250 && cb2 >= 250) {
                continue;
            }

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

    const double contentBased = 100.0 * (1.0 - static_cast<double>(pixelsDifferent) / static_cast<double>(pixelsWithContent));
    const double hsvBased = 100.0 * (1.0 - totalDiff / static_cast<double>(pixelsWithContent));
    return qMin(contentBased, hsvBased);
}

void tst_QPsdWriter::visualRoundTrip_data()
{
    QTest::addColumn<QString>("psd");
    QTest::addColumn<qint64>("fileSize");

    const QString zooPath = QFINDTESTDATA("../../3rdparty/psd-zoo/");
    if (zooPath.isEmpty() || !QDir(zooPath).exists()) {
        QSKIP("psd-zoo submodule not available");
        return;
    }
    QDirIterator it(zooPath, QStringList() << "*.psd"_L1, QDir::Files, QDirIterator::Subdirectories);
    QList<std::tuple<qint64, QString, QString>> rows;
    while (it.hasNext()) {
        const QString filePath = it.next();
        const QString relativePath = QDir(zooPath).relativeFilePath(filePath);
        const qint64 size = QFileInfo(filePath).size();
        rows.append({size, relativePath, filePath});
    }
    std::sort(rows.begin(), rows.end());
    if (rows.isEmpty()) {
        QSKIP("No PSD files found in psd-zoo");
        return;
    }
    for (const auto &[size, rel, path] : rows)
        QTest::newRow(rel.toLatin1().data()) << path << size;
}

void tst_QPsdWriter::visualRoundTrip()
{
    QFETCH(QString, psd);
    QFETCH(qint64, fileSize);

    const QString zooPath = QFINDTESTDATA("../../3rdparty/psd-zoo/");
    const QString relativePath = QDir(zooPath).relativeFilePath(psd);
    const int slashIdx = relativePath.indexOf(u'/');
    const QString category = (slashIdx > 0) ? relativePath.left(slashIdx) : u"other"_s;
    const QString fileName = (slashIdx > 0) ? relativePath.mid(slashIdx + 1) : relativePath;

    VisualResult result;
    result.category = category;
    result.fileName = fileName;
    result.fileSize = fileSize;

    // 1. Render original PSD
    const QImage originalImage = renderPsd(psd);
    result.originalRendered = !originalImage.isNull();
    if (originalImage.isNull()) {
        result.error = u"Failed to render original PSD"_s;
        m_visualResults.append(result);
        return; // Not a test failure — some files have empty canvas
    }

    // 2. Parse and write without raw bytes
    QPsdParser parser;
    parser.load(psd);

    QPsdWriter writer;
    writer.setFileHeader(parser.fileHeader());
    writer.setColorModeData(parser.colorModeData());
    writer.setImageResources(parser.imageResources());
    writer.setLayerAndMaskInformation(parser.layerAndMaskInformation());
    writer.setImageData(parser.imageData());
    writer.setUseRawBytes(false);

    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    const QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    if (!writer.write(tmpPath)) {
        result.error = writer.errorString();
        m_visualResults.append(result);
        QFAIL(qPrintable(u"Write failed: %1"_s.arg(writer.errorString())));
        return;
    }

    // 3. Render written PSD
    const QImage writtenImage = renderPsd(tmpPath);
    result.writtenRendered = !writtenImage.isNull();
    if (writtenImage.isNull()) {
        result.error = u"Failed to render written PSD"_s;
        m_visualResults.append(result);
        QFAIL("Failed to render written PSD");
        return;
    }

    // 4. Compare
    if (originalImage.size() != writtenImage.size()) {
        result.error = u"Size mismatch: %1x%2 vs %3x%4"_s
            .arg(originalImage.width()).arg(originalImage.height())
            .arg(writtenImage.width()).arg(writtenImage.height());
        m_visualResults.append(result);
        QFAIL(qPrintable(result.error));
        return;
    }

    result.similarity = compareImages(originalImage, writtenImage);
    m_visualResults.append(result);

    QVERIFY2(result.similarity >= 90.0,
             qPrintable(u"Visual similarity %.1f%% < 90%% for %1"_s
                        .arg(result.similarity).arg(fileName)));
}

void tst_QPsdWriter::photoshopSimilarity_data()
{
    QTest::addColumn<QString>("psd");
    QTest::addColumn<qint64>("fileSize");

    const QString zooPath = QFINDTESTDATA("../../3rdparty/psd-zoo/");
    if (zooPath.isEmpty() || !QDir(zooPath).exists()) {
        QSKIP("psd-zoo submodule not available");
        return;
    }
    QDirIterator it(zooPath, QStringList() << "*.psd"_L1, QDir::Files, QDirIterator::Subdirectories);
    QList<std::tuple<qint64, QString, QString>> rows;
    while (it.hasNext()) {
        const QString filePath = it.next();
        const QString relativePath = QDir(zooPath).relativeFilePath(filePath);
        const qint64 size = QFileInfo(filePath).size();
        rows.append({size, relativePath, filePath});
    }
    std::sort(rows.begin(), rows.end());
    if (rows.isEmpty()) {
        QSKIP("No PSD files found in psd-zoo");
        return;
    }
    for (const auto &[size, rel, path] : rows)
        QTest::newRow(rel.toLatin1().data()) << path << size;
}

void tst_QPsdWriter::photoshopSimilarity()
{
    QFETCH(QString, psd);
    QFETCH(qint64, fileSize);

    const QString zooPath = QFINDTESTDATA("../../3rdparty/psd-zoo/");
    const QString relativePath = QDir(zooPath).relativeFilePath(psd);
    const int slashIdx = relativePath.indexOf(u'/');
    const QString category = (slashIdx > 0) ? relativePath.left(slashIdx) : u"other"_s;
    const QString fileName = (slashIdx > 0) ? relativePath.mid(slashIdx + 1) : relativePath;

    PhotoshopResult result;
    result.category = category;
    result.fileName = fileName;
    result.fileSize = fileSize;

    // 1. Render original PSD via QPsdView
    const QImage originalImage = renderPsd(psd);
    result.originalRendered = !originalImage.isNull();
    if (originalImage.isNull()) {
        m_photoshopResults.append(result);
        return; // Not a test failure — some files have empty canvas
    }
    result.originalSize = originalImage.size();

    // 2. Load Photoshop-rendered PNG from round-trip/<category>/<basename>.png
    const QString baseName = QFileInfo(psd).completeBaseName();
    const QString roundTripDir = QStringLiteral(QT_TESTCASE_SOURCEDIR "/round-trip");
    const QString photoshopPng = roundTripDir + u'/' + category + u'/' + baseName + u".png"_s;

    if (!QFile::exists(photoshopPng)) {
        m_photoshopResults.append(result);
        return; // No Photoshop screenshot available
    }

    QImage photoshopImage(photoshopPng);
    result.photoshopAvailable = !photoshopImage.isNull();
    if (photoshopImage.isNull()) {
        m_photoshopResults.append(result);
        return;
    }
    result.photoshopSize = photoshopImage.size();

    // 3. Handle size mismatch by scaling Photoshop image to match original
    if (photoshopImage.size() != originalImage.size()) {
        result.sizeMismatch = true;
        photoshopImage = photoshopImage.scaled(originalImage.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    // 4. Compare
    result.similarity = compareImages(originalImage, photoshopImage);
    m_photoshopResults.append(result);
}

void tst_QPsdWriter::binaryRoundTrip_data()
{
    QTest::addColumn<QString>("psd");
    QTest::addColumn<qint64>("fileSize");

    const QString zooPath = QFINDTESTDATA("../../3rdparty/psd-zoo/");
    if (zooPath.isEmpty() || !QDir(zooPath).exists()) {
        QSKIP("psd-zoo submodule not available");
        return;
    }
    QDirIterator it(zooPath, QStringList() << "*.psd"_L1, QDir::Files, QDirIterator::Subdirectories);
    QList<std::tuple<qint64, QString, QString>> rows;
    while (it.hasNext()) {
        const QString filePath = it.next();
        const QString relativePath = QDir(zooPath).relativeFilePath(filePath);
        const qint64 size = QFileInfo(filePath).size();
        rows.append({size, relativePath, filePath});
    }
    // Sort by file size ascending
    std::sort(rows.begin(), rows.end());
    if (rows.isEmpty()) {
        QSKIP("No PSD files found in psd-zoo");
        return;
    }
    for (const auto &[size, rel, path] : rows)
        QTest::newRow(rel.toLatin1().data()) << path << size;
}

void tst_QPsdWriter::binaryRoundTrip()
{
    QFETCH(QString, psd);
    QFETCH(qint64, fileSize);
    Q_UNUSED(fileSize);

    // Read original bytes
    QFile originalFile(psd);
    QVERIFY(originalFile.open(QIODevice::ReadOnly));
    const QByteArray originalBytes = originalFile.readAll();
    originalFile.close();

    // Parse
    QPsdParser parser;
    parser.load(psd);

    // Write through QPsdWriter
    QPsdWriter writer;
    writer.setFileHeader(parser.fileHeader());
    writer.setColorModeData(parser.colorModeData());
    writer.setImageResources(parser.imageResources());
    writer.setLayerAndMaskInformation(parser.layerAndMaskInformation());
    writer.setImageData(parser.imageData());

    // Save to round-trip/ directory in repo, preserving category structure
    const QString zooPath = QFINDTESTDATA("../../3rdparty/psd-zoo/");
    const QString relativePath = QDir(zooPath).relativeFilePath(psd);
    const QString roundTripDir = QStringLiteral(QT_TESTCASE_SOURCEDIR "/round-trip");
    const QString outPath = roundTripDir + u'/' + relativePath;
    QDir().mkpath(QFileInfo(outPath).absolutePath());

    QVERIFY2(writer.write(outPath), qPrintable(writer.errorString()));

    // Read written bytes
    QFile writtenFile(outPath);
    QVERIFY(writtenFile.open(QIODevice::ReadOnly));
    const QByteArray writtenBytes = writtenFile.readAll();
    writtenFile.close();

    // Compare — binary round-trip must be identical
    QCOMPARE(writtenBytes, originalBytes);
}

void tst_QPsdWriter::writeReport_data()
{
    QTest::addColumn<QString>("psd");
    QTest::addColumn<qint64>("fileSize");

    const QString zooPath = QFINDTESTDATA("../../3rdparty/psd-zoo/");
    if (zooPath.isEmpty() || !QDir(zooPath).exists()) {
        QSKIP("psd-zoo submodule not available");
        return;
    }
    QDirIterator it(zooPath, QStringList() << "*.psd"_L1, QDir::Files, QDirIterator::Subdirectories);
    QList<std::tuple<qint64, QString, QString>> rows;
    while (it.hasNext()) {
        const QString filePath = it.next();
        const QString relativePath = QDir(zooPath).relativeFilePath(filePath);
        const qint64 size = QFileInfo(filePath).size();
        rows.append({size, relativePath, filePath});
    }
    std::sort(rows.begin(), rows.end());
    if (rows.isEmpty()) {
        QSKIP("No PSD files found in psd-zoo");
        return;
    }
    for (const auto &[size, rel, path] : rows)
        QTest::newRow(rel.toLatin1().data()) << path << size;
}

// Determine which PSD section a byte offset falls in
static QString sectionForOffset(qint64 offset, qint64 headerEnd, qint64 colorEnd,
                                qint64 irEnd, qint64 lmiEnd)
{
    if (offset < headerEnd) return u"FileHeader"_s;
    if (offset < colorEnd) return u"ColorMode"_s;
    if (offset < irEnd) return u"ImageResources"_s;
    if (offset < lmiEnd) return u"LMI"_s;
    return u"ImageData"_s;
}

void tst_QPsdWriter::writeReport()
{
    QFETCH(QString, psd);
    QFETCH(qint64, fileSize);

    const QString zooPath = QFINDTESTDATA("../../3rdparty/psd-zoo/");
    const QString relativePath = QDir(zooPath).relativeFilePath(psd);

    // Extract category (first path component)
    const int slashIdx = relativePath.indexOf(u'/');
    const QString category = (slashIdx > 0) ? relativePath.left(slashIdx) : u"other"_s;
    const QString fileName = (slashIdx > 0) ? relativePath.mid(slashIdx + 1) : relativePath;

    // Read original bytes
    QFile originalFile(psd);
    QVERIFY(originalFile.open(QIODevice::ReadOnly));
    const QByteArray originalBytes = originalFile.readAll();
    originalFile.close();

    // Parse to determine section boundaries
    const qint64 headerEnd = 26;
    auto readU32 = [&](const QByteArray &data, qint64 offset) -> quint32 {
        return qFromBigEndian<quint32>(data.constData() + offset);
    };
    const quint32 colorLen = readU32(originalBytes, headerEnd);
    const qint64 colorEnd = headerEnd + 4 + colorLen;
    const quint32 irLen = readU32(originalBytes, colorEnd);
    const qint64 irEnd = colorEnd + 4 + irLen;
    const quint32 lmiLen = readU32(originalBytes, irEnd);
    const qint64 lmiEnd = irEnd + 4 + lmiLen;

    // Parse and write
    QPsdParser parser;
    parser.load(psd);

    QPsdWriter writer;
    writer.setFileHeader(parser.fileHeader());
    writer.setColorModeData(parser.colorModeData());
    writer.setImageResources(parser.imageResources());
    writer.setLayerAndMaskInformation(parser.layerAndMaskInformation());
    writer.setImageData(parser.imageData());

    QBuffer outBuf;
    outBuf.open(QIODevice::WriteOnly);
    const bool writeOk = writer.write(&outBuf);
    outBuf.close();

    WriteResult result;
    result.category = category;
    result.fileName = fileName;
    result.fileSize = fileSize;

    if (!writeOk) {
        result.pass = false;
        result.failSection = u"WRITE_ERROR"_s;
        result.details = writer.errorString();
        m_results.append(result);
        return;
    }

    const QByteArray writtenBytes = outBuf.data();

    if (writtenBytes == originalBytes) {
        result.pass = true;
        m_results.append(result);
        return;
    }

    result.pass = false;
    result.sizeDelta = writtenBytes.size() - originalBytes.size();

    // Find first diff
    qint64 firstDiff = -1;
    const qint64 minLen = qMin<qint64>(writtenBytes.size(), originalBytes.size());
    for (qint64 i = 0; i < minLen; ++i) {
        if (writtenBytes.at(i) != originalBytes.at(i)) {
            firstDiff = i;
            break;
        }
    }
    if (firstDiff < 0 && writtenBytes.size() != originalBytes.size())
        firstDiff = minLen;

    result.failSection = sectionForOffset(firstDiff, headerEnd, colorEnd, irEnd, lmiEnd);

    if (result.failSection == u"LMI"_s) {
        // Detailed LMI analysis: compare sub-section sizes
        const qint64 lmiStart = irEnd + 4;
        const quint32 origLiLen = readU32(originalBytes, lmiStart);
        qint64 origLiEnd = lmiStart + 4 + origLiLen;
        if (origLiLen % 2 != 0) origLiEnd++;

        // Also read written LMI sub-structure
        const quint32 writtenLmiLen = readU32(writtenBytes, irEnd);
        const quint32 writtenLiLen = readU32(writtenBytes, lmiStart);
        qint64 writtenLiEnd = lmiStart + 4 + writtenLiLen;
        if (writtenLiLen % 2 != 0) writtenLiEnd++;

        QStringList parts;
        if (origLiLen != writtenLiLen) {
            qint64 liDelta = static_cast<qint64>(writtenLiLen) - static_cast<qint64>(origLiLen);
            parts << u"LayerInfo %1%2"_s.arg(liDelta >= 0 ? u"+"_s : u""_s).arg(liDelta);
        }

        // GLMI
        if (origLiEnd + 4 <= originalBytes.size() && writtenLiEnd + 4 <= writtenBytes.size()) {
            const quint32 origGlmiLen = readU32(originalBytes, origLiEnd);
            const quint32 writtenGlmiLen = readU32(writtenBytes, writtenLiEnd);
            if (origGlmiLen != writtenGlmiLen) {
                qint64 glmiDelta = static_cast<qint64>(writtenGlmiLen) - static_cast<qint64>(origGlmiLen);
                parts << u"GLMI %1%2"_s.arg(glmiDelta >= 0 ? u"+"_s : u""_s).arg(glmiDelta);
            }

            // Top-level ALI size
            qint64 origAliStart = origLiEnd + 4 + origGlmiLen;
            qint64 writtenAliStart = writtenLiEnd + 4 + writtenGlmiLen;
            qint64 origAliSize = (irEnd + 4 + lmiLen) - origAliStart;
            qint64 writtenAliSize = (irEnd + 4 + writtenLmiLen) - writtenAliStart;
            if (origAliSize != writtenAliSize) {
                qint64 aliDelta = writtenAliSize - origAliSize;
                parts << u"TopALI %1%2"_s.arg(aliDelta >= 0 ? u"+"_s : u""_s).arg(aliDelta);
            }
        }

        result.details = parts.isEmpty() ? u"byte diff"_s : parts.join(u", "_s);
    } else {
        result.details = u"byte diff at %1"_s.arg(firstDiff);
    }

    m_results.append(result);
}

void tst_QPsdWriter::cleanupTestCase()
{
    // === Binary round-trip report ===
    if (!m_results.isEmpty()) {
        const QString reportPath = QStringLiteral(QT_TESTCASE_SOURCEDIR "/../../../../docs/psd-zoo-write-report.md");

        QFile reportFile(reportPath);
        if (reportFile.open(QFile::WriteOnly | QFile::Truncate)) {
            QTextStream ts(&reportFile);

            // Count totals
            int totalPass = 0;
            int totalFail = 0;
            QMap<QString, int> categoryPass;
            QMap<QString, int> categoryTotal;
            QMap<QString, int> sectionCounts;

            for (const auto &r : m_results) {
                categoryTotal[r.category]++;
                if (r.pass) {
                    totalPass++;
                    categoryPass[r.category]++;
                } else {
                    totalFail++;
                    sectionCounts[r.failSection]++;
                }
            }

            ts << "# PSD Writer Round-Trip Report\n\n";
            ts << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";

            // === Summary Statistics ===
            ts << "## Summary Statistics\n\n";
            ts << "| Metric | Value |\n";
            ts << "|--------|-------|\n";
            ts << "| Total Tests | " << m_results.size() << " |\n";
            const double passRate = m_results.isEmpty() ? 0.0
                : 100.0 * totalPass / m_results.size();
            ts << "| Passed | " << totalPass << " (" << QString::number(passRate, 'f', 1) << "%) |\n";
            ts << "| Failed | " << totalFail << " |\n\n";

            // === Per-Category Summary ===
            ts << "## Per-Category Summary\n\n";
            ts << "| Category | Pass | Total | Rate |\n";
            ts << "|----------|------|-------|------|\n";
            for (auto it = categoryTotal.cbegin(); it != categoryTotal.cend(); ++it) {
                const int pass = categoryPass.value(it.key(), 0);
                const int total = it.value();
                const double rate = total > 0 ? 100.0 * pass / total : 0.0;
                ts << "| " << it.key() << " | " << pass << " | " << total
                   << " | " << QString::number(rate, 'f', 1) << "% |\n";
            }
            ts << "\n";

            // === Per-Section Failure Breakdown ===
            if (!sectionCounts.isEmpty()) {
                ts << "## Failure Breakdown by Section\n\n";
                ts << "| Section | Count |\n";
                ts << "|---------|-------|\n";
                for (auto it = sectionCounts.cbegin(); it != sectionCounts.cend(); ++it)
                    ts << "| " << it.key() << " | " << it.value() << " |\n";
                ts << "\n";
            }

            // === Per-Category Detailed Tables ===
            QList<WriteResult> sorted = m_results;
            std::sort(sorted.begin(), sorted.end(), [](const WriteResult &a, const WriteResult &b) {
                if (a.category != b.category) return a.category < b.category;
                return a.fileName < b.fileName;
            });

            QString currentCategory;
            for (const auto &r : sorted) {
                if (r.category != currentCategory) {
                    currentCategory = r.category;
                    ts << "## " << currentCategory << "\n\n";
                    ts << "| File | Size | Result | Delta | Details |\n";
                    ts << "|------|-----:|:------:|------:|:--------|\n";
                }
                const QString psdLink = u"[%1](https://github.com/signal-slot/psd-zoo/blob/master/%2/%1)"_s
                    .arg(r.fileName, r.category);
                if (r.pass) {
                    ts << "| " << psdLink << " | " << r.fileSize << " | PASS | - | - |\n";
                } else {
                    const QString delta = (r.sizeDelta == 0) ? u"0"_s
                        : ((r.sizeDelta > 0) ? u"+%1"_s.arg(r.sizeDelta) : QString::number(r.sizeDelta));
                    ts << "| " << psdLink << " | " << r.fileSize
                       << " | FAIL | " << delta << " | " << r.details << " |\n";
                }
            }

            reportFile.close();
            qDebug() << "Write report:" << reportPath;
            qDebug() << "Pass:" << totalPass << "Fail:" << totalFail;
        }
    }

    // === Visual round-trip report ===
    if (!m_visualResults.isEmpty()) {
        const QString visualReportPath = QStringLiteral(QT_TESTCASE_SOURCEDIR "/../../../../docs/psd-zoo-visual-report.md");

        QFile visualFile(visualReportPath);
        if (visualFile.open(QFile::WriteOnly | QFile::Truncate)) {
            QTextStream vs(&visualFile);

            // Count totals
            int totalRendered = 0;
            int totalPassed = 0;
            int totalFailed = 0;
            int totalSkipped = 0;
            double sumSimilarity = 0.0;
            QMap<QString, int> catRendered;
            QMap<QString, int> catPassed;
            QMap<QString, double> catSumSim;

            for (const auto &r : m_visualResults) {
                if (!r.originalRendered) {
                    ++totalSkipped;
                    continue;
                }
                ++totalRendered;
                catRendered[r.category]++;
                if (r.writtenRendered && r.error.isEmpty()) {
                    sumSimilarity += r.similarity;
                    catSumSim[r.category] += r.similarity;
                    if (r.similarity >= 90.0) {
                        ++totalPassed;
                        catPassed[r.category]++;
                    } else {
                        ++totalFailed;
                    }
                } else {
                    ++totalFailed;
                }
            }

            vs << "# PSD Writer Visual Round-Trip Report\n\n";
            vs << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
            vs << "Reconstruction mode: `useRawBytes=false` (all data written from parsed fields)\n\n";

            // === Summary ===
            vs << "## Summary Statistics\n\n";
            vs << "| Metric | Value |\n";
            vs << "|--------|-------|\n";
            vs << "| Total Files | " << m_visualResults.size() << " |\n";
            vs << "| Rendered | " << totalRendered << " |\n";
            vs << "| Skipped (empty canvas) | " << totalSkipped << " |\n";
            const double passRate = totalRendered > 0 ? 100.0 * totalPassed / totalRendered : 0.0;
            vs << "| Passed (>=90%) | " << totalPassed << " (" << QString::number(passRate, 'f', 1) << "%) |\n";
            vs << "| Failed (<90%) | " << totalFailed << " |\n";
            const double avgSim = totalRendered > 0 ? sumSimilarity / totalRendered : 0.0;
            vs << "| Average Similarity | " << QString::number(avgSim, 'f', 1) << "% |\n\n";

            // === Per-Category Summary ===
            vs << "## Per-Category Summary\n\n";
            vs << "| Category | Pass | Total | Rate | Avg Similarity |\n";
            vs << "|----------|------|-------|------|----------------|\n";
            for (auto it = catRendered.cbegin(); it != catRendered.cend(); ++it) {
                const int pass = catPassed.value(it.key(), 0);
                const int total = it.value();
                const double rate = total > 0 ? 100.0 * pass / total : 0.0;
                const double avg = total > 0 ? catSumSim.value(it.key(), 0.0) / total : 0.0;
                vs << "| " << it.key() << " | " << pass << " | " << total
                   << " | " << QString::number(rate, 'f', 1) << "%"
                   << " | " << QString::number(avg, 'f', 1) << "% |\n";
            }
            vs << "\n";

            // === Per-Category Detailed Tables ===
            QList<VisualResult> sorted = m_visualResults;
            std::sort(sorted.begin(), sorted.end(), [](const VisualResult &a, const VisualResult &b) {
                if (a.category != b.category) return a.category < b.category;
                return a.fileName < b.fileName;
            });

            QString currentCategory;
            for (const auto &r : sorted) {
                if (r.category != currentCategory) {
                    currentCategory = r.category;
                    vs << "## " << currentCategory << "\n\n";
                    vs << "| File | Size | Similarity | Result | Details |\n";
                    vs << "|------|-----:|----------:|:------:|:--------|\n";
                }
                const QString psdLink = u"[%1](https://github.com/signal-slot/psd-zoo/blob/master/%2/%1)"_s
                    .arg(r.fileName, r.category);
                if (!r.originalRendered) {
                    vs << "| " << psdLink << " | " << r.fileSize << " | - | SKIP | empty canvas |\n";
                } else if (!r.error.isEmpty()) {
                    vs << "| " << psdLink << " | " << r.fileSize << " | - | FAIL | " << r.error << " |\n";
                } else {
                    const QString status = r.similarity >= 90.0 ? u"PASS"_s : u"FAIL"_s;
                    vs << "| " << psdLink << " | " << r.fileSize
                       << " | " << QString::number(r.similarity, 'f', 1) << "%"
                       << " | " << status << " | - |\n";
                }
            }

            visualFile.close();
            qDebug() << "Visual report:" << visualReportPath;
            qDebug() << "Pass:" << totalPassed << "Fail:" << totalFailed << "Skip:" << totalSkipped;
        }
    }

    // === Photoshop similarity report ===
    if (!m_photoshopResults.isEmpty()) {
        const QString photoshopReportPath = QStringLiteral(QT_TESTCASE_SOURCEDIR "/../../../../docs/psd-zoo-photoshop-report.md");

        QFile photoshopFile(photoshopReportPath);
        if (photoshopFile.open(QFile::WriteOnly | QFile::Truncate)) {
            QTextStream ps(&photoshopFile);

            // Count totals
            int totalCompared = 0;
            int totalPassed = 0;
            int totalFailed = 0;
            int totalSkipped = 0;
            double sumSimilarity = 0.0;
            QMap<QString, int> catCompared;
            QMap<QString, int> catPassed;
            QMap<QString, double> catSumSim;

            for (const auto &r : m_photoshopResults) {
                if (!r.originalRendered || !r.photoshopAvailable) {
                    ++totalSkipped;
                    continue;
                }
                ++totalCompared;
                catCompared[r.category]++;
                sumSimilarity += r.similarity;
                catSumSim[r.category] += r.similarity;
                if (r.similarity >= 90.0) {
                    ++totalPassed;
                    catPassed[r.category]++;
                } else {
                    ++totalFailed;
                }
            }

            ps << "# PSD Photoshop Similarity Report\n\n";
            ps << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
            ps << "Compares **QPsdView rendering of original PSD** vs **Photoshop rendering of round-trip PSD** (PNG screenshots).\n\n";

            // === Summary ===
            ps << "## Summary Statistics\n\n";
            ps << "| Metric | Value |\n";
            ps << "|--------|-------|\n";
            ps << "| Total Files | " << m_photoshopResults.size() << " |\n";
            ps << "| Compared | " << totalCompared << " |\n";
            ps << "| Skipped (no render / no screenshot) | " << totalSkipped << " |\n";
            const double passRate = totalCompared > 0 ? 100.0 * totalPassed / totalCompared : 0.0;
            ps << "| Passed (>=90%) | " << totalPassed << " (" << QString::number(passRate, 'f', 1) << "%) |\n";
            ps << "| Failed (<90%) | " << totalFailed << " |\n";
            const double avgSim = totalCompared > 0 ? sumSimilarity / totalCompared : 0.0;
            ps << "| Average Similarity | " << QString::number(avgSim, 'f', 1) << "% |\n\n";

            // === Per-Category Summary ===
            ps << "## Per-Category Summary\n\n";
            ps << "| Category | Pass | Total | Rate | Avg Similarity |\n";
            ps << "|----------|------|-------|------|----------------|\n";
            for (auto it = catCompared.cbegin(); it != catCompared.cend(); ++it) {
                const int pass = catPassed.value(it.key(), 0);
                const int total = it.value();
                const double rate = total > 0 ? 100.0 * pass / total : 0.0;
                const double avg = total > 0 ? catSumSim.value(it.key(), 0.0) / total : 0.0;
                ps << "| " << it.key() << " | " << pass << " | " << total
                   << " | " << QString::number(rate, 'f', 1) << "%"
                   << " | " << QString::number(avg, 'f', 1) << "% |\n";
            }
            ps << "\n";

            // === Per-Category Detailed Tables ===
            QList<PhotoshopResult> sorted = m_photoshopResults;
            std::sort(sorted.begin(), sorted.end(), [](const PhotoshopResult &a, const PhotoshopResult &b) {
                if (a.category != b.category) return a.category < b.category;
                return a.fileName < b.fileName;
            });

            QString currentCategory;
            for (const auto &r : sorted) {
                if (r.category != currentCategory) {
                    currentCategory = r.category;
                    ps << "## " << currentCategory << "\n\n";
                    ps << "| File | Size | Similarity | Result | Details |\n";
                    ps << "|------|-----:|----------:|:------:|:--------|\n";
                }
                const QString psdLink = u"[%1](https://github.com/signal-slot/psd-zoo/blob/master/%2/%1)"_s
                    .arg(r.fileName, r.category);
                if (!r.originalRendered) {
                    ps << "| " << psdLink << " | " << r.fileSize << " | - | SKIP | empty canvas |\n";
                } else if (!r.photoshopAvailable) {
                    ps << "| " << psdLink << " | " << r.fileSize << " | - | SKIP | no screenshot |\n";
                } else {
                    const QString status = r.similarity >= 90.0 ? u"PASS"_s : u"FAIL"_s;
                    QString details = u"-"_s;
                    if (r.sizeMismatch) {
                        details = u"scaled %1x%2→%3x%4"_s
                            .arg(r.photoshopSize.width()).arg(r.photoshopSize.height())
                            .arg(r.originalSize.width()).arg(r.originalSize.height());
                    }
                    ps << "| " << psdLink << " | " << r.fileSize
                       << " | " << QString::number(r.similarity, 'f', 1) << "%"
                       << " | " << status << " | " << details << " |\n";
                }
            }

            photoshopFile.close();
            qDebug() << "Photoshop report:" << photoshopReportPath;
            qDebug() << "Compared:" << totalCompared << "Pass:" << totalPassed << "Fail:" << totalFailed << "Skip:" << totalSkipped;
        }
    }
}

QTEST_MAIN(tst_QPsdWriter)
#include "tst_qpsdwriter.moc"

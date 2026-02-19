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
#include <QtPsdWriter/QPsdWriter>
#include <QtTest/QtTest>

#include <QtCore/QBuffer>
#include <QtCore/QDateTime>
#include <QtCore/QDirIterator>
#include <QtCore/QTemporaryFile>

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
    void writeReport_data();
    void writeReport();
    void cleanupTestCase();

private:
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

    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    const QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    QVERIFY2(writer.write(tmpPath), qPrintable(writer.errorString()));

    // Read written bytes
    QFile writtenFile(tmpPath);
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
    if (m_results.isEmpty())
        return;

    const QString reportPath = QStringLiteral(QT_TESTCASE_SOURCEDIR "/../../../../docs/psd-zoo-write-report.md");

    QFile reportFile(reportPath);
    if (!reportFile.open(QFile::WriteOnly | QFile::Truncate))
        return;

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
    // Sort results by category then fileName for grouped output
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
        const QString psdLink = u"[%1](https://github.com/signal-slot/psd-zoo/blob/main/%2/%1)"_s
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

QTEST_MAIN(tst_QPsdWriter)
#include "tst_qpsdwriter.moc"

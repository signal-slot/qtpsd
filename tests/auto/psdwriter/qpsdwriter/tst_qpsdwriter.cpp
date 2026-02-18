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

QTEST_MAIN(tst_QPsdWriter)
#include "tst_qpsdwriter.moc"

// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtGui/QImage>
#include <QtPsdCore/QPsdFileHeader>
#include <QtPsdCore/QPsdImageData>
#include <QtPsdCore/QPsdLayerRecord>
#include <QtPsdCore/QPsdParser>
#include <QtPsdGui/qpsdguiglobal.h>
#include <QtTest/QtTest>

class tst_ImageDataToImage : public QObject
{
    Q_OBJECT
private slots:
    void imageData_data();
    void imageData();
    void layerImageData_data();
    void layerImageData();

private:
    void addPsdFiles();
};

void tst_ImageDataToImage::addPsdFiles()
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

QTEST_MAIN(tst_ImageDataToImage)
#include "tst_image_data_to_image.moc"

// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/QPsdExporterPlugin>

#include <QtTest/QtTest>

QT_BEGIN_NAMESPACE

class ImageFileNameTestHelper : public QPsdExporterPlugin
{
public:
    using QPsdExporterPlugin::imageFileName;

    int priority() const override { return 0; }
    QString name() const override { return {}; }
    ExportType exportType() const override { return File; }
    bool exportTo(const QPsdExporterTreeItemModel *, const QString &, const QVariantMap &) const override { return false; }
};

QT_END_NAMESPACE

QT_USE_NAMESPACE

class tst_ImageFileName : public QObject
{
    Q_OBJECT
private slots:
    void asciiNameUnchanged();
    void nonAsciiNameHashed();
    void uniqueIdDifferentiatesHash();
    void emptyUniqueIdBackwardCompatible();
};

void tst_ImageFileName::asciiNameUnchanged()
{
    const QString result = ImageFileNameTestHelper::imageFileName("hello_world.png"_L1, "PNG"_L1);
    QCOMPARE(result, "hello_world.png"_L1);
}

void tst_ImageFileName::nonAsciiNameHashed()
{
    const QString result = ImageFileNameTestHelper::imageFileName(u"ベクトルスマートオブジェクト.ai"_s, "PNG"_L1);
    QVERIFY(result.endsWith(".png"_L1));
    QCOMPARE(result.length(), 16 + 4); // 16 hex chars + ".png"
}

void tst_ImageFileName::uniqueIdDifferentiatesHash()
{
    const QString name = u"ベクトルスマートオブジェクト.ai"_s;
    const QString r1 = ImageFileNameTestHelper::imageFileName(name, "PNG"_L1, "uuid-aaa"_ba);
    const QString r2 = ImageFileNameTestHelper::imageFileName(name, "PNG"_L1, "uuid-bbb"_ba);

    QVERIFY(r1 != r2);
    QVERIFY(r1.endsWith(".png"_L1));
    QVERIFY(r2.endsWith(".png"_L1));
}

void tst_ImageFileName::emptyUniqueIdBackwardCompatible()
{
    const QString name = u"ベクトルスマートオブジェクト.ai"_s;
    const QString withEmpty = ImageFileNameTestHelper::imageFileName(name, "PNG"_L1, {});
    const QString without = ImageFileNameTestHelper::imageFileName(name, "PNG"_L1);

    QCOMPARE(withEmpty, without);
}

QTEST_APPLESS_MAIN(tst_ImageFileName)
#include "tst_image_file_name.moc"

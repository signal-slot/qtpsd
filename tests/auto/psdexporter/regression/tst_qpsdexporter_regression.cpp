// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtPsdExporter/QPsdExporterPlugin>

#include <QtTest/QtTest>

class tst_QPsdExporter_Regression : public QObject {
    Q_OBJECT
private slots:
    void exporter_data();
    void exporter();

private:
    void removeContents(const QDir &dir);
    void compareFile(const QString &path1, const QString &path2);
    QList<QFileInfo> entryInfoList(const QDir &dir);
    void compareDir(const QString &path1, const QString &path2);
    void createGitKeep(const QString &path);
};

void tst_QPsdExporter_Regression::removeContents(const QDir &dir) {
    const auto contentsInfo = dir.entryInfoList(QDir::AllDirs | QDir::Files | QDir::Filter::NoDotAndDotDot);
    for (auto &contentInfo : contentsInfo) {
        if (contentInfo.isFile()) {
            auto res = QFile(contentInfo.absoluteFilePath()).remove();
        }
        if (contentInfo.isDir()) {
            auto res = QDir(contentInfo.absoluteFilePath()).removeRecursively();
        }
    }
}

void tst_QPsdExporter_Regression::compareFile(const QString &path1, const QString &path2)
{
    QFile file1(path1);
    QFile file2(path2);

    qint64 size1 = file1.size();
    QCOMPARE(size1, file2.size());

    QVERIFY(file1.open(QIODeviceBase::ReadOnly));
    QVERIFY(file2.open(QIODeviceBase::ReadOnly));

    int blockSize = 64 * 1024;
    while (file1.bytesAvailable()) {
        QByteArray bytes1 = file1.read(blockSize);
        QByteArray bytes2 = file2.read(blockSize);

        QCOMPARE(bytes1, bytes2);
    }
}

QList<QFileInfo> tst_QPsdExporter_Regression::entryInfoList(const QDir &dir) {
    auto entriesInfo = dir.entryInfoList(QDir::Filter::AllDirs | QDir::Filter::Files | QDir::Filter::NoDotAndDotDot);
    entriesInfo.removeIf([](const auto &entryInfo) { return entryInfo.fileName() == ".gitkeep"_L1; });

    return entriesInfo;
}

void tst_QPsdExporter_Regression::compareDir(const QString &path1, const QString &path2)
{
    QDir dir1(path1);
    QDir dir2(path2);

    QVERIFY(dir1.exists());
    QVERIFY(dir2.exists());

    const auto entriesInfo = entryInfoList(dir1);
    QCOMPARE(entriesInfo.size(), entryInfoList(dir2).size());

    for (const auto &entryInfo : entriesInfo) {
        if (entryInfo.isFile()) {
            compareFile(entryInfo.absoluteFilePath(), dir2.filePath(entryInfo.fileName()));
        }

        if (entryInfo.isDir()) {
            compareDir(entryInfo.absoluteFilePath(), dir2.filePath(entryInfo.fileName()));
        }
    }
}

void tst_QPsdExporter_Regression::createGitKeep(const QString &path) {
    QDir dir(path);

    const auto entries = dir.entryInfoList(QDir::Filter::AllDirs | QDir::Filter::Files | QDir::Filter::NoDotAndDotDot);

    if (entries.size() == 0) {
        QFile keep(dir.filePath(".gitkeep"_L1));
        keep.open(QFile::WriteOnly);
        keep.close();
    } else {
        for (const auto &entry : entries) {
            if (entry.isDir()) {
                createGitKeep(entry.absoluteFilePath());
            }
        }
    }
}

void tst_QPsdExporter_Regression::exporter_data() {
    QTest::addColumn<QDir>("subdir");
    QTest::addColumn<QString>("psd");
    QTest::addColumn<QByteArray>("pluginKey");
    QTest::addColumn<QString>("to");
    QTest::addColumn<QString>("expect");
    QTest::addColumn<bool>("updateExpects");
    bool updateExpects = qEnvironmentVariable("UPDATE_EXPECTS", "0") == "1";

    QDir dir;
    dir.cd(QFINDTESTDATA("data/"_L1));
    const auto subdirs = dir.entryList(QDir::Filter::AllDirs | QDir::Filter::NoDotAndDotDot);
    const auto keys = QPsdExporterPlugin::keys();
    for (const QString &sub : subdirs) {
        QDir subdir = dir;
        subdir.cd(sub);
        const auto fileNames = subdir.entryList(QStringList() << "*.psd"_L1);
        for (const QString &filename : fileNames) {
            for (const auto &key : keys) {
                QFileInfo psdInfo(subdir, filename);
                QString psdBaseName = psdInfo.completeBaseName();

                QDir exportRootDir(QDir::current().filePath("exports"_L1));
                QDir exportDir(exportRootDir.filePath(psdBaseName + ".exports"_L1));
                QDir exportPluginDir(exportDir.filePath(key));

                auto exporter = QPsdExporterPlugin::plugin(key);

                QDir expectsDir(subdir.filePath(psdBaseName + ".expects"_L1));
                QDir expectsPluginDir(expectsDir.filePath(key));

                QString to;
                QString expect;
                switch (exporter->exportType()) {
                case QPsdExporterPlugin::File: {
                    QString exportFileName = "export"_L1 + exporter->filters().values().at(0);
                    to = exportPluginDir.filePath(exportFileName);
                    expect = expectsPluginDir.filePath(exportFileName);
                    break;
                }
                case QPsdExporterPlugin::Directory:
                    to = exportPluginDir.absolutePath();
                    expect = expectsPluginDir.absolutePath();
                    break;
                }

                if (updateExpects) {
                    QTest::newRow("%1_%2_%3"_L1.arg(sub).arg(filename).arg(key).toLatin1().data()) << subdir << filename << key << expect << u""_s << true;
                } else {
                    QTest::newRow("%1_%2_%3"_L1.arg(sub).arg(filename).arg(key).toLatin1().data()) << subdir << filename << key << to << expect << false;
                }
            }
        }
    }
}

void tst_QPsdExporter_Regression::exporter() {
    QFETCH(QDir, subdir);
    QFETCH(QString, psd);
    QFETCH(QByteArray, pluginKey);
    QFETCH(QString, to);
    QFETCH(QString, expect);
    QFETCH(bool, updateExpects);

    QPsdGuiLayerTreeItemModel guiModel;
    QPsdExporterTreeItemModel model;
    model.setSourceModel(&guiModel);
    model.load(subdir.filePath(psd));

    QPsdExporterPlugin::ExportConfig defaultConfig;
    defaultConfig.targetSize = model.size();

    auto exporter = QPsdExporterPlugin::plugin(pluginKey);

    QFileInfo toInfo(to);
    QFileInfo expectInfo(expect);

    QDir toParent(toInfo.absoluteDir());
    switch (exporter->exportType()) {
    case QPsdExporterPlugin::File:
        QDir().mkpath(toInfo.absolutePath());
        break;
    case QPsdExporterPlugin::Directory:
        QDir().mkpath(toInfo.absoluteFilePath());
        removeContents(toInfo.absoluteFilePath());
        break;
    }

    exporter->exportTo(&model, to, defaultConfig);
    
    if (updateExpects && toInfo.isDir()) {
        createGitKeep(to);
    }

    if (expectInfo.exists()) {
        if (toInfo.isFile() && expectInfo.isFile()) {
            compareFile(toInfo.absoluteFilePath(), expectInfo.absoluteFilePath());
        } else if (toInfo.isDir() && expectInfo.isDir()) {
            compareDir(toInfo.absoluteFilePath(), expectInfo.absoluteFilePath());
        } else {
            QFAIL("invalid expect");
        }
    }
}

QTEST_MAIN(tst_QPsdExporter_Regression)
#include "tst_qpsdexporter_regression.moc"

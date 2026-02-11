// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtTest/QtTest>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtPsdCore/QPsdParser>
#include <QtPsdWidget/QPsdView>
#include <QtPsdWidget/QPsdWidgetTreeItemModel>
#include <QtCore/QDirIterator>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

class tst_QPsdView : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void generateImages_data();
    void generateImages();

private:
    struct SourceInfo {
        QString id;             // "ag-psd", "psd-zoo", ...
        QString testDataPath;   // QFINDTESTDATA result
    };

    void addPsdFiles();
    QImage renderPsdView(const QString &filePath);
    QString detectSource(const QString &psdPath) const;
    QString sourceRelativePath(const QString &psdPath, const QString &sourceId) const;

    bool m_saveImages = false;
    QString m_outputBaseDir;
    int m_limit = 0;
    QList<SourceInfo> m_sources;
};

void tst_QPsdView::initTestCase()
{
    const QString outputPath = qEnvironmentVariable("QTPSD_VIEW_OUTPUT_PATH");
    m_saveImages = !outputPath.isEmpty();

    if (m_saveImages) {
        QDir projectRoot = QDir::current();
        while (!projectRoot.exists("CMakeLists.txt") && projectRoot.cdUp()) {
            // Keep going up until project root is found.
        }

        if (QDir::isRelativePath(outputPath)) {
            m_outputBaseDir = projectRoot.absoluteFilePath(outputPath);
        } else {
            m_outputBaseDir = outputPath;
        }

        QDir().mkpath(m_outputBaseDir + "/images/psdview");
        qDebug() << "QPsdView image output directory:" << m_outputBaseDir;
    }

    m_sources = {
        {"ag-psd"_L1, QFINDTESTDATA("../../3rdparty/ag-psd/test/")},
        {"psd-zoo"_L1, QFINDTESTDATA("../../3rdparty/psd-zoo/")},
        {"psd-tools"_L1, QFINDTESTDATA("../../3rdparty/psd-tools/tests/psd_files/")},
    };

    const QStringList sourceFilter = qEnvironmentVariable("QTPSD_VIEW_SOURCES")
                                         .split(',', Qt::SkipEmptyParts);
    if (!sourceFilter.isEmpty()) {
        QList<SourceInfo> filtered;
        for (const auto &src : m_sources) {
            for (const auto &wanted : sourceFilter) {
                if (src.id.compare(wanted.trimmed(), Qt::CaseInsensitive) == 0) {
                    filtered.append(src);
                    break;
                }
            }
        }
        if (!filtered.isEmpty()) {
            m_sources = filtered;
        }
    }

    bool limitOk = false;
    const int limit = qEnvironmentVariable("QTPSD_VIEW_LIMIT").toInt(&limitOk);
    m_limit = (limitOk && limit > 0) ? limit : 0;
}

void tst_QPsdView::addPsdFiles()
{
    QTest::addColumn<QString>("psd");

    struct RowData {
        QString name;
        QString path;
    };

    QList<RowData> rows;
    for (const auto &src : m_sources) {
        if (src.testDataPath.isEmpty() || !QDir(src.testDataPath).exists()) {
            continue;
        }
        QDirIterator it(src.testDataPath, QStringList() << "*.psd", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString filePath = it.next();
            const QString relativePath = QDir(src.testDataPath).relativeFilePath(filePath);
            rows.append({src.id + "/"_L1 + relativePath, filePath});
        }
    }

    std::sort(rows.begin(), rows.end(), [](const RowData &a, const RowData &b) {
        return a.name < b.name;
    });

    int added = 0;
    for (const auto &row : rows) {
        if (m_limit > 0 && added >= m_limit) {
            break;
        }
        const QByteArray rowName = row.name.toUtf8();
        QTest::newRow(rowName.constData()) << row.path;
        ++added;
    }
}

QImage tst_QPsdView::renderPsdView(const QString &filePath)
{
    QPsdParser parser;
    parser.load(filePath);

    QPsdWidgetTreeItemModel model;
    model.fromParser(parser);

    QPsdView view;
    const QSize canvasSize = model.size();
    if (canvasSize.isEmpty()) {
        return QImage();
    }

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

QString tst_QPsdView::detectSource(const QString &psdPath) const
{
    const QString canonicalPsd = QFileInfo(psdPath).canonicalFilePath();
    for (const auto &src : m_sources) {
        if (src.testDataPath.isEmpty()) {
            continue;
        }
        const QString canonicalBase = QFileInfo(src.testDataPath).canonicalFilePath();
        if (!canonicalBase.isEmpty() && canonicalPsd.startsWith(canonicalBase)) {
            return src.id;
        }
    }
    return QString();
}

QString tst_QPsdView::sourceRelativePath(const QString &psdPath, const QString &sourceId) const
{
    for (const auto &src : m_sources) {
        if (src.id != sourceId) {
            continue;
        }
        const QString canonicalBase = QFileInfo(src.testDataPath).canonicalFilePath();
        const QString canonicalPsd = QFileInfo(psdPath).canonicalFilePath();
        if (!canonicalBase.isEmpty() && !canonicalPsd.isEmpty()) {
            return QDir(canonicalBase).relativeFilePath(canonicalPsd);
        }
    }
    return QFileInfo(psdPath).fileName();
}

void tst_QPsdView::generateImages_data()
{
    addPsdFiles();
}

void tst_QPsdView::generateImages()
{
    QFETCH(QString, psd);

    QImage viewRendering = renderPsdView(psd);
    if (viewRendering.isNull()) {
        QPsdParser parser;
        parser.load(psd);
        const auto header = parser.fileHeader();
        const QSize canvasSize(header.width(), header.height());
        if (!canvasSize.isEmpty()) {
            viewRendering = QImage(canvasSize, QImage::Format_ARGB32);
            viewRendering.fill(Qt::transparent);
        }
    }

    if (!m_saveImages) {
        QVERIFY2(!viewRendering.isNull(), qPrintable(QString("Failed to render QPsdView image: %1").arg(psd)));
        return;
    }

    const QString source = detectSource(psd);
    const QString sourceId = source.isEmpty() ? "other"_L1 : source;
    const QString relativePsdPath = sourceRelativePath(psd, sourceId);
    const QString subDir = QFileInfo(relativePsdPath).path();
    const QString baseName = QFileInfo(relativePsdPath).baseName();
    const QString dirPart = (subDir.isEmpty() || subDir == ".") ? QString() : (subDir + "/"_L1);

    if (subDir != ".") {
        QDir().mkpath(m_outputBaseDir + "/images/psdview/" + sourceId + "/" + subDir);
    } else {
        QDir().mkpath(m_outputBaseDir + "/images/psdview/" + sourceId);
    }

    if (!viewRendering.isNull()) {
        const QString psdViewPath = m_outputBaseDir + "/images/psdview/" + sourceId + "/" + dirPart + baseName + ".png";
        QVERIFY2(viewRendering.save(psdViewPath),
                 qPrintable(QString("Failed to save QPsdView image: %1").arg(psdViewPath)));
    }
}

QTEST_MAIN(tst_QPsdView)

#include "tst_qpsdview.moc"

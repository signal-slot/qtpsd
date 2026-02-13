// Copyright (C) 2026 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QtTest/QtTest>
#include <QtGui/QImage>
#include <QtCore/QDateTime>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>
#include <QtCore/QTextStream>

#include <algorithm>
#include <cmath>
#include <tuple>

class tst_PsdExporterSimilarity : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void generateReport();

private:
    struct Result {
        QString fileName;
        qint64 fileSize = 0;

        double similarityImageDataVsPsdView = 0.0;
        double similarityImageDataVsQtQuick = 0.0;
        double similarityImageDataVsSlint = 0.0;
        double similarityImageDataVsFlutter = 0.0;

        bool passedImageDataVsPsdView = false;
        bool passedImageDataVsQtQuick = false;
        bool passedImageDataVsSlint = false;
        bool passedImageDataVsFlutter = false;

        bool hasImageData = false;
        bool hasPsdView = false;
        bool hasQtQuick = false;
        bool hasSlint = false;
        bool hasFlutter = false;

        QString imageDataRel;
        QString psdViewRel;
        QString psdViewDiffRel;
        QString qtQuickRel;
        QString qtQuickDiffRel;
        QString slintRel;
        QString slintDiffRel;
        QString flutterRel;
        QString flutterDiffRel;
    };

    enum class Metric {
        ImageDataVsPsdView,
        ImageDataVsQtQuick,
        ImageDataVsSlint,
        ImageDataVsFlutter,
    };

    QString findProjectRoot() const;
    QString sourcePathFor(const QString &sourceId) const;
    QString findPsdExporterBin() const;
    QString findQmlCaptureScript() const;
    QString findSlintCaptureScript() const;
    QString findFlutterCaptureScript() const;

    QString formatFileSize(qint64 fileSize) const;
    double compareImages(const QImage &img1, const QImage &img2) const;
    QImage createDiffImage(const QImage &img1, const QImage &img2) const;

    bool runProcess(const QString &program,
                    const QStringList &arguments,
                    const QString &workingDirectory,
                    int timeoutMs,
                    QString *errorMessage) const;

    bool exportPsd(const QString &psdPath,
                   const QString &type,
                   const QString &outDir,
                   QString *errorMessage) const;

    QImage renderQtQuick(const QString &qmlPath,
                         const QString &outPngPath,
                         QString *errorMessage) const;

    QImage renderSlint(const QString &slintPath,
                       const QString &outPngPath,
                       QString *errorMessage) const;
    QImage renderFlutter(const QString &dartPath,
                         const QString &outPngPath,
                         QString *errorMessage) const;

    double metricSimilarity(const Result &result, Metric metric) const;
    bool metricPassed(const Result &result, Metric metric) const;
    QString metricTargetImage(const Result &result, Metric metric) const;
    QString metricDiffImage(const Result &result, Metric metric) const;
    QString metricStatus(const Result &result, Metric metric) const;

    void writeSection(QTextStream &stream,
                      const QList<Result> &results,
                      const QString &title,
                      Metric metric,
                      const QString &rightLabel) const;

    void writeReport(const QList<Result> &results) const;

    QString m_projectRoot;
    QString m_outputBaseDir;
    QString m_sourceId;
    QString m_psdRoot;
    QString m_reportPath;

    QString m_psdExporterBin;
    QString m_qmlCaptureScript;
    QString m_slintCaptureScript;
    QString m_flutterCaptureScript;

    bool m_runExport = true;
    bool m_exportOnly = false;
    bool m_renderQtQuick = true;
    bool m_renderSlint = true;
    bool m_renderFlutter = true;
    int m_limit = 0;
};

QString tst_PsdExporterSimilarity::findProjectRoot() const
{
    QDir dir = QDir::current();
    while (!dir.exists("CMakeLists.txt") && dir.cdUp()) {
        // Keep going up.
    }
    return dir.absolutePath();
}

QString tst_PsdExporterSimilarity::sourcePathFor(const QString &sourceId) const
{
    if (sourceId == QStringLiteral("psd-zoo")) {
        return QFINDTESTDATA("../../3rdparty/psd-zoo/");
    }
    if (sourceId == QStringLiteral("ag-psd")) {
        return QFINDTESTDATA("../../3rdparty/ag-psd/test/");
    }
    if (sourceId == QStringLiteral("psd-tools")) {
        return QFINDTESTDATA("../../3rdparty/psd-tools/tests/psd_files/");
    }
    return QString();
}

QString tst_PsdExporterSimilarity::findPsdExporterBin() const
{
    const QString fromEnv = qEnvironmentVariable("QTPSD_PSDEXPORTER_BIN");
    if (!fromEnv.isEmpty() && QFileInfo::exists(fromEnv)) {
        return fromEnv;
    }

    const QString fromPath = QStandardPaths::findExecutable("psdexporter");
    if (!fromPath.isEmpty()) {
        return fromPath;
    }

    const QStringList candidates = {
        m_projectRoot + "/build/Qt_6-Debug/bin/psdexporter",
        m_projectRoot + "/build/bin/psdexporter",
    };
    for (const auto &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return QString();
}

QString tst_PsdExporterSimilarity::findQmlCaptureScript() const
{
    const QString fromEnv = qEnvironmentVariable("QTPSD_QML_CAPTURE_SCRIPT");
    if (!fromEnv.isEmpty() && QFileInfo::exists(fromEnv)) {
        return fromEnv;
    }

    const QString scriptPath = m_projectRoot + "/scripts/qml2png.sh";
    if (QFileInfo::exists(scriptPath)) {
        return scriptPath;
    }

    return QString();
}

QString tst_PsdExporterSimilarity::findSlintCaptureScript() const
{
    const QString fromEnv = qEnvironmentVariable("QTPSD_SLINT_CAPTURE_SCRIPT");
    if (!fromEnv.isEmpty() && QFileInfo::exists(fromEnv)) {
        return fromEnv;
    }

    const QString scriptPath = m_projectRoot + "/scripts/slint2png.sh";
    if (QFileInfo::exists(scriptPath)) {
        return scriptPath;
    }

    return QString();
}

QString tst_PsdExporterSimilarity::findFlutterCaptureScript() const
{
    const QString fromEnv = qEnvironmentVariable("QTPSD_FLUTTER_CAPTURE_SCRIPT");
    if (!fromEnv.isEmpty() && QFileInfo::exists(fromEnv)) {
        return fromEnv;
    }

    const QString scriptPath = m_projectRoot + "/scripts/flutter2png.sh";
    if (QFileInfo::exists(scriptPath)) {
        return scriptPath;
    }

    return QString();
}

QString tst_PsdExporterSimilarity::formatFileSize(qint64 fileSize) const
{
    if (fileSize < 1024) {
        return QString::number(fileSize) + QStringLiteral(" B");
    }
    if (fileSize < 1024 * 1024) {
        return QString::number(fileSize / 1024.0, 'f', 1) + QStringLiteral(" KB");
    }
    return QString::number(fileSize / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
}

double tst_PsdExporterSimilarity::compareImages(const QImage &img1, const QImage &img2) const
{
    if (img1.size() != img2.size() || img1.isNull() || img2.isNull()) {
        return 0.0;
    }

    QImage a = img1.convertToFormat(QImage::Format_ARGB32);
    QImage b = img2.convertToFormat(QImage::Format_ARGB32);
    if (a.isNull() || b.isNull()) {
        return 0.0;
    }

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
            if (cmax == rf) {
                h = 60.0 * fmod((gf - bf) / diff, 6.0);
            } else if (cmax == gf) {
                h = 60.0 * ((bf - rf) / diff + 2.0);
            } else {
                h = 60.0 * ((rf - gf) / diff + 4.0);
            }
        }
        if (h < 0.0) {
            h += 360.0;
        }

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

            if (cr1 >= 250 && cg1 >= 250 && cb1 >= 250 &&
                cr2 >= 250 && cg2 >= 250 && cb2 >= 250) {
                continue;
            }

            ++pixelsWithContent;

            const auto [h1, s1, v1] = rgbToHsv(cr1, cg1, cb1);
            const auto [h2, s2, v2] = rgbToHsv(cr2, cg2, cb2);

            double hDiff = qAbs(h1 - h2);
            if (hDiff > 180.0) {
                hDiff = 360.0 - hDiff;
            }
            hDiff /= 180.0;

            const double sDiff = qAbs(s1 - s2);
            const double vDiff = qAbs(v1 - v2);
            const double pixelDiff = hDiff * 0.3 + sDiff * 0.3 + vDiff * 0.4;

            totalDiff += pixelDiff;
            if (pixelDiff > 0.1) {
                ++pixelsDifferent;
            }
        }
    }

    if (pixelsWithContent == 0) {
        return 100.0;
    }

    const double contentBasedSimilarity = 100.0 * (1.0 - static_cast<double>(pixelsDifferent) / static_cast<double>(pixelsWithContent));
    const double hsvBasedSimilarity = 100.0 * (1.0 - totalDiff / static_cast<double>(pixelsWithContent));
    return qMin(contentBasedSimilarity, hsvBasedSimilarity);
}

QImage tst_PsdExporterSimilarity::createDiffImage(const QImage &img1, const QImage &img2) const
{
    if (img1.size() != img2.size() || img1.isNull() || img2.isNull()) {
        return QImage();
    }

    QImage diff(img1.size(), QImage::Format_ARGB32);
    diff.fill(Qt::transparent);

    auto compositeOverWhite = [](int r, int g, int b, int alpha) -> std::tuple<int, int, int> {
        const int rr = (r * alpha + 255 * (255 - alpha)) / 255;
        const int gg = (g * alpha + 255 * (255 - alpha)) / 255;
        const int bb = (b * alpha + 255 * (255 - alpha)) / 255;
        return {rr, gg, bb};
    };

    for (int y = 0; y < diff.height(); ++y) {
        for (int x = 0; x < diff.width(); ++x) {
            const QRgb p1 = img1.pixel(x, y);
            const QRgb p2 = img2.pixel(x, y);

            const auto [cr1, cg1, cb1] = compositeOverWhite(qRed(p1), qGreen(p1), qBlue(p1), qAlpha(p1));
            const auto [cr2, cg2, cb2] = compositeOverWhite(qRed(p2), qGreen(p2), qBlue(p2), qAlpha(p2));

            const bool p1HasContent = (cr1 < 240 || cg1 < 240 || cb1 < 240);
            const bool p2HasContent = (cr2 < 240 || cg2 < 240 || cb2 < 240);

            if (p1HasContent && !p2HasContent) {
                diff.setPixel(x, y, qRgba(255, 0, 0, 255));
            } else if (!p1HasContent && p2HasContent) {
                diff.setPixel(x, y, qRgba(0, 0, 255, 255));
            } else if (p1HasContent && p2HasContent) {
                const int colorDiff = qAbs(cr1 - cr2) + qAbs(cg1 - cg2) + qAbs(cb1 - cb2);
                if (colorDiff > 30) {
                    diff.setPixel(x, y, qRgba(255, 255, 0, 255));
                } else {
                    diff.setPixel(x, y, qRgba(128, 128, 128, 255));
                }
            }
        }
    }

    return diff;
}

bool tst_PsdExporterSimilarity::runProcess(const QString &program,
                                           const QStringList &arguments,
                                           const QString &workingDirectory,
                                           int timeoutMs,
                                           QString *errorMessage) const
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    if (!workingDirectory.isEmpty()) {
        process.setWorkingDirectory(workingDirectory);
    }

    process.start();
    if (!process.waitForStarted()) {
        if (errorMessage) {
            *errorMessage = QString("Failed to start process: %1").arg(program);
        }
        return false;
    }

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished();
        if (errorMessage) {
            *errorMessage = QString("Process timeout: %1 %2")
                                .arg(program, arguments.join(' '));
        }
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = QString("Command failed (%1):\nstdout:\n%2\nstderr:\n%3")
                                .arg(process.exitCode())
                                .arg(QString::fromUtf8(process.readAllStandardOutput()))
                                .arg(QString::fromUtf8(process.readAllStandardError()));
        }
        return false;
    }

    return true;
}

bool tst_PsdExporterSimilarity::exportPsd(const QString &psdPath,
                                          const QString &type,
                                          const QString &outDir,
                                          QString *errorMessage) const
{
    if (m_psdExporterBin.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("psdexporter binary is not found");
        }
        return false;
    }

    QDir exportDir(outDir);
    if (exportDir.exists()) {
        exportDir.removeRecursively();
    }
    QDir().mkpath(outDir);

    const QStringList args = {
        QStringLiteral("--input"),
        psdPath,
        QStringLiteral("--type"),
        type,
        QStringLiteral("--outdir"),
        outDir,
        QStringLiteral("--resolution"),
        QStringLiteral("original"),
    };

    return runProcess(m_psdExporterBin, args, QString(), 120000, errorMessage);
}

QImage tst_PsdExporterSimilarity::renderQtQuick(const QString &qmlPath,
                                                const QString &outPngPath,
                                                QString *errorMessage) const
{
    if (m_qmlCaptureScript.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("qml capture script is not found");
        }
        return QImage();
    }

    QDir().mkpath(QFileInfo(outPngPath).absolutePath());
    QFile::remove(outPngPath);

    if (!runProcess(m_qmlCaptureScript, {qmlPath, outPngPath}, QString(), 120000, errorMessage)) {
        return QImage();
    }

    QImage image(outPngPath);
    if (image.isNull() && errorMessage) {
        *errorMessage = QString("Failed to load captured QtQuick image: %1").arg(outPngPath);
    }
    return image;
}

QImage tst_PsdExporterSimilarity::renderSlint(const QString &slintPath,
                                              const QString &outPngPath,
                                              QString *errorMessage) const
{
    if (m_slintCaptureScript.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("slint capture script is not found");
        }
        return QImage();
    }

    QDir().mkpath(QFileInfo(outPngPath).absolutePath());
    QFile::remove(outPngPath);

    if (!runProcess(m_slintCaptureScript, {slintPath, outPngPath}, QString(), 120000, errorMessage)) {
        return QImage();
    }

    QImage image(outPngPath);
    if (image.isNull() && errorMessage) {
        *errorMessage = QString("Failed to load captured Slint image: %1").arg(outPngPath);
    }
    return image;
}

QImage tst_PsdExporterSimilarity::renderFlutter(const QString &dartPath,
                                                const QString &outPngPath,
                                                QString *errorMessage) const
{
    if (m_flutterCaptureScript.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("flutter capture script is not found");
        }
        return QImage();
    }

    QDir().mkpath(QFileInfo(outPngPath).absolutePath());
    QFile::remove(outPngPath);

    if (!runProcess(m_flutterCaptureScript, {dartPath, outPngPath}, QString(), 300000, errorMessage)) {
        return QImage();
    }

    QImage image(outPngPath);
    if (image.isNull() && errorMessage) {
        *errorMessage = QString("Failed to load captured Flutter image: %1").arg(outPngPath);
    }
    return image;
}

double tst_PsdExporterSimilarity::metricSimilarity(const Result &result, Metric metric) const
{
    switch (metric) {
    case Metric::ImageDataVsPsdView:
        return result.similarityImageDataVsPsdView;
    case Metric::ImageDataVsQtQuick:
        return result.similarityImageDataVsQtQuick;
    case Metric::ImageDataVsSlint:
        return result.similarityImageDataVsSlint;
    case Metric::ImageDataVsFlutter:
        return result.similarityImageDataVsFlutter;
    }
    return 0.0;
}

bool tst_PsdExporterSimilarity::metricPassed(const Result &result, Metric metric) const
{
    switch (metric) {
    case Metric::ImageDataVsPsdView:
        return result.passedImageDataVsPsdView;
    case Metric::ImageDataVsQtQuick:
        return result.passedImageDataVsQtQuick;
    case Metric::ImageDataVsSlint:
        return result.passedImageDataVsSlint;
    case Metric::ImageDataVsFlutter:
        return result.passedImageDataVsFlutter;
    }
    return false;
}

QString tst_PsdExporterSimilarity::metricTargetImage(const Result &result, Metric metric) const
{
    switch (metric) {
    case Metric::ImageDataVsPsdView:
        return result.psdViewRel;
    case Metric::ImageDataVsQtQuick:
        return result.qtQuickRel;
    case Metric::ImageDataVsSlint:
        return result.slintRel;
    case Metric::ImageDataVsFlutter:
        return result.flutterRel;
    }
    return QString();
}

QString tst_PsdExporterSimilarity::metricDiffImage(const Result &result, Metric metric) const
{
    switch (metric) {
    case Metric::ImageDataVsPsdView:
        return result.psdViewDiffRel;
    case Metric::ImageDataVsQtQuick:
        return result.qtQuickDiffRel;
    case Metric::ImageDataVsSlint:
        return result.slintDiffRel;
    case Metric::ImageDataVsFlutter:
        return result.flutterDiffRel;
    }
    return QString();
}

QString tst_PsdExporterSimilarity::metricStatus(const Result &result, Metric metric) const
{
    const double similarity = metricSimilarity(result, metric);
    const bool passed = metricPassed(result, metric);

    if (metric == Metric::ImageDataVsPsdView && (!result.hasImageData || !result.hasPsdView)) {
        return QStringLiteral("MISSING");
    }
    if (metric == Metric::ImageDataVsQtQuick && (!result.hasImageData || !result.hasQtQuick)) {
        return QStringLiteral("MISSING");
    }
    if (metric == Metric::ImageDataVsSlint && (!result.hasImageData || !result.hasSlint)) {
        return QStringLiteral("MISSING");
    }
    if (metric == Metric::ImageDataVsFlutter && (!result.hasImageData || !result.hasFlutter)) {
        return QStringLiteral("MISSING");
    }

    if (!passed) {
        return QStringLiteral("FAILED");
    }
    if (similarity >= 99.0) {
        return QStringLiteral("PERFECT");
    }
    if (similarity >= 90.0) {
        return QStringLiteral("GOOD");
    }
    return QStringLiteral("LOW");
}

void tst_PsdExporterSimilarity::writeSection(QTextStream &stream,
                                             const QList<Result> &results,
                                             const QString &title,
                                             Metric metric,
                                             const QString &rightLabel) const
{
    stream << "## " << title << "\n\n";
    stream << "| File | Size | Similarity | Status | Image Data | " << rightLabel << " | Difference |\n";
    stream << "|------|------|------------|--------|------------|" << QString("-").repeated(rightLabel.size() + 2) << "|------------|\n";

    auto encoded = [](QString path) {
        return path.replace(" ", "%20");
    };

    const QString githubBase = QStringLiteral("https://github.com/signal-slot/psd-zoo/tree/main/");

    for (const auto &result : results) {
        const QString relForLink = encoded(result.fileName);
        const QString githubUrl = githubBase + relForLink;
        const double similarity = metricSimilarity(result, metric);
        const QString status = metricStatus(result, metric);
        const QString targetRel = metricTargetImage(result, metric);
        const QString diffRel = metricDiffImage(result, metric);

        stream << "| [" << result.fileName << "](" << githubUrl << ")"
               << " | " << formatFileSize(result.fileSize)
               << " | " << QString::number(similarity, 'f', 2) << "%"
               << " | " << status;

        if (!result.imageDataRel.isEmpty()) {
            const QString p = encoded(result.imageDataRel);
            stream << " | [<img src=\"" << p << "\" width=\"100\">](" << p << ")";
        } else {
            stream << " | ";
        }

        if (!targetRel.isEmpty()) {
            const QString p = encoded(targetRel);
            stream << " | [<img src=\"" << p << "\" width=\"100\">](" << p << ")";
        } else {
            stream << " | ";
        }

        if (!diffRel.isEmpty()) {
            const QString p = encoded(diffRel);
            stream << " | [<img src=\"" << p << "\" width=\"100\">](" << p << ")";
        } else {
            stream << " | ";
        }

        stream << " |\n";
    }

    stream << "\n";
}

void tst_PsdExporterSimilarity::writeReport(const QList<Result> &results) const
{
    QFile reportFile(m_reportPath);
    QVERIFY2(reportFile.open(QIODevice::WriteOnly | QIODevice::Text), qPrintable("Failed to open report file: " + m_reportPath));

    QTextStream stream(&reportFile);
    stream << "# QtPsd Similarity Test Results (" << m_sourceId << ")\n\n";
    stream << "Generated on: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";

    auto accumulate = [&](Metric metric) {
        int total = 0;
        int passed = 0;
        double sum = 0.0;
        double min = 100.0;
        double max = 0.0;

        for (const auto &r : results) {
            const bool available =
                (metric == Metric::ImageDataVsPsdView && r.hasImageData && r.hasPsdView) ||
                (metric == Metric::ImageDataVsQtQuick && r.hasImageData && r.hasQtQuick) ||
                (metric == Metric::ImageDataVsSlint && r.hasImageData && r.hasSlint) ||
                (metric == Metric::ImageDataVsFlutter && r.hasImageData && r.hasFlutter);

            if (!available) {
                continue;
            }

            const double value = metricSimilarity(r, metric);
            ++total;
            if (metricPassed(r, metric)) {
                ++passed;
            }
            sum += value;
            min = qMin(min, value);
            max = qMax(max, value);
        }

        const double avg = total > 0 ? sum / total : 0.0;
        if (total == 0) {
            min = 0.0;
        }
        return std::tuple<int, int, double, double, double>(total, passed, avg, min, max);
    };

    const auto [pvTotal, pvPassed, pvAvg, pvMin, pvMax] = accumulate(Metric::ImageDataVsPsdView);
    const auto [qtTotal, qtPassed, qtAvg, qtMin, qtMax] = accumulate(Metric::ImageDataVsQtQuick);
    const auto [slTotal, slPassed, slAvg, slMin, slMax] = accumulate(Metric::ImageDataVsSlint);
    const auto [flTotal, flPassed, flAvg, flMin, flMax] = accumulate(Metric::ImageDataVsFlutter);

    stream << "## Summary Statistics\n\n";
    stream << "| Metric | Image Data vs QPsdView | Image Data vs QtQuick Export | Image Data vs Slint Export | Image Data vs Flutter Export |\n";
    stream << "|--------|------------------------|------------------------------|----------------------------|------------------------------|\n";
    stream << "| Total Tests | " << pvTotal << " | " << qtTotal << " | " << slTotal << " | " << flTotal << " |\n";
    stream << "| Passed Tests (>50%) | " << pvPassed << " ("
           << QString::number(pvTotal > 0 ? 100.0 * pvPassed / pvTotal : 0.0, 'f', 1)
           << "%) | " << qtPassed << " ("
           << QString::number(qtTotal > 0 ? 100.0 * qtPassed / qtTotal : 0.0, 'f', 1)
           << "%) | " << slPassed << " ("
           << QString::number(slTotal > 0 ? 100.0 * slPassed / slTotal : 0.0, 'f', 1)
           << "%) | " << flPassed << " ("
           << QString::number(flTotal > 0 ? 100.0 * flPassed / flTotal : 0.0, 'f', 1)
           << "%) |\n";
    stream << "| Average Similarity | "
           << QString::number(pvAvg, 'f', 2) << "% | "
           << QString::number(qtAvg, 'f', 2) << "% | "
           << QString::number(slAvg, 'f', 2) << "% | "
           << QString::number(flAvg, 'f', 2) << "% |\n";
    stream << "| Minimum Similarity | "
           << QString::number(pvMin, 'f', 2) << "% | "
           << QString::number(qtMin, 'f', 2) << "% | "
           << QString::number(slMin, 'f', 2) << "% | "
           << QString::number(flMin, 'f', 2) << "% |\n";
    stream << "| Maximum Similarity | "
           << QString::number(pvMax, 'f', 2) << "% | "
           << QString::number(qtMax, 'f', 2) << "% | "
           << QString::number(slMax, 'f', 2) << "% | "
           << QString::number(flMax, 'f', 2) << "% |\n\n";

    auto encoded = [](QString path) {
        return path.replace(" ", "%20");
    };

    auto availableForMetric = [](const Result &r, Metric metric) {
        if (metric == Metric::ImageDataVsPsdView) {
            return r.hasImageData && r.hasPsdView;
        }
        if (metric == Metric::ImageDataVsQtQuick) {
            return r.hasImageData && r.hasQtQuick;
        }
        if (metric == Metric::ImageDataVsSlint) {
            return r.hasImageData && r.hasSlint;
        }
        return r.hasImageData && r.hasFlutter;
    };

    auto exportDirForMetric = [&](const Result &r, Metric metric) -> QString {
        const int dotPos = r.fileName.lastIndexOf('.');
        const QString relNoExt = dotPos >= 0 ? r.fileName.left(dotPos) : r.fileName;
        switch (metric) {
        case Metric::ImageDataVsQtQuick:
            return QString("exports/%1/%2/QtQuick").arg(m_sourceId, relNoExt);
        case Metric::ImageDataVsSlint:
            return QString("exports/%1/%2/Slint").arg(m_sourceId, relNoExt);
        case Metric::ImageDataVsFlutter:
            return QString("exports/%1/%2/Flutter").arg(m_sourceId, relNoExt);
        case Metric::ImageDataVsPsdView:
            break;
        }
        return QString();
    };

    auto similarityCell = [&](const Result &r, Metric metric) {
        if (!availableForMetric(r, metric)) {
            return QStringLiteral("MISSING");
        }
        const QString value = QString::number(metricSimilarity(r, metric), 'f', 2) + QStringLiteral("%");
        const QString exportDir = exportDirForMetric(r, metric);
        if (exportDir.isEmpty()) {
            return value;
        }
        const QString path = encoded(exportDir);
        return QStringLiteral("[%1](%2/)").arg(value, path);
    };

    auto imageCell = [&](const QString &relPath, bool available) {
        if (!available || relPath.isEmpty()) {
            return QStringLiteral("-");
        }
        const QString path = encoded(relPath);
        return QStringLiteral("[<img src=\"%1\" width=\"140\">](%1)").arg(path);
    };

    const QString githubBase = QStringLiteral("https://github.com/signal-slot/psd-zoo/tree/main/");
    const QString tableHeader =
        "| Image Data | QPsdView | QtQuick | Slint | Flutter |\n"
        "|---:|---:|---:|---:|---:|\n";

    QString currentCategory;
    bool firstCategory = true;

    for (const auto &result : results) {
        const int slashPos = result.fileName.indexOf('/');
        const QString category = slashPos >= 0 ? result.fileName.left(slashPos) : QStringLiteral("uncategorized");
        const QString displayName = slashPos >= 0 ? result.fileName.mid(slashPos + 1) : result.fileName;
        const QString relForLink = encoded(result.fileName);
        const QString githubUrl = githubBase + relForLink;

        if (category != currentCategory) {
            if (!firstCategory) {
                stream << "\n";
            }
            stream << "## " << category << "\n\n";
            stream << tableHeader;
            currentCategory = category;
            firstCategory = false;
        }

        stream << "| [" << displayName << "](" << githubUrl << ") | "
               << similarityCell(result, Metric::ImageDataVsPsdView) << " | "
               << similarityCell(result, Metric::ImageDataVsQtQuick) << " | "
               << similarityCell(result, Metric::ImageDataVsSlint) << " | "
               << similarityCell(result, Metric::ImageDataVsFlutter) << " |\n";
        stream << "| "
               << imageCell(result.imageDataRel, result.hasImageData) << " | "
               << imageCell(result.psdViewRel, result.hasPsdView) << " | "
               << imageCell(result.qtQuickRel, result.hasQtQuick) << " | "
               << imageCell(result.slintRel, result.hasSlint) << " | "
               << imageCell(result.flutterRel, result.hasFlutter) << " |\n";
    }
    stream << "\n";
}

void tst_PsdExporterSimilarity::initTestCase()
{
    m_projectRoot = findProjectRoot();
    m_sourceId = qEnvironmentVariable("QTPSD_SIMILARITY_SOURCE", "psd-zoo");
    m_psdRoot = sourcePathFor(m_sourceId);

    QVERIFY2(!m_psdRoot.isEmpty(), qPrintable("Unsupported source id: " + m_sourceId));
    QVERIFY2(QDir(m_psdRoot).exists(), qPrintable("Source path does not exist: " + m_psdRoot));

    QString outputPath = qEnvironmentVariable("QTPSD_SIMILARITY_OUTPUT_PATH");
    if (outputPath.isEmpty()) {
        outputPath = m_projectRoot + "/docs";
    }
    if (QDir::isRelativePath(outputPath)) {
        m_outputBaseDir = QDir(m_projectRoot).absoluteFilePath(outputPath);
    } else {
        m_outputBaseDir = outputPath;
    }

    m_reportPath = m_outputBaseDir + QString("/similarity_report_%1.md").arg(m_sourceId);

    m_runExport = qEnvironmentVariableIntValue("QTPSD_SIMILARITY_RUN_EXPORT") != 0;
    m_exportOnly = qEnvironmentVariableIntValue("QTPSD_SIMILARITY_EXPORT_ONLY") != 0;
    if (m_exportOnly) {
        m_runExport = true;
    }

    const QString exporterList = qEnvironmentVariable("QTPSD_SIMILARITY_EXPORTERS", "qtquick,slint,flutter").toLower();
    m_renderQtQuick = exporterList.contains("qtquick");
    m_renderSlint = exporterList.contains("slint");
    m_renderFlutter = exporterList.contains("flutter");

    bool limitOk = false;
    const int limit = qEnvironmentVariable("QTPSD_SIMILARITY_LIMIT").toInt(&limitOk);
    m_limit = (limitOk && limit > 0) ? limit : 0;

    m_psdExporterBin = findPsdExporterBin();
    if ((m_renderQtQuick || m_renderSlint || m_renderFlutter) && m_runExport) {
        QVERIFY2(!m_psdExporterBin.isEmpty(), "psdexporter binary not found. Set QTPSD_PSDEXPORTER_BIN or add it to PATH.");
    }

    m_qmlCaptureScript = findQmlCaptureScript();
    if (m_renderQtQuick) {
        QVERIFY2(!m_qmlCaptureScript.isEmpty(),
                 "QML capture script not found. Expected scripts/qml2png.sh or set QTPSD_QML_CAPTURE_SCRIPT.");
    }

    m_slintCaptureScript = findSlintCaptureScript();
    if (m_renderSlint) {
        QVERIFY2(!m_slintCaptureScript.isEmpty(), "Slint capture script not found. Expected scripts/slint2png.sh or set QTPSD_SLINT_CAPTURE_SCRIPT.");
    }

    m_flutterCaptureScript = findFlutterCaptureScript();
    if (m_renderFlutter) {
        QVERIFY2(!m_flutterCaptureScript.isEmpty(), "Flutter capture script not found. Expected scripts/flutter2png.sh or set QTPSD_FLUTTER_CAPTURE_SCRIPT.");
    }

    QDir().mkpath(m_outputBaseDir + "/images/qtquick/" + m_sourceId);
    QDir().mkpath(m_outputBaseDir + "/images/slint/" + m_sourceId);
    QDir().mkpath(m_outputBaseDir + "/images/flutter/" + m_sourceId);
    QDir().mkpath(m_outputBaseDir + "/exports/" + m_sourceId);

    qInfo() << "Source:" << m_sourceId;
    qInfo() << "PSD root:" << m_psdRoot;
    qInfo() << "Output directory:" << m_outputBaseDir;
    qInfo() << "Run export:" << m_runExport << "QtQuick:" << m_renderQtQuick << "Slint:" << m_renderSlint << "Flutter:" << m_renderFlutter
            << "Export-only:" << m_exportOnly;
}

void tst_PsdExporterSimilarity::generateReport()
{
    QStringList psdFiles;
    QDirIterator it(m_psdRoot, QStringList() << "*.psd", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        psdFiles << it.next();
    }
    std::sort(psdFiles.begin(), psdFiles.end());

    if (m_limit > 0 && psdFiles.size() > m_limit) {
        psdFiles = psdFiles.mid(0, m_limit);
    }

    QList<Result> results;
    results.reserve(psdFiles.size());

    for (const auto &psdPath : psdFiles) {
        const QFileInfo psdInfo(psdPath);
        const QString relPsdPath = QDir(m_psdRoot).relativeFilePath(psdInfo.absoluteFilePath());
        const QString relNoExt = relPsdPath.left(relPsdPath.lastIndexOf('.'));

        Result result;
        result.fileName = relPsdPath;
        result.fileSize = psdInfo.size();

        const QString imageDataRel = "images/imagedata/" + m_sourceId + "/" + relNoExt + ".png";
        const QString psdViewRel = "images/psdview/" + m_sourceId + "/" + relNoExt + ".png";
        const QString psdViewDiffRel = "images/psdview/" + m_sourceId + "/" + relNoExt + "_diff.png";
        const QString qtQuickRel = "images/qtquick/" + m_sourceId + "/" + relNoExt + ".png";
        const QString qtQuickDiffRel = "images/qtquick/" + m_sourceId + "/" + relNoExt + "_diff.png";
        const QString slintRel = "images/slint/" + m_sourceId + "/" + relNoExt + ".png";
        const QString slintDiffRel = "images/slint/" + m_sourceId + "/" + relNoExt + "_diff.png";
        const QString flutterRel = "images/flutter/" + m_sourceId + "/" + relNoExt + ".png";
        const QString flutterDiffRel = "images/flutter/" + m_sourceId + "/" + relNoExt + "_diff.png";

        result.imageDataRel = imageDataRel;
        result.psdViewRel = psdViewRel;
        result.psdViewDiffRel = psdViewDiffRel;
        result.qtQuickRel = qtQuickRel;
        result.qtQuickDiffRel = qtQuickDiffRel;
        result.slintRel = slintRel;
        result.slintDiffRel = slintDiffRel;
        result.flutterRel = flutterRel;
        result.flutterDiffRel = flutterDiffRel;

        const QString imageDataAbs = m_outputBaseDir + "/" + imageDataRel;
        const QString psdViewAbs = m_outputBaseDir + "/" + psdViewRel;
        const QString psdViewDiffAbs = m_outputBaseDir + "/" + psdViewDiffRel;
        const QString qtQuickAbs = m_outputBaseDir + "/" + qtQuickRel;
        const QString qtQuickDiffAbs = m_outputBaseDir + "/" + qtQuickDiffRel;
        const QString slintAbs = m_outputBaseDir + "/" + slintRel;
        const QString slintDiffAbs = m_outputBaseDir + "/" + slintDiffRel;
        const QString flutterAbs = m_outputBaseDir + "/" + flutterRel;
        const QString flutterDiffAbs = m_outputBaseDir + "/" + flutterDiffRel;

        QImage imageDataImage(imageDataAbs);
        QImage psdViewImage(psdViewAbs);
        if (!m_exportOnly) {
            result.hasImageData = !imageDataImage.isNull();
            result.hasPsdView = !psdViewImage.isNull();

            if (result.hasImageData && result.hasPsdView && imageDataImage.size() == psdViewImage.size()) {
                result.similarityImageDataVsPsdView = compareImages(imageDataImage, psdViewImage);
                result.passedImageDataVsPsdView = result.similarityImageDataVsPsdView > 50.0;
                QDir().mkpath(QFileInfo(psdViewDiffAbs).absolutePath());
                createDiffImage(imageDataImage, psdViewImage).save(psdViewDiffAbs);
            }
        }

        if (m_renderQtQuick) {
            QImage qtQuickImage;

            if (m_runExport) {
                const QString exportDir = m_outputBaseDir + "/exports/" + m_sourceId + "/" + relNoExt + "/QtQuick";
                QString errorMessage;
                if (!exportPsd(psdPath, QStringLiteral("QtQuick"), exportDir, &errorMessage)) {
                    qWarning().noquote() << "QtQuick export failed:" << relPsdPath << "\n" << errorMessage;
                } else {
                    const QString qmlPath = exportDir + "/MainWindow.ui.qml";
                    if (QFileInfo::exists(qmlPath)) {
                        qtQuickImage = renderQtQuick(qmlPath, qtQuickAbs, &errorMessage);
                        if (qtQuickImage.isNull()) {
                            qWarning().noquote() << "QtQuick render failed:" << relPsdPath << "\n" << errorMessage;
                        }
                    } else {
                        qWarning() << "QtQuick output is missing MainWindow.ui.qml:" << relPsdPath;
                    }
                }

                if (!qtQuickImage.isNull()) {
                    // renderQtQuick already writes the file; keep this for local copy consistency.
                    qtQuickImage.save(qtQuickAbs);
                }
            } else {
                qtQuickImage.load(qtQuickAbs);
            }

            if (qtQuickImage.isNull()) {
                qtQuickImage.load(qtQuickAbs);
            }

            result.hasQtQuick = !qtQuickImage.isNull();
            if (!m_exportOnly && result.hasImageData && result.hasQtQuick && imageDataImage.size() == qtQuickImage.size()) {
                result.similarityImageDataVsQtQuick = compareImages(imageDataImage, qtQuickImage);
                result.passedImageDataVsQtQuick = result.similarityImageDataVsQtQuick > 50.0;
                QDir().mkpath(QFileInfo(qtQuickDiffAbs).absolutePath());
                createDiffImage(imageDataImage, qtQuickImage).save(qtQuickDiffAbs);
            }
        }

        if (m_renderSlint) {
            QImage slintImage;

            if (m_runExport) {
                const QString exportDir = m_outputBaseDir + "/exports/" + m_sourceId + "/" + relNoExt + "/Slint";
                QString errorMessage;
                if (!exportPsd(psdPath, QStringLiteral("Slint"), exportDir, &errorMessage)) {
                    qWarning().noquote() << "Slint export failed:" << relPsdPath << "\n" << errorMessage;
                } else {
                    const QString slintPath = exportDir + "/MainWindow.slint";
                    if (QFileInfo::exists(slintPath)) {
                        slintImage = renderSlint(slintPath, slintAbs, &errorMessage);
                        if (slintImage.isNull()) {
                            qWarning().noquote() << "Slint render failed:" << relPsdPath << "\n" << errorMessage;
                        }
                    } else {
                        qWarning() << "Slint output is missing MainWindow.slint:" << relPsdPath;
                    }
                }
            } else {
                slintImage.load(slintAbs);
            }

            if (slintImage.isNull()) {
                slintImage.load(slintAbs);
            }

            result.hasSlint = !slintImage.isNull();
            if (!m_exportOnly && result.hasImageData && result.hasSlint && imageDataImage.size() == slintImage.size()) {
                result.similarityImageDataVsSlint = compareImages(imageDataImage, slintImage);
                result.passedImageDataVsSlint = result.similarityImageDataVsSlint > 50.0;
                QDir().mkpath(QFileInfo(slintDiffAbs).absolutePath());
                createDiffImage(imageDataImage, slintImage).save(slintDiffAbs);
            }
        }

        if (m_renderFlutter) {
            QImage flutterImage;

            if (m_runExport) {
                const QString exportDir = m_outputBaseDir + "/exports/" + m_sourceId + "/" + relNoExt + "/Flutter";
                QString errorMessage;
                if (!exportPsd(psdPath, QStringLiteral("Flutter"), exportDir, &errorMessage)) {
                    qWarning().noquote() << "Flutter export failed:" << relPsdPath << "\n" << errorMessage;
                } else {
                    const QString dartPath = exportDir + "/main_window.dart";
                    if (QFileInfo::exists(dartPath)) {
                        flutterImage = renderFlutter(dartPath, flutterAbs, &errorMessage);
                        if (flutterImage.isNull()) {
                            qWarning().noquote() << "Flutter render failed:" << relPsdPath << "\n" << errorMessage;
                        }
                    } else {
                        qWarning() << "Flutter output is missing main_window.dart:" << relPsdPath;
                    }
                }
            } else {
                flutterImage.load(flutterAbs);
            }

            if (flutterImage.isNull()) {
                flutterImage.load(flutterAbs);
            }

            result.hasFlutter = !flutterImage.isNull();
            if (!m_exportOnly && result.hasImageData && result.hasFlutter && imageDataImage.size() == flutterImage.size()) {
                result.similarityImageDataVsFlutter = compareImages(imageDataImage, flutterImage);
                result.passedImageDataVsFlutter = result.similarityImageDataVsFlutter > 50.0;
                QDir().mkpath(QFileInfo(flutterDiffAbs).absolutePath());
                createDiffImage(imageDataImage, flutterImage).save(flutterDiffAbs);
            }
        }

        results.append(result);
    }

    if (m_exportOnly) {
        qInfo() << "Export-only mode finished. Captures were updated in" << (m_outputBaseDir + "/images");
        return;
    }

    writeReport(results);
    qInfo() << "Similarity report written to" << m_reportPath;
}

QTEST_MAIN(tst_PsdExporterSimilarity)

#include "tst_psdexporter_similarity.moc"

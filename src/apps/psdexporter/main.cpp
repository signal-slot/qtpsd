// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include <QtWidgets/QApplication>
#include "mainwindow.h"
#include "psdtreeitemmodel.h"

#include <QtCore/QCommandLineParser>
#include <QtCore/QLoggingCategory>
#include <QtCore/QRegularExpression>
#include <QtPsdExporter/QPsdExporterPlugin>
#include <QtPsdWidget/QPsdWidgetTreeItemModel>

static QSize parseResolution(const QString &resolution, const QSize &originalSize)
{
    static const QHash<QString, QSize> presets = {
        {"original", QSize()},
        {"4k", QSize(3840, 2160)},
        {"fhd", QSize(1920, 1080)},
        {"hd", QSize(1280, 720)},
        {"xga", QSize(1024, 768)},
        {"svga", QSize(800, 600)},
        {"vga", QSize(640, 480)},
        {"qvga", QSize(320, 240)},
    };

    QString lower = resolution.toLower();
    if (presets.contains(lower)) {
        QSize size = presets.value(lower);
        return size.isEmpty() ? originalSize : size;
    }

    // Try to parse as WIDTHxHEIGHT
    QRegularExpression re("^(\\d+)x(\\d+)$");
    auto match = re.match(resolution);
    if (match.hasMatch()) {
        return QSize(match.captured(1).toInt(), match.captured(2).toInt());
    }

    return QSize(); // Invalid
}

static int runAutoExport(const QString &input, const QString &type, const QString &outdir, const QString &resolution)
{
    QFileInfo inputInfo(input);
    if (!inputInfo.exists()) {
        qCritical() << "Input file does not exist:" << input;
        return 1;
    }

    auto plugin = QPsdExporterPlugin::plugin(type.toUtf8());
    if (!plugin) {
        qCritical() << "Unknown exporter type:" << type;
        qCritical() << "Available types:" << QPsdExporterPlugin::keys();
        return 1;
    }

    if (plugin->exportType() != QPsdExporterPlugin::Directory) {
        qCritical() << "Exporter" << type << "does not support directory export";
        return 1;
    }

    QDir outDir(outdir);
    if (!outDir.exists()) {
        if (!outDir.mkpath(".")) {
            qCritical() << "Failed to create output directory:" << outdir;
            return 1;
        }
    }

    QPsdWidgetTreeItemModel widgetModel;
    PsdTreeItemModel model;
    model.setSourceModel(&widgetModel);
    model.load(input);

    if (!model.errorMessage().isEmpty()) {
        qCritical() << "Failed to load PSD file:" << model.errorMessage();
        return 1;
    }

    QSize outputSize = parseResolution(resolution, model.size());
    if (outputSize.isEmpty()) {
        qCritical() << "Invalid resolution:" << resolution;
        qCritical() << "Use preset (original, 4k, fhd, hd, xga, svga, vga, qvga) or WIDTHxHEIGHT format";
        return 1;
    }

    QVariantMap hint;
    hint.insert("width", outputSize.width());
    hint.insert("height", outputSize.height());
    hint.insert("fontScaleFactor", 1.0);
    hint.insert("imageScaling", false);
    hint.insert("makeCompact", false);

    qInfo() << "Exporting" << input << "to" << outdir << "using" << type << "at" << outputSize;
    if (!plugin->exportTo(&model, outdir, hint)) {
        qCritical() << "Export failed";
        return 1;
    }

    qInfo() << "Export completed successfully";
    return 0;
}

int main(int argc, char *argv[])
{
    qSetMessagePattern("[%{if-debug}D%{endif}%{if-info}I%{endif}%{if-warning}W%{endif}%{if-critical}C%{endif}%{if-fatal}F%{endif}] %{function}:%{line} - %{message}");
    // QLoggingCategory::setFilterRules("qt.psdcore.effectslayer.debug=true");

    QCoreApplication::setOrganizationName("Signal Slot Inc.");
    QCoreApplication::setOrganizationDomain("signal-slot.co.jp");
    QCoreApplication::setApplicationName("PsdViewer");

    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("PSD Viewer and Exporter");
    parser.addHelpOption();

    QCommandLineOption inputOption(QStringList() << "input",
                                   "Input PSD file for auto export",
                                   "file");
    parser.addOption(inputOption);

    QCommandLineOption typeOption(QStringList() << "type",
                                  "Exporter type (e.g., qtquick, flutter, swiftui)",
                                  "type");
    parser.addOption(typeOption);

    QCommandLineOption outdirOption(QStringList() << "outdir",
                                    "Output directory for auto export",
                                    "directory");
    parser.addOption(outdirOption);

    QCommandLineOption resolutionOption(QStringList() << "resolution",
                                        "Output resolution: preset (original, 4k, fhd, hd, xga, svga, vga, qvga) or WIDTHxHEIGHT",
                                        "resolution",
                                        "original");
    parser.addOption(resolutionOption);

    QCommandLineOption listOption(QStringList() << "list",
                                  "List available exporter types");
    parser.addOption(listOption);

    parser.process(app);

    if (parser.isSet(listOption)) {
        qInfo() << "Available exporter types:";
        for (const auto &key : QPsdExporterPlugin::keys()) {
            auto plugin = QPsdExporterPlugin::plugin(key);
            QString typeStr = plugin->exportType() == QPsdExporterPlugin::Directory ? "Directory" : "File";
            qInfo().noquote() << "  " << key << "-" << plugin->name() << "(" << typeStr << ")";
        }
        return 0;
    }

    bool hasInput = parser.isSet(inputOption);
    bool hasType = parser.isSet(typeOption);
    bool hasOutdir = parser.isSet(outdirOption);

    if (hasInput || hasType || hasOutdir) {
        if (!hasInput || !hasType || !hasOutdir) {
            qCritical() << "Auto export requires all three options: -input, -type, -outdir";
            return 1;
        }
        return runAutoExport(parser.value(inputOption),
                             parser.value(typeOption),
                             parser.value(outdirOption),
                             parser.value(resolutionOption));
    }

    MainWindow window;
    window.show();
    return app.exec();
}


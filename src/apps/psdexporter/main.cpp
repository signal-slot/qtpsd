// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include <QtWidgets/QApplication>
#include "mainwindow.h"
#include "psdtreeitemmodel.h"

#include <QtCore/QCommandLineParser>
#include <QtCore/QLoggingCategory>
#include <QtPsdExporter/QPsdExporterPlugin>
#include <QtPsdWidget/QPsdWidgetTreeItemModel>

static int runAutoExport(const QString &input, const QString &type, const QString &outdir)
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

    QVariantMap hint;
    hint.insert("width", model.size().width());
    hint.insert("height", model.size().height());
    hint.insert("fontScaleFactor", 1.0);
    hint.insert("imageScaling", false);
    hint.insert("makeCompact", false);

    qInfo() << "Exporting" << input << "to" << outdir << "using" << type;
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
                             parser.value(outdirOption));
    }

    MainWindow window;
    window.show();
    return app.exec();
}


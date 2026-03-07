// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include <QtWidgets/QApplication>
#include "mainwindow.h"
#include "psdtreeitemmodel.h"

#include <QtCore/QCommandLineParser>
#include <QtCore/QFileInfo>
#include <QtCore/QMetaProperty>
#include <QtCore/QLoggingCategory>
#include <QtCore/QRegularExpression>
#include <QtCore/QSettings>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtPsdExporter/QPsdExporterPlugin>
#include <QtPsdGui/QPsdFolderLayerItem>
#include <QtPsdImporter/QPsdImporterPlugin>
#include <QtPsdWidget/QPsdWidgetTreeItemModel>

static QSize parseResolution(const QString &resolution, bool *ok = nullptr)
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
        if (ok) *ok = true;
        return presets.value(lower);
    }

    // Try to parse as WIDTHxHEIGHT
    QRegularExpression re("^(\\d+)x(\\d+)$");
    auto match = re.match(resolution);
    if (match.hasMatch()) {
        if (ok) *ok = true;
        return QSize(match.captured(1).toInt(), match.captured(2).toInt());
    }

    if (ok) *ok = false;
    return QSize(); // Invalid
}

static int runAutoExport(const QString &input, const QString &type, const QString &outdir, const QString &resolution, const QStringList &propertyArgs, bool makeCompact, bool artboardToOrigin, const QString &licenseText)
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

    // Apply plugin-specific properties from CLI (e.g., --effect-mode nogpu)
    const auto mo = plugin->metaObject();
    for (const auto &arg : propertyArgs) {
        const auto parts = arg.split('=');
        if (parts.size() != 2) continue;
        const auto propName = parts.at(0).toUtf8();
        const auto propValue = parts.at(1);
        int idx = mo->indexOfProperty(propName.constData());
        if (idx < 0) {
            qCritical() << "Unknown property:" << propName << "for exporter" << type;
            return 1;
        }
        const auto prop = mo->property(idx);
        if (prop.isEnumType()) {
            const auto enumerator = prop.enumerator();
            bool found = false;
            for (int j = 0; j < enumerator.keyCount(); j++) {
                if (propValue.compare(QString::fromLatin1(enumerator.key(j)), Qt::CaseInsensitive) == 0) {
                    prop.write(plugin, enumerator.value(j));
                    found = true;
                    break;
                }
            }
            if (!found) {
                QStringList validValues;
                for (int j = 0; j < enumerator.keyCount(); j++)
                    validValues << QString::fromLatin1(enumerator.key(j)).toLower();
                qCritical() << "Invalid value:" << propValue << "for property" << propName;
                qCritical() << "Valid values:" << validValues.join(", ");
                return 1;
            }
        } else if (prop.typeId() == QMetaType::Bool) {
            prop.write(plugin, QVariant(propValue).toBool());
        } else {
            prop.write(plugin, propValue);
        }
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

    // "original" → empty targetSize so initializeExport uses effectiveCanvasSize
    bool resOk = false;
    QSize outputSize = parseResolution(resolution, &resOk);
    if (!resOk) {
        qCritical() << "Invalid resolution:" << resolution;
        qCritical() << "Use preset (original, 4k, fhd, hd, xga, svga, vga, qvga) or WIDTHxHEIGHT format";
        return 1;
    }

    QPsdExporterPlugin::ExportConfig config;
    config.targetSize = outputSize;
    config.fontScaleFactor = 1.0;
    config.makeCompact = makeCompact;
    config.imageScaling = false;
    config.artboardToOrigin = artboardToOrigin;
    config.licenseText = licenseText;

    // For File-type exporters, construct the output file path from outdir
    QString exportTarget = outdir;
    if (plugin->exportType() == QPsdExporterPlugin::File) {
        const auto filters = plugin->filters();
        QString extension = filters.isEmpty() ? ".out"_L1 : filters.values().at(0);
        exportTarget = outDir.filePath(inputInfo.completeBaseName() + extension);
    }

    qInfo() << "Exporting" << input << "to" << exportTarget << "using" << type << "at" << outputSize;
    if (!plugin->exportTo(&model, exportTarget, config)) {
        qCritical() << "Export failed";
        return 1;
    }

    qInfo() << "Export completed successfully";
    return 0;
}

static int runAutoImport(const QString &importType, const QString &source, const QString &apiKey, int imageScale, const QList<int> &pageIndices,
                         const QString &exportType = {}, const QString &outdir = {}, const QStringList &propertyArgs = {},
                         const QString &resolution = "original"_L1, bool makeCompact = false, bool artboardToOrigin = false, const QString &licenseText = {})
{
    auto plugin = QPsdImporterPlugin::plugin(importType.toUtf8());
    if (!plugin) {
        qCritical() << "Unknown importer type:" << importType;
        qCritical() << "Available types:" << QPsdImporterPlugin::keys();
        return 1;
    }

    QVariantMap baseOptions;
    baseOptions["source"] = source;

    QString key = apiKey;
    if (key.isEmpty()) {
        QSettings settings;
        settings.beginGroup("Importers/Figma");
        key = settings.value("apiKey").toString();
    }
    if (key.isEmpty())
        key = qEnvironmentVariable("FIGMA_API_KEY");
    if (key.isEmpty())
        key = qEnvironmentVariable("FIGMA_ACCESS_TOKEN");
    if (!key.isEmpty())
        baseOptions["apiKey"] = key;
    baseOptions["imageScale"] = imageScale;

    for (int pageIdx : pageIndices) {
        QPsdWidgetTreeItemModel widgetModel;
        PsdTreeItemModel model;
        model.setSourceModel(&widgetModel);

        QVariantMap options = baseOptions;
        options["pageIndex"] = pageIdx;

        qInfo() << "Importing from" << source << "page" << pageIdx << "using" << importType;
        if (!plugin->importFrom(&model, options)) {
            const QString err = plugin->errorMessage();
            qCritical() << "Import failed:" << (err.isEmpty() ? u"unknown error"_s : err);
            return 1;
        }

        qInfo() << "Import completed successfully";
        qInfo() << "File name:" << model.fileName();
        qInfo() << "Size:" << model.size();

        // Export if --type and --outdir were also specified
        if (!exportType.isEmpty() && !outdir.isEmpty()) {
            auto exporter = QPsdExporterPlugin::plugin(exportType.toUtf8());
            if (!exporter) {
                qCritical() << "Unknown exporter type:" << exportType;
                return 1;
            }

            // Apply plugin properties
            const auto *mo = exporter->metaObject();
            for (const auto &arg : propertyArgs) {
                const auto parts = arg.split('='_L1);
                if (parts.size() != 2) continue;
                const auto propName = parts[0].toUtf8();
                const auto propValue = parts[1];
                int idx = mo->indexOfProperty(propName.constData());
                if (idx < 0) continue;
                const auto prop = mo->property(idx);
                if (prop.isEnumType()) {
                    const auto enumerator = prop.enumerator();
                    for (int j = 0; j < enumerator.keyCount(); j++) {
                        if (propValue.compare(QString::fromLatin1(enumerator.key(j)), Qt::CaseInsensitive) == 0) {
                            prop.write(exporter, enumerator.value(j));
                            break;
                        }
                    }
                } else if (prop.typeId() == QMetaType::Bool) {
                    prop.write(exporter, QVariant(propValue).toBool());
                } else {
                    prop.write(exporter, propValue);
                }
            }

            QDir outDir(outdir);
            if (!outDir.exists())
                outDir.mkpath(".");

            // "original" → empty targetSize so initializeExport uses effectiveCanvasSize
            QSize outputSize = parseResolution(resolution);

            QPsdExporterPlugin::ExportConfig config;
            config.targetSize = outputSize;
            config.fontScaleFactor = 1.0;
            config.makeCompact = makeCompact;
            config.imageScaling = false;
            config.artboardToOrigin = artboardToOrigin;
            config.licenseText = licenseText;

            QString exportTarget = outdir;
            if (exporter->exportType() == QPsdExporterPlugin::File) {
                const auto filters = exporter->filters();
                QString extension = filters.isEmpty() ? ".out"_L1 : filters.values().at(0);
                exportTarget = outDir.filePath(model.fileName() + extension);
            }

            qInfo() << "Exporting to" << exportTarget << "using" << exportType;
            if (!exporter->exportTo(&model, exportTarget, config)) {
                qCritical() << "Export failed";
                return 1;
            }
            qInfo() << "Export completed successfully";
        }
    }

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

    QCommandLineOption effectModeOption(QStringList() << "effect-mode",
                                        "Effect rendering mode (nogpu, qt5effects, effectmaker)",
                                        "mode");
    parser.addOption(effectModeOption);

    QCommandLineOption makeCompactOption(QStringList() << "make-compact",
                                         "Enable compact layout");
    parser.addOption(makeCompactOption);

    QCommandLineOption artboardOriginOption(QStringList() << "artboard-to-origin",
                                            "Move artboards to (0,0) and fit window");
    parser.addOption(artboardOriginOption);

    QCommandLineOption licenseTextOption(QStringList() << "license-text",
                                         "License text to prepend to generated files",
                                         "text");
    parser.addOption(licenseTextOption);

    QCommandLineOption listOption(QStringList() << "list",
                                  "List available exporter types");
    parser.addOption(listOption);

    QCommandLineOption importTypeOption(QStringList() << "import-type",
                                        "Importer type (e.g., figma)",
                                        "type");
    parser.addOption(importTypeOption);

    QCommandLineOption importSourceOption(QStringList() << "import-source",
                                          "Import source (e.g., Figma URL)",
                                          "source");
    parser.addOption(importSourceOption);

    QCommandLineOption importApiKeyOption(QStringList() << "import-api-key",
                                          "API key for import (or set FIGMA_API_KEY env var)",
                                          "key");
    parser.addOption(importApiKeyOption);

    QCommandLineOption importScaleOption(QStringList() << "import-scale",
                                         "Image scale for import (default: 2)",
                                         "scale",
                                         "2");
    parser.addOption(importScaleOption);

    QCommandLineOption importPageIndexOption(QStringList() << "import-page-index",
                                             "Page index(es) for import (comma-separated, default: 0)",
                                             "indices",
                                             "0");
    parser.addOption(importPageIndexOption);

    parser.addPositionalArgument("urls", "PSD files or URLs to open", "[file-or-url...]");

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

    // Handle --import-type / --import-source
    if (parser.isSet(importTypeOption) || parser.isSet(importSourceOption)) {
        if (!parser.isSet(importTypeOption) || !parser.isSet(importSourceOption)) {
            qCritical() << "Auto import requires both: --import-type and --import-source";
            return 1;
        }
        // Parse comma-separated page indices
        QList<int> pageIndices;
        const auto parts = parser.value(importPageIndexOption).split(','_L1, Qt::SkipEmptyParts);
        for (const auto &part : parts)
            pageIndices.append(part.trimmed().toInt());
        if (pageIndices.isEmpty())
            pageIndices.append(0);
        QStringList propertyArgs;
        if (parser.isSet(effectModeOption))
            propertyArgs << u"effectMode=%1"_s.arg(parser.value(effectModeOption));
        return runAutoImport(parser.value(importTypeOption),
                             parser.value(importSourceOption),
                             parser.value(importApiKeyOption),
                             parser.value(importScaleOption).toInt(),
                             pageIndices,
                             parser.value(typeOption),
                             parser.value(outdirOption),
                             propertyArgs,
                             parser.value(resolutionOption),
                             parser.isSet(makeCompactOption),
                             parser.isSet(artboardOriginOption),
                             parser.value(licenseTextOption));
    }

    bool hasInput = parser.isSet(inputOption);
    bool hasType = parser.isSet(typeOption);
    bool hasOutdir = parser.isSet(outdirOption);

    if (hasInput || hasType || hasOutdir) {
        if (!hasInput || !hasType || !hasOutdir) {
            qCritical() << "Auto export requires all three options: -input, -type, -outdir";
            return 1;
        }
        QStringList propertyArgs;
        if (parser.isSet(effectModeOption))
            propertyArgs << u"effectMode=%1"_s.arg(parser.value(effectModeOption));
        return runAutoExport(parser.value(inputOption),
                             parser.value(typeOption),
                             parser.value(outdirOption),
                             parser.value(resolutionOption),
                             propertyArgs,
                             parser.isSet(makeCompactOption),
                             parser.isSet(artboardOriginOption),
                             parser.value(licenseTextOption));
    }

    MainWindow window;
    window.show();

    const auto positionalArgs = parser.positionalArguments();
    for (const auto &arg : positionalArgs) {
        const QUrl url(arg, QUrl::TolerantMode);
        if (url.scheme() == "http"_L1 || url.scheme() == "https"_L1) {
            // URL-based import — needs event loop for network I/O
            QTimer::singleShot(0, &window, [&window, url]() {
                window.importFromUrl(url);
            });
        } else {
            // Local file path (file:// scheme or no scheme)
            const QString path = url.isLocalFile() ? url.toLocalFile() : arg;
            window.openFile(QFileInfo(path).absoluteFilePath());
        }
    }

    return app.exec();
}


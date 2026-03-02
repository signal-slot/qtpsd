// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include <QtCore/QCommandLineParser>
#include <QtCore/QLoggingCategory>
#include <QtCore/QSettings>
#include <QtWidgets/QApplication>
#include <QtGui/QPainter>

#include <QtPsdWidget/QPsdWidgetTreeItemModel>
#include <QtPsdWidget/QPsdScene>
#include <QtPsdExporter/QPsdExporterTreeItemModel>
#include <QtPsdImporter/QPsdImporterPlugin>

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("Signal Slot Inc."_L1);
    QCoreApplication::setOrganizationDomain("signal-slot.co.jp"_L1);
    QCoreApplication::setApplicationName("PsdViewer"_L1);
    QCoreApplication::setApplicationVersion("0.1.0"_L1);

    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Render Figma design to PNG"_L1);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{"k"_L1, "api-key"_L1}, "Figma API key (or set FIGMA_API_KEY env var)"_L1, "key"_L1});
    parser.addOption({{"s"_L1, "scale"_L1}, "Image scale (default: 2)"_L1, "scale"_L1, "2"_L1});
    parser.addOption({{"r"_L1, "render-scale"_L1}, "Render scale factor (default: 1)"_L1, "render-scale"_L1, "1"_L1});
    parser.addPositionalArgument("url"_L1, "Figma design URL"_L1);
    parser.addPositionalArgument("output"_L1, "Output PNG file"_L1);
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 2) {
        parser.showHelp(1);
        Q_UNREACHABLE_RETURN(1);
    }

    const QString figmaUrl = args.at(0);
    const QString outputFile = args.at(1);

    // Find figma importer plugin
    auto *importer = QPsdImporterPlugin::plugin("figma");
    if (!importer) {
        qCritical() << "Figma importer plugin not found";
        qCritical() << "Available importers:" << QPsdImporterPlugin::keys();
        return 1;
    }

    // Build options
    QVariantMap options;
    options["source"_L1] = figmaUrl;
    options["imageScale"_L1] = parser.value("scale"_L1).toInt();

    QString apiKey = parser.value("api-key"_L1);
    if (apiKey.isEmpty()) {
        QSettings settings;
        settings.beginGroup("Importers/Figma"_L1);
        apiKey = settings.value("apiKey"_L1).toString();
    }
    if (apiKey.isEmpty())
        apiKey = qEnvironmentVariable("FIGMA_API_KEY");
    if (apiKey.isEmpty())
        apiKey = qEnvironmentVariable("FIGMA_ACCESS_TOKEN");
    if (apiKey.isEmpty()) {
        qCritical() << "No API key provided. Use -k, set FIGMA_API_KEY env var, or import once from the GUI to save it.";
        return 1;
    }
    options["apiKey"_L1] = apiKey;

    // Import
    QPsdExporterTreeItemModel model;
    QPsdWidgetTreeItemModel widgetModel;
    model.setSourceModel(&widgetModel);

    qInfo() << "Importing from Figma...";
    if (!importer->importFrom(&model, options)) {
        qCritical() << "Figma import failed";
        return 1;
    }

    qInfo() << "File:" << model.fileName();
    qInfo() << "Size:" << model.size();

    // Render
    auto *wm = dynamic_cast<QPsdWidgetTreeItemModel *>(model.sourceModel());
    if (!wm) {
        qCritical() << "No widget model after import";
        return 1;
    }

    QPsdScene scene;
    scene.setModel(wm);

    const int renderScale = parser.value("render-scale"_L1).toInt();
    const QSize size = model.size() * renderScale;
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    const QColor canvasColor = model.canvasColor();
    image.fill(canvasColor.isValid() && canvasColor.alpha() > 0
               ? canvasColor.rgba() : qRgba(0, 0, 0, 0));

    QPainter painter(&image);
    scene.render(&painter, QRectF(QPointF(), size), scene.sceneRect());
    painter.end();

    if (!image.save(outputFile)) {
        qCritical() << "Error saving PNG:" << outputFile;
        return 1;
    }

    qInfo() << "Saved:" << outputFile;
    return 0;
}

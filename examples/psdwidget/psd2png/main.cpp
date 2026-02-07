// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include <QtCore/QCommandLineParser>
#include <QtCore/QLoggingCategory>
#include <QtWidgets/QApplication>
#include <QtGui/QPainter>

#include <QtPsdWidget/QPsdWidgetTreeItemModel>
#include <QtPsdWidget/QPsdScene>
#include <QtPsdGui/QPsdAbstractLayerItem>

using namespace Qt::StringLiterals;

static void printLayerInfo(QPsdWidgetTreeItemModel *model, const QModelIndex &parent, int depth = 0, bool verbose = false)
{
    for (int row = 0; row < model->rowCount(parent); ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        const QString name = model->layerName(index);
        const QPsdAbstractLayerItem *item = model->layerItem(index);
        const QPsdLayerRecord *record = model->layerRecord(index);
        const QString indent = QString(depth * 2, u' ');
        if (item) {
            qInfo().noquote() << indent + QString("Layer: %1, opacity: %2, fillOpacity: %3")
                .arg(name).arg(item->opacity()).arg(item->fillOpacity());
        } else {
            qInfo().noquote() << indent + QString("Layer: %1 (no item)").arg(name);
        }
        if (record) {
            const auto info = record->additionalLayerInformation();
            if (info.contains("iOpa")) {
                const quint8 fillOpacity = info.value("iOpa").value<quint8>();
                qInfo().noquote() << indent + QString("  Fill opacity (iOpa): %1 (%2%)")
                    .arg(fillOpacity).arg(fillOpacity * 100 / 255);
            }
            if (verbose) {
                const auto keys = info.keys();
                qInfo().noquote() << indent + QString("  Additional info keys: %1").arg(QString::fromLatin1(keys.join(", ")));
            }
        }
        printLayerInfo(model, index, depth + 1, verbose);
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("Signal Slot Inc."_L1);
    QCoreApplication::setOrganizationDomain("signal-slot.co.jp"_L1);
    QCoreApplication::setApplicationName("psd2png"_L1);
    QCoreApplication::setApplicationVersion("0.1.0"_L1);

    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Render PSD to PNG"_L1);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"verbose"_L1, "Enable verbose logging"_L1});
    parser.addOption({{"l"_L1, "layers"_L1}, "Print layer information"_L1});
    parser.addOption({{"i"_L1, "image-data"_L1}, "Generate PNG from baked image data instead of compositing layers"_L1});
    parser.addPositionalArgument("input"_L1, "Input PSD file"_L1);
    parser.addPositionalArgument("output"_L1, "Output PNG file"_L1);
    parser.process(app);

    if (parser.isSet("verbose"_L1)) {
        QLoggingCategory::setFilterRules("qt.psdcore.*=true"_L1);
    }

    const QStringList args = parser.positionalArguments();
    if (args.size() != 2) {
        parser.showHelp(1);
        Q_UNREACHABLE_RETURN(1);
    }

    const QString inputFile = args.at(0);
    const QString outputFile = args.at(1);

    QPsdWidgetTreeItemModel model;
    model.load(inputFile);

    if (model.errorMessage().isEmpty() == false) {
        qCritical() << "Error loading PSD:" << model.errorMessage();
        return 1;
    }

    if (parser.isSet("layers"_L1)) {
        printLayerInfo(&model, QModelIndex(), 0, parser.isSet("verbose"_L1));
    }

    QImage image;
    if (parser.isSet("image-data"_L1)) {
        image = model.mergedImage();
        if (image.isNull()) {
            qCritical() << "No image data found in PSD file";
            return 1;
        }
    } else {
        QPsdScene scene;
        scene.setModel(&model);

        const QSize size = model.size();
        image = QImage(size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);

        QPainter painter(&image);
        scene.render(&painter);
        painter.end();
    }

    if (!image.save(outputFile)) {
        qCritical() << "Error saving PNG:" << outputFile;
        return 1;
    }

    qInfo() << "Saved:" << outputFile;
    return 0;
}

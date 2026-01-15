// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include <QtCore/QCommandLineParser>
#include <QtWidgets/QApplication>
#include <QtGui/QPainter>

#include <QtPsdWidget/QPsdWidgetTreeItemModel>
#include <QtPsdWidget/QPsdScene>

using namespace Qt::StringLiterals;

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
    parser.addPositionalArgument("input"_L1, "Input PSD file"_L1);
    parser.addPositionalArgument("output"_L1, "Output PNG file"_L1);
    parser.process(app);

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

    QPsdScene scene;
    scene.setModel(&model);

    const QSize size = model.size();
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    scene.render(&painter);
    painter.end();

    if (!image.save(outputFile)) {
        qCritical() << "Error saving PNG:" << outputFile;
        return 1;
    }

    qInfo() << "Saved:" << outputFile;
    return 0;
}

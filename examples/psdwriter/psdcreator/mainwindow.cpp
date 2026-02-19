// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "mainwindow.h"
#include "canvas.h"
#include "layermodel.h"

#include <QtCore/QBuffer>
#include <QtCore/QCborMap>
#include <QtCore/QCborArray>
#include <QtGui/QActionGroup>
#include <QtWidgets/QApplication>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFontComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>

#include <QtPsdCore/QPsdFileHeader>
#include <QtPsdCore/QPsdLayerAndMaskInformation>
#include <QtPsdCore/QPsdLayerInfo>
#include <QtPsdCore/QPsdLayerRecord>
#include <QtPsdCore/QPsdChannelInfo>
#include <QtPsdCore/QPsdChannelImageData>
#include <QtPsdCore/QPsdImageData>
#include <QtPsdCore/QPsdParser>
#include <QtPsdCore/QPsdSectionDividerSetting>
#include <QtPsdCore/QPsdTypeToolObjectSetting>
#include <QtPsdCore/QPsdDescriptor>
#include <QtPsdCore/QPsdEngineDataParser>
#include <QtPsdCore/QPsdEnum>
#include <QtPsdWriter/QPsdWriter>
#include <QtPsdGui/QPsdGuiLayerTreeItemModel>
#include <QtPsdGui/QPsdAbstractLayerItem>
#include <QtPsdGui/QPsdTextLayerItem>
#include <QtPsdGui/qpsdguiglobal.h>

using namespace Qt::StringLiterals;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(u"PSD Creator"_s);
    resize(1024, 768);

    m_model = new LayerModel(this);
    m_canvas = new Canvas(m_model, this);
    setCentralWidget(m_canvas);

    setupMenus();
    setupToolbar();
    setupLayersPanel();

    // Start with a default canvas and one layer
    const auto idx = m_model->addLayer(u"Background"_s);
    m_canvas->setActiveLayer(idx);
    m_canvas->updateLayers();

    connect(m_canvas, &Canvas::modified, this, [this]() {
        m_canvas->updateLayers();
    });
    connect(m_model, &LayerModel::layerChanged, this, [this]() {
        m_canvas->updateLayers();
    });
}

void MainWindow::setupMenus()
{
    auto *fileMenu = menuBar()->addMenu(u"&File"_s);

    auto *newAction = fileMenu->addAction(u"&New..."_s);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newFile);

    auto *openAction = fileMenu->addAction(u"&Open..."_s);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    fileMenu->addSeparator();

    auto *saveAction = fileMenu->addAction(u"Save &As..."_s);
    saveAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);

    fileMenu->addSeparator();

    auto *exitAction = fileMenu->addAction(u"E&xit"_s);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::setupToolbar()
{
    auto *toolbar = addToolBar(u"Tools"_s);

    auto *brushAction = toolbar->addAction(u"Brush"_s);
    brushAction->setCheckable(true);
    brushAction->setChecked(true);
    connect(brushAction, &QAction::triggered, this, [this, brushAction]() {
        m_canvas->setTool(Canvas::BrushTool);
        brushAction->setChecked(true);
    });

    auto *eraserAction = toolbar->addAction(u"Eraser"_s);
    eraserAction->setCheckable(true);
    connect(eraserAction, &QAction::triggered, this, [this, eraserAction]() {
        m_canvas->setTool(Canvas::EraserTool);
        eraserAction->setChecked(true);
    });

    auto *fillAction = toolbar->addAction(u"Fill"_s);
    fillAction->setCheckable(true);
    connect(fillAction, &QAction::triggered, this, [this, fillAction]() {
        m_canvas->setTool(Canvas::FillTool);
        fillAction->setChecked(true);
    });

    auto *toolGroup = new QActionGroup(this);
    toolGroup->addAction(brushAction);
    toolGroup->addAction(eraserAction);
    toolGroup->addAction(fillAction);
    toolGroup->setExclusive(true);

    toolbar->addSeparator();

    auto *colorButton = new QPushButton(u"Color"_s, this);
    colorButton->setFixedSize(60, 28);
    connect(colorButton, &QPushButton::clicked, this, &MainWindow::pickColor);
    toolbar->addWidget(colorButton);

    toolbar->addSeparator();

    auto *sizeLabel = new QLabel(u"Size:"_s, this);
    toolbar->addWidget(sizeLabel);

    m_brushSizeSpin = new QSpinBox(this);
    m_brushSizeSpin->setRange(1, 100);
    m_brushSizeSpin->setValue(5);
    connect(m_brushSizeSpin, &QSpinBox::valueChanged, m_canvas, &Canvas::setBrushSize);
    toolbar->addWidget(m_brushSizeSpin);
}

void MainWindow::setupLayersPanel()
{
    auto *dock = new QDockWidget(u"Layers"_s, this);
    dock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);

    auto *panel = new QWidget(dock);
    auto *layout = new QVBoxLayout(panel);

    // Layer tree view
    m_layerTree = new QTreeView(panel);
    m_layerTree->setModel(m_model);
    m_layerTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layerTree->setHeaderHidden(true);
    m_layerTree->setExpandsOnDoubleClick(false); // We handle double-click ourselves
    connect(m_layerTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onLayerSelectionChanged);
    connect(m_layerTree, &QTreeView::doubleClicked,
            this, &MainWindow::onLayerDoubleClicked);
    layout->addWidget(m_layerTree);

    // Opacity slider
    auto *opacityLayout = new QHBoxLayout;
    opacityLayout->addWidget(new QLabel(u"Opacity:"_s, panel));
    m_opacitySlider = new QSlider(Qt::Horizontal, panel);
    m_opacitySlider->setRange(0, 255);
    m_opacitySlider->setValue(255);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &MainWindow::onOpacityChanged);
    opacityLayout->addWidget(m_opacitySlider);
    m_opacityLabel = new QLabel(u"255"_s, panel);
    m_opacityLabel->setFixedWidth(30);
    opacityLayout->addWidget(m_opacityLabel);
    layout->addLayout(opacityLayout);

    // Blend mode combo
    auto *blendLayout = new QHBoxLayout;
    blendLayout->addWidget(new QLabel(u"Blend:"_s, panel));
    m_blendCombo = new QComboBox(panel);
    m_blendCombo->addItem(u"Normal"_s, static_cast<int>(QPsdBlend::Normal));
    m_blendCombo->addItem(u"Multiply"_s, static_cast<int>(QPsdBlend::Multiply));
    m_blendCombo->addItem(u"Screen"_s, static_cast<int>(QPsdBlend::Screen));
    m_blendCombo->addItem(u"Overlay"_s, static_cast<int>(QPsdBlend::Overlay));
    m_blendCombo->addItem(u"Darken"_s, static_cast<int>(QPsdBlend::Darken));
    m_blendCombo->addItem(u"Lighten"_s, static_cast<int>(QPsdBlend::Lighten));
    m_blendCombo->addItem(u"Soft Light"_s, static_cast<int>(QPsdBlend::SoftLight));
    m_blendCombo->addItem(u"Hard Light"_s, static_cast<int>(QPsdBlend::HardLight));
    m_blendCombo->addItem(u"Difference"_s, static_cast<int>(QPsdBlend::Difference));
    connect(m_blendCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onBlendModeChanged);
    blendLayout->addWidget(m_blendCombo);
    layout->addLayout(blendLayout);

    // Buttons
    auto *buttonLayout = new QHBoxLayout;
    auto *addBtn = new QPushButton(u"Add Layer"_s, panel);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addLayer);
    buttonLayout->addWidget(addBtn);

    auto *addFolderBtn = new QPushButton(u"Add Folder"_s, panel);
    connect(addFolderBtn, &QPushButton::clicked, this, &MainWindow::addFolder);
    buttonLayout->addWidget(addFolderBtn);

    auto *addTextBtn = new QPushButton(u"Add Text"_s, panel);
    connect(addTextBtn, &QPushButton::clicked, this, &MainWindow::addTextLayer);
    buttonLayout->addWidget(addTextBtn);
    layout->addLayout(buttonLayout);

    auto *buttonLayout2 = new QHBoxLayout;
    auto *removeBtn = new QPushButton(u"Remove Layer"_s, panel);
    connect(removeBtn, &QPushButton::clicked, this, &MainWindow::removeLayer);
    buttonLayout2->addWidget(removeBtn);

    auto *replaceBtn = new QPushButton(u"Replace Image..."_s, panel);
    connect(replaceBtn, &QPushButton::clicked, this, &MainWindow::replaceImage);
    buttonLayout2->addWidget(replaceBtn);
    layout->addLayout(buttonLayout2);

    dock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

void MainWindow::newFile()
{
    QDialog dialog(this);
    dialog.setWindowTitle(u"New Canvas"_s);

    auto *form = new QFormLayout(&dialog);

    auto *widthSpin = new QSpinBox(&dialog);
    widthSpin->setRange(1, 10000);
    widthSpin->setValue(m_model->canvasSize().width());
    form->addRow(u"Width:"_s, widthSpin);

    auto *heightSpin = new QSpinBox(&dialog);
    heightSpin->setRange(1, 10000);
    heightSpin->setValue(m_model->canvasSize().height());
    form->addRow(u"Height:"_s, heightSpin);

    QColor bgColor = Qt::white;
    auto *bgButton = new QPushButton(u"White"_s, &dialog);
    connect(bgButton, &QPushButton::clicked, &dialog, [&bgColor, bgButton, &dialog]() {
        const QColor c = QColorDialog::getColor(bgColor, &dialog, u"Background Color"_s);
        if (c.isValid()) {
            bgColor = c;
            bgButton->setText(c.name());
        }
    });
    form->addRow(u"Background:"_s, bgButton);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    m_model->clear();
    m_model->setCanvasSize(QSize(widthSpin->value(), heightSpin->value()));
    m_model->setBackgroundColor(bgColor);
    const auto idx = m_model->addLayer(u"Background"_s);
    m_canvas->setActiveLayer(idx);
    m_canvas->updateLayers();
}

void MainWindow::openFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, u"Open PSD"_s, QString(), u"PSD Files (*.psd)"_s);
    if (filePath.isEmpty())
        return;
    loadFromPsd(filePath);
}

void MainWindow::saveFile()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this, u"Save PSD"_s, QString(), u"PSD Files (*.psd)"_s);
    if (filePath.isEmpty())
        return;
    saveToPsd(filePath);
}

void MainWindow::onLayerSelectionChanged()
{
    const QModelIndex current = m_layerTree->currentIndex();
    if (current.isValid()) {
        m_canvas->setActiveLayer(current);
        updateLayerControls();
    }
}

void MainWindow::onOpacityChanged(int value)
{
    const QModelIndex current = m_layerTree->currentIndex();
    if (!current.isValid())
        return;
    m_model->setLayerOpacity(current, static_cast<quint8>(value));
    m_opacityLabel->setText(QString::number(value));
}

void MainWindow::onBlendModeChanged(int comboIndex)
{
    const QModelIndex current = m_layerTree->currentIndex();
    if (!current.isValid() || comboIndex < 0)
        return;
    const auto mode = static_cast<QPsdBlend::Mode>(m_blendCombo->itemData(comboIndex).toInt());
    m_model->setLayerBlendMode(current, mode);
}

void MainWindow::addLayer()
{
    const QModelIndex current = m_layerTree->currentIndex();
    // If a folder is selected, add inside it; otherwise add at root
    QModelIndex parentIdx;
    if (current.isValid()) {
        const auto *layer = m_model->layerForIndex(current);
        if (layer && layer->type == Layer::FolderLayer)
            parentIdx = current;
    }
    const int num = m_model->totalLayerCount() + 1;
    const auto idx = m_model->addLayer(u"Layer %1"_s.arg(num), parentIdx);
    m_layerTree->setCurrentIndex(idx);
    m_canvas->setActiveLayer(idx);
    m_canvas->updateLayers();
}

void MainWindow::addFolder()
{
    const QModelIndex current = m_layerTree->currentIndex();
    QModelIndex parentIdx;
    if (current.isValid()) {
        const auto *layer = m_model->layerForIndex(current);
        if (layer && layer->type == Layer::FolderLayer)
            parentIdx = current;
    }
    const auto idx = m_model->addFolder(u"Group"_s, parentIdx);
    m_layerTree->setCurrentIndex(idx);
    m_layerTree->expand(idx);
}

void MainWindow::addTextLayer()
{
    const QModelIndex current = m_layerTree->currentIndex();
    QModelIndex parentIdx;
    if (current.isValid()) {
        const auto *layer = m_model->layerForIndex(current);
        if (layer && layer->type == Layer::FolderLayer)
            parentIdx = current;
    }
    const auto idx = m_model->addTextLayer(u"Text Layer"_s, parentIdx);

    // Set default text run
    auto *layer = m_model->layerForIndex(idx);
    if (layer) {
        QPsdTextLayerItem::Run run;
        run.text = u"Hello"_s;
        run.font = QFont(u"Arial"_s, 24);
        run.color = Qt::black;
        layer->textRuns = {run};
        layer->textPosition = QPoint(10, 30);
    }

    m_layerTree->setCurrentIndex(idx);
    m_canvas->setActiveLayer(idx);
    m_canvas->updateLayers();
}

void MainWindow::removeLayer()
{
    const QModelIndex current = m_layerTree->currentIndex();
    if (!current.isValid())
        return;
    m_model->removeLayer(current);
    if (m_model->rowCount() > 0) {
        const auto idx = m_model->index(0, 0);
        m_layerTree->setCurrentIndex(idx);
        m_canvas->setActiveLayer(idx);
    } else {
        m_canvas->setActiveLayer(QModelIndex());
    }
    m_canvas->updateLayers();
}

void MainWindow::replaceImage()
{
    const QModelIndex current = m_layerTree->currentIndex();
    if (!current.isValid())
        return;
    auto *layer = m_model->layerForIndex(current);
    if (!layer || layer->type != Layer::ImageLayer)
        return;

    const QString filePath = QFileDialog::getOpenFileName(
        this, u"Replace Image"_s, QString(), u"Images (*.png *.jpg *.jpeg *.bmp)"_s);
    if (filePath.isEmpty())
        return;

    QImage newImage(filePath);
    if (newImage.isNull()) {
        QMessageBox::warning(this, u"Error"_s, u"Could not load image."_s);
        return;
    }

    // Place the loaded image on a canvas-sized transparent image
    QImage canvasImage(m_model->canvasSize(), QImage::Format_ARGB32);
    canvasImage.fill(Qt::transparent);
    QPainter p(&canvasImage);
    // Scale to fit canvas if larger
    const QSize targetSize = newImage.size().boundedTo(m_model->canvasSize());
    const QImage scaled = newImage.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    p.drawImage(0, 0, scaled);
    p.end();

    m_model->setLayerImage(current, canvasImage);
    m_canvas->updateLayers();
}

void MainWindow::pickColor()
{
    const QColor c = QColorDialog::getColor(m_canvas->color(), this, u"Brush Color"_s);
    if (c.isValid())
        m_canvas->setColor(c);
}

void MainWindow::onLayerDoubleClicked(const QModelIndex &index)
{
    auto *layer = m_model->layerForIndex(index);
    if (!layer)
        return;
    if (layer->type == Layer::TextLayer)
        editTextLayer(index);
}

void MainWindow::updateLayerControls()
{
    const QModelIndex current = m_layerTree->currentIndex();
    if (!current.isValid())
        return;

    const auto *layer = m_model->layerForIndex(current);
    if (!layer)
        return;

    m_opacitySlider->blockSignals(true);
    m_opacitySlider->setValue(layer->opacity);
    m_opacitySlider->blockSignals(false);
    m_opacityLabel->setText(QString::number(layer->opacity));

    m_blendCombo->blockSignals(true);
    const int idx = m_blendCombo->findData(static_cast<int>(layer->blendMode));
    m_blendCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_blendCombo->blockSignals(false);
}

// --- Phase 3: Text Layer Editing ---

void MainWindow::editTextLayer(const QModelIndex &index)
{
    auto *layer = m_model->layerForIndex(index);
    if (!layer || layer->type != Layer::TextLayer)
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(u"Edit Text Layer"_s);
    dialog.resize(400, 300);

    auto *layout = new QVBoxLayout(&dialog);

    auto *textEdit = new QPlainTextEdit(&dialog);
    // Populate with existing text (concatenate all runs)
    QString existingText;
    for (const auto &run : layer->textRuns)
        existingText += run.text;
    textEdit->setPlainText(existingText);
    layout->addWidget(textEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString newText = textEdit->toPlainText();
    if (newText == existingText)
        return;

    // Update only the text content, keeping font/color/alignment from existing runs
    if (layer->textRuns.isEmpty()) {
        QPsdTextLayerItem::Run run;
        run.text = newText;
        run.font = QFont(u"Arial"_s, 24);
        run.color = Qt::black;
        layer->textRuns = {run};
    } else {
        // Put all new text into the first run, keeping its font/color
        layer->textRuns.first().text = newText;
        while (layer->textRuns.size() > 1)
            layer->textRuns.removeLast();
    }

    // Clear original TySh so it gets regenerated on save
    layer->originalTySh = QVariant();
    // Canvas uses QGraphicsTextItem for text layers — just refresh
    emit m_model->dataChanged(index, index);
    m_canvas->updateLayers();
}

// --- Phase 5: Save/Open with tree support ---

// Helper: collect layer records recursively for PSD save (bottom-to-top within each level)
static void collectLayerRecords(const LayerModel *model, const QModelIndex &parent,
                                const QSize &canvasSize,
                                QList<QPsdLayerRecord> &records,
                                QList<QPsdChannelImageData> &channelDataList)
{
    const int count = model->rowCount(parent);
    // PSD expects bottom-to-top order; model stores top-first
    for (int i = count - 1; i >= 0; --i) {
        const auto idx = model->index(i, 0, parent);
        const auto *layer = model->layerForIndex(idx);
        if (!layer)
            continue;

        if (layer->type == Layer::FolderLayer) {
            // Write bounding section divider (end marker) first
            {
                QPsdLayerRecord record;
                record.setRect(QRect(0, 0, 0, 0));
                // 4 zero-length channels
                QList<QPsdChannelInfo> channelInfos;
                for (auto id : {QPsdChannelInfo::TransparencyMask, QPsdChannelInfo::Red,
                                QPsdChannelInfo::Green, QPsdChannelInfo::Blue}) {
                    QPsdChannelInfo ci;
                    ci.setId(id);
                    ci.setLength(2); // Just compression header, no data
                    channelInfos.append(ci);
                }
                record.setChannelInfo(channelInfos);
                record.setBlendMode(QPsdBlend::Normal);
                record.setOpacity(255);
                record.setFlags(0x00);
                record.setName("</Layer group>");

                QHash<QByteArray, QVariant> ali;
                ali.insert("luni", u"</Layer group>"_s);
                QPsdSectionDividerSetting sds;
                sds.setType(QPsdSectionDividerSetting::BoundingSectionDivider);
                ali.insert("lsct", QVariant::fromValue(sds));
                record.setAdditionalLayerInformation(ali);

                QPsdChannelImageData emptyData;
                emptyData.setWidth(0);
                emptyData.setHeight(0);
                record.setImageData(emptyData);
                records.append(record);
                channelDataList.append(emptyData);
            }

            // Recurse into children
            collectLayerRecords(model, idx, canvasSize, records, channelDataList);

            // Write folder header (open folder marker)
            {
                QPsdLayerRecord record;
                record.setRect(QRect(0, 0, 0, 0));
                QList<QPsdChannelInfo> channelInfos;
                for (auto id : {QPsdChannelInfo::TransparencyMask, QPsdChannelInfo::Red,
                                QPsdChannelInfo::Green, QPsdChannelInfo::Blue}) {
                    QPsdChannelInfo ci;
                    ci.setId(id);
                    ci.setLength(2);
                    channelInfos.append(ci);
                }
                record.setChannelInfo(channelInfos);
                record.setBlendMode(layer->blendMode);
                record.setOpacity(layer->opacity);
                record.setFlags(layer->visible ? 0x00 : 0x02);
                record.setName(layer->name.toUtf8());

                QHash<QByteArray, QVariant> ali;
                ali.insert("luni", layer->name);
                QPsdSectionDividerSetting sds;
                sds.setType(QPsdSectionDividerSetting::OpenFolder);
                sds.setKey(layer->blendMode);
                ali.insert("lsct", QVariant::fromValue(sds));
                record.setAdditionalLayerInformation(ali);

                QPsdChannelImageData emptyData;
                emptyData.setWidth(0);
                emptyData.setHeight(0);
                record.setImageData(emptyData);
                records.append(record);
                channelDataList.append(emptyData);
            }
        } else {
            // Regular layer (image or text)
            const int pixelCount = canvasSize.width() * canvasSize.height();

            QByteArray rData(pixelCount, '\0');
            QByteArray gData(pixelCount, '\0');
            QByteArray bData(pixelCount, '\0');
            QByteArray aData(pixelCount, '\0');

            if (!layer->image.isNull()) {
                const QImage img = layer->image.convertToFormat(QImage::Format_ARGB32);
                for (int y = 0; y < img.height(); ++y) {
                    const auto *scanline = reinterpret_cast<const quint32 *>(img.constScanLine(y));
                    const int rowOffset = y * img.width();
                    for (int x = 0; x < img.width(); ++x) {
                        const quint32 pixel = scanline[x];
                        aData[rowOffset + x] = static_cast<char>((pixel >> 24) & 0xFF);
                        rData[rowOffset + x] = static_cast<char>((pixel >> 16) & 0xFF);
                        gData[rowOffset + x] = static_cast<char>((pixel >> 8) & 0xFF);
                        bData[rowOffset + x] = static_cast<char>(pixel & 0xFF);
                    }
                }
            }

            const quint32 channelLength = 2 + static_cast<quint32>(pixelCount);
            QList<QPsdChannelInfo> channelInfos;
            if (layer->hasAlpha) {
                QPsdChannelInfo ci;
                ci.setId(QPsdChannelInfo::TransparencyMask);
                ci.setLength(channelLength);
                channelInfos.append(ci);
            }
            for (auto id : {QPsdChannelInfo::Red,
                            QPsdChannelInfo::Green, QPsdChannelInfo::Blue}) {
                QPsdChannelInfo ci;
                ci.setId(id);
                ci.setLength(channelLength);
                channelInfos.append(ci);
            }

            QPsdLayerRecord record;
            record.setRect(QRect(0, 0, canvasSize.width(), canvasSize.height()));
            record.setChannelInfo(channelInfos);
            record.setBlendMode(layer->blendMode);
            record.setOpacity(layer->opacity);
            record.setFlags(layer->originalFlags != 0 ? layer->originalFlags
                            : (layer->visible ? 0x00 : 0x02));
            record.setName(layer->name.toUtf8());

            QHash<QByteArray, QVariant> ali;
            ali.insert("luni", layer->name);

            // Text layer: add TySh ALI
            if (layer->type == Layer::TextLayer) {
                if (layer->originalTySh.isValid()) {
                    // Round-trip: use original TySh data
                    ali.insert("TySh", layer->originalTySh);
                } else if (!layer->textRuns.isEmpty()) {
                    // Build TySh from text runs
                    const auto &run = layer->textRuns.first();

                    // Build EngineData
                    QCborMap engineData;

                    // ResourceDict with FontSet
                    QCborMap resourceDict;
                    QCborArray fontSet;
                    QCborMap font;
                    font.insert(u"Name"_s, QCborValue(run.font.family()));
                    font.insert(u"Script"_s, QCborValue(0));
                    font.insert(u"FontType"_s, QCborValue(0));
                    font.insert(u"Synthetic"_s, QCborValue(0));
                    fontSet.append(QCborValue(font));
                    resourceDict.insert(u"FontSet"_s, QCborValue(fontSet));
                    engineData.insert(u"ResourceDict"_s, QCborValue(resourceDict));

                    // EngineDict with Editor, StyleRun
                    QCborMap engineDict;
                    QCborMap editor;
                    editor.insert(u"Text"_s, QCborValue(run.text));
                    engineDict.insert(u"Editor"_s, QCborValue(editor));

                    // StyleRun
                    QCborMap styleRun;
                    QCborArray runArray;
                    QCborMap runLengthArray;
                    runLengthArray.insert(u"From"_s, QCborValue(0));
                    runLengthArray.insert(u"To"_s, QCborValue(static_cast<qint64>(run.text.length())));
                    runArray.append(QCborValue(runLengthArray));
                    styleRun.insert(u"RunArray"_s, QCborValue(runArray));

                    QCborArray runLenArray;
                    runLenArray.append(QCborValue(static_cast<qint64>(run.text.length())));
                    styleRun.insert(u"RunLengthArray"_s, QCborValue(runLenArray));

                    QCborArray styleSheetArray;
                    QCborMap styleSheet;
                    QCborMap styleSheetData;
                    styleSheetData.insert(u"Font"_s, QCborValue(0)); // Index into FontSet
                    styleSheetData.insert(u"FontSize"_s, QCborValue(static_cast<double>(run.font.pointSizeF())));
                    // Color as RGBA
                    QCborMap fillColor;
                    fillColor.insert(u"Type"_s, QCborValue(1)); // RGB
                    QCborArray colorValues;
                    colorValues.append(QCborValue(run.color.redF()));
                    colorValues.append(QCborValue(run.color.greenF()));
                    colorValues.append(QCborValue(run.color.blueF()));
                    colorValues.append(QCborValue(run.color.alphaF()));
                    fillColor.insert(u"Values"_s, QCborValue(colorValues));
                    styleSheetData.insert(u"FillColor"_s, QCborValue(fillColor));
                    styleSheet.insert(u"StyleSheetData"_s, QCborValue(styleSheetData));
                    styleSheetArray.append(QCborValue(styleSheet));
                    styleRun.insert(u"RunArray"_s, QCborValue(styleSheetArray));

                    engineDict.insert(u"StyleRun"_s, QCborValue(styleRun));
                    engineData.insert(u"EngineDict"_s, QCborValue(engineDict));

                    const QByteArray edBytes = QPsdEngineDataParser::serializeEngineData(engineData);

                    // Build text descriptor
                    QPsdDescriptor textDesc;
                    textDesc.setName(u""_s);
                    textDesc.setClassID("TxLr");
                    QHash<QByteArray, QVariant> textData;
                    textData.insert("Txt ", run.text);
                    textData.insert("EngineData", edBytes);
                    textDesc.setData(textData);

                    // Build warp descriptor
                    QPsdDescriptor warpDesc;
                    warpDesc.setName(u""_s);
                    warpDesc.setClassID("warp");
                    QHash<QByteArray, QVariant> warpData;
                    QPsdEnum warpStyleEnum;
                    warpStyleEnum.setType("warpStyle");
                    warpStyleEnum.setValue("warpNone");
                    warpData.insert("warpStyle", QVariant::fromValue(warpStyleEnum));
                    warpData.insert("warpValue", 0.0);
                    warpData.insert("warpPerspective", 0.0);
                    warpData.insert("warpPerspectiveOther", 0.0);
                    QPsdEnum warpRotateEnum;
                    warpRotateEnum.setType("Ornt");
                    warpRotateEnum.setValue("Hrzn");
                    warpData.insert("warpRotate", QVariant::fromValue(warpRotateEnum));
                    warpDesc.setData(warpData);

                    // Build TySh
                    QPsdTypeToolObjectSetting tysh;
                    tysh.setTransform({1.0, 0.0, 0.0, 1.0, 0.0, 0.0}); // Identity
                    tysh.setTextData(textDesc);
                    tysh.setWarpData(warpDesc);
                    tysh.setRect(QRect(0, 0, canvasSize.width(), canvasSize.height()));
                    ali.insert("TySh", QVariant::fromValue(tysh));
                }
            }

            record.setAdditionalLayerInformation(ali);

            QPsdChannelImageData channelImageData;
            channelImageData.setWidth(canvasSize.width());
            channelImageData.setHeight(canvasSize.height());
            if (layer->hasAlpha)
                channelImageData.setChannelData(QPsdChannelInfo::TransparencyMask, aData);
            channelImageData.setChannelData(QPsdChannelInfo::Red, rData);
            channelImageData.setChannelData(QPsdChannelInfo::Green, gData);
            channelImageData.setChannelData(QPsdChannelInfo::Blue, bData);

            record.setImageData(channelImageData);
            records.append(record);
            channelDataList.append(channelImageData);
        }
    }
}

void MainWindow::saveToPsd(const QString &filePath)
{
    const QSize size = m_model->canvasSize();
    const int pixelCount = size.width() * size.height();

    // 1. File header: 3 channels (RGB composite), 8-bit, RGB
    QPsdFileHeader header;
    header.setChannels(3);
    header.setWidth(size.width());
    header.setHeight(size.height());
    header.setDepth(8);
    header.setColorMode(QPsdFileHeader::RGB);

    // 2. Build layer records and channel image data recursively
    QList<QPsdLayerRecord> records;
    QList<QPsdChannelImageData> channelDataList;
    collectLayerRecords(m_model, QModelIndex(), size, records, channelDataList);

    // 3. Layer info
    QPsdLayerInfo layerInfo;
    layerInfo.setRecords(records);
    layerInfo.setChannelImageData(channelDataList);

    // 4. Layer and mask information
    QPsdLayerAndMaskInformation lmi;
    lmi.setLayerInfo(layerInfo);

    // 5. Composite image data (flattened, 3 channels: R/G/B)
    const QImage flat = m_model->flattenedImage().convertToFormat(QImage::Format_ARGB32);
    QByteArray compositeData;
    compositeData.reserve(3 * pixelCount);

    // R channel
    for (int y = 0; y < flat.height(); ++y) {
        const auto *scanline = reinterpret_cast<const quint32 *>(flat.constScanLine(y));
        for (int x = 0; x < flat.width(); ++x)
            compositeData.append(static_cast<char>((scanline[x] >> 16) & 0xFF));
    }
    // G channel
    for (int y = 0; y < flat.height(); ++y) {
        const auto *scanline = reinterpret_cast<const quint32 *>(flat.constScanLine(y));
        for (int x = 0; x < flat.width(); ++x)
            compositeData.append(static_cast<char>((scanline[x] >> 8) & 0xFF));
    }
    // B channel
    for (int y = 0; y < flat.height(); ++y) {
        const auto *scanline = reinterpret_cast<const quint32 *>(flat.constScanLine(y));
        for (int x = 0; x < flat.width(); ++x)
            compositeData.append(static_cast<char>(scanline[x] & 0xFF));
    }

    QPsdImageData imageData;
    imageData.setWidth(size.width());
    imageData.setHeight(size.height());
    imageData.setImageData(compositeData);

    // 6. Write
    QPsdWriter writer;
    writer.setFileHeader(header);
    writer.setLayerAndMaskInformation(lmi);
    writer.setImageData(imageData);

    if (!writer.write(filePath)) {
        QMessageBox::critical(this, u"Save Error"_s, writer.errorString());
    }
}

void MainWindow::loadFromPsd(const QString &filePath)
{
    QPsdGuiLayerTreeItemModel guiModel;
    guiModel.load(filePath);

    if (!guiModel.errorMessage().isEmpty()) {
        QMessageBox::critical(this, u"Open Error"_s, guiModel.errorMessage());
        return;
    }

    const QSize size = guiModel.size();
    m_model->clear();
    m_model->setCanvasSize(size);

    // Recursive walk that preserves tree structure
    std::function<void(const QModelIndex &, const QModelIndex &)> walkTree;
    walkTree = [&](const QModelIndex &sourceParent, const QModelIndex &destParent) {
        // Insert bottom-to-top so that top-first ordering is maintained
        for (int row = guiModel.rowCount(sourceParent) - 1; row >= 0; --row) {
            const QModelIndex srcIdx = guiModel.index(row, 0, sourceParent);
            const auto folderType = guiModel.folderType(srcIdx);

            if (folderType == QPsdLayerTreeItemModel::FolderType::OpenFolder
                || folderType == QPsdLayerTreeItemModel::FolderType::ClosedFolder) {
                // Folder
                Layer folder;
                folder.type = Layer::FolderLayer;
                folder.name = guiModel.layerName(srcIdx);
                const auto *item = guiModel.layerItem(srcIdx);
                if (item) {
                    folder.opacity = static_cast<quint8>(qRound(item->opacity() * 255.0));
                    folder.visible = item->isVisible();
                }
                const QPsdLayerRecord *record = guiModel.layerRecord(srcIdx);
                if (record)
                    folder.blendMode = record->blendMode();

                const auto destIdx = m_model->insertLayerAt(destParent, 0, folder);
                // Recurse into children
                walkTree(srcIdx, destIdx);
            } else {
                // Leaf layer
                const auto *item = guiModel.layerItem(srcIdx);
                if (!item)
                    continue;

                Layer layer;
                layer.name = guiModel.layerName(srcIdx);
                layer.opacity = static_cast<quint8>(qRound(item->opacity() * 255.0));
                layer.visible = item->isVisible();

                layer.imageRect = item->rect();

                const QPsdLayerRecord *record = guiModel.layerRecord(srcIdx);
                if (record) {
                    layer.blendMode = record->blendMode();
                    // Reconstruct flags from boolean getters
                    quint8 flags = 0;
                    if (record->isTransparencyProtected())
                        flags |= 0x01;
                    if (record->isVisible())
                        flags |= 0x02;
                    if (record->hasPixelDataIrrelevantToAppearanceDocument())
                        flags |= 0x08;
                    if (record->isPixelDataIrrelevantToAppearanceDocument())
                        flags |= 0x10;
                    layer.originalFlags = flags;
                    // Check if layer has alpha channel (transparency mask)
                    layer.hasAlpha = false;
                    for (const auto &ci : record->channelInfo()) {
                        if (ci.id() == QPsdChannelInfo::TransparencyMask) {
                            layer.hasAlpha = true;
                            break;
                        }
                    }
                    // Check for text layer
                    if (record->additionalLayerInformation().contains("TySh")) {
                        layer.type = Layer::TextLayer;
                        layer.originalTySh = record->additionalLayerInformation().value("TySh");
                        layer.textPosition = item->rect().topLeft();
                        // Extract text runs and origin from the text layer item
                        if (item->type() == QPsdAbstractLayerItem::Text) {
                            const auto *textItem = static_cast<const QPsdTextLayerItem *>(item);
                            layer.textRuns = textItem->runs();
                            layer.textOrigin = textItem->textOrigin();
                        }
                    }
                }

                // Image layers: place pixel data on a full canvas
                if (layer.type != Layer::TextLayer) {
                    QImage canvasImage(size, QImage::Format_ARGB32);
                    canvasImage.fill(Qt::transparent);
                    const QImage layerImage = item->image();
                    if (!layerImage.isNull()) {
                        QPainter p(&canvasImage);
                        p.drawImage(item->rect().topLeft(), layerImage);
                        p.end();
                    }
                    layer.image = canvasImage;
                }

                m_model->insertLayerAt(destParent, 0, layer);
            }
        }
    };
    walkTree(QModelIndex(), QModelIndex());

    m_layerTree->expandAll();
    if (m_model->rowCount() > 0) {
        const auto idx = m_model->index(0, 0);
        m_layerTree->setCurrentIndex(idx);
        m_canvas->setActiveLayer(idx);
    }
    m_canvas->updateLayers();
}

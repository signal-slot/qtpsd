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
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QSpinBox>
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
#include <QtPsdGui/QPsdShapeLayerItem>
#include <QtPsdGui/QPsdTextLayerItem>
#include <QtPsdGui/qpsdguiglobal.h>

using namespace Qt::StringLiterals;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(u"PSD Creator"_s);
    resize(1200, 800);

    m_model = new LayerModel(this);
    m_canvas = new Canvas(m_model, this);
    setCentralWidget(m_canvas);

    setupMenus();
    setupLayersPanel();
    setupPropertiesPanel();

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

void MainWindow::setupLayersPanel()
{
    auto *dock = new QDockWidget(u"Layers"_s, this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto *panel = new QWidget(dock);
    auto *layout = new QVBoxLayout(panel);

    // Buttons above the tree
    auto *buttonLayout = new QHBoxLayout;
    auto *addFolderBtn = new QPushButton(u"Folder"_s, panel);
    connect(addFolderBtn, &QPushButton::clicked, this, &MainWindow::addFolder);
    buttonLayout->addWidget(addFolderBtn);

    auto *addTextBtn = new QPushButton(u"Text"_s, panel);
    connect(addTextBtn, &QPushButton::clicked, this, &MainWindow::addTextLayer);
    buttonLayout->addWidget(addTextBtn);

    auto *addBtn = new QPushButton(u"Image"_s, panel);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addLayer);
    buttonLayout->addWidget(addBtn);

    auto *addShapeBtn = new QPushButton(u"Shape"_s, panel);
    connect(addShapeBtn, &QPushButton::clicked, this, &MainWindow::addShapeLayer);
    buttonLayout->addWidget(addShapeBtn);

    auto *removeBtn = new QPushButton(u"Remove"_s, panel);
    connect(removeBtn, &QPushButton::clicked, this, &MainWindow::removeLayer);
    buttonLayout->addWidget(removeBtn);

    layout->addLayout(buttonLayout);

    // Layer tree view
    m_layerTree = new QTreeView(panel);
    m_layerTree->setModel(m_model);
    m_layerTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layerTree->setHeaderHidden(true);
    m_layerTree->setExpandsOnDoubleClick(false);
    connect(m_layerTree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onLayerSelectionChanged);
    connect(m_layerTree, &QTreeView::doubleClicked,
            this, &MainWindow::onLayerDoubleClicked);
    layout->addWidget(m_layerTree);

    dock->setWidget(panel);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

void MainWindow::setupPropertiesPanel()
{
    auto *dock = new QDockWidget(u"Properties"_s, this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto *panel = new QWidget(dock);
    auto *layout = new QVBoxLayout(panel);

    // --- Common section ---
    m_commonGroup = new QGroupBox(u"Common"_s, panel);
    auto *commonLayout = new QFormLayout(m_commonGroup);

    m_opacitySlider = new QSlider(Qt::Horizontal, m_commonGroup);
    m_opacitySlider->setRange(0, 255);
    m_opacitySlider->setValue(255);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &MainWindow::onOpacityChanged);
    m_opacityLabel = new QLabel(u"255"_s, m_commonGroup);
    m_opacityLabel->setFixedWidth(30);
    auto *opacityRow = new QHBoxLayout;
    opacityRow->addWidget(m_opacitySlider);
    opacityRow->addWidget(m_opacityLabel);
    commonLayout->addRow(u"Opacity:"_s, opacityRow);

    m_blendCombo = new QComboBox(m_commonGroup);
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
    commonLayout->addRow(u"Blend:"_s, m_blendCombo);

    layout->addWidget(m_commonGroup);

    // --- Image section ---
    m_imageGroup = new QGroupBox(u"Image"_s, panel);
    auto *imageLayout = new QVBoxLayout(m_imageGroup);

    // Tool buttons
    auto *toolLayout = new QHBoxLayout;
    auto *brushBtn = new QPushButton(u"Brush"_s, m_imageGroup);
    brushBtn->setCheckable(true);
    brushBtn->setChecked(true);
    auto *eraserBtn = new QPushButton(u"Eraser"_s, m_imageGroup);
    eraserBtn->setCheckable(true);
    auto *fillBtn = new QPushButton(u"Fill"_s, m_imageGroup);
    fillBtn->setCheckable(true);

    auto *toolGroup = new QButtonGroup(this);
    toolGroup->setExclusive(true);
    toolGroup->addButton(brushBtn, 0);
    toolGroup->addButton(eraserBtn, 1);
    toolGroup->addButton(fillBtn, 2);
    connect(toolGroup, &QButtonGroup::idClicked, this, [this](int id) {
        switch (id) {
        case 0: m_canvas->setTool(Canvas::BrushTool); break;
        case 1: m_canvas->setTool(Canvas::EraserTool); break;
        case 2: m_canvas->setTool(Canvas::FillTool); break;
        }
    });
    toolLayout->addWidget(brushBtn);
    toolLayout->addWidget(eraserBtn);
    toolLayout->addWidget(fillBtn);
    imageLayout->addLayout(toolLayout);

    // Color + Size
    auto *imageForm = new QFormLayout;
    auto *colorButton = new QPushButton(u"Color..."_s, m_imageGroup);
    connect(colorButton, &QPushButton::clicked, this, &MainWindow::pickColor);
    imageForm->addRow(u"Color:"_s, colorButton);

    m_brushSizeSpin = new QSpinBox(m_imageGroup);
    m_brushSizeSpin->setRange(1, 100);
    m_brushSizeSpin->setValue(5);
    connect(m_brushSizeSpin, &QSpinBox::valueChanged, m_canvas, &Canvas::setBrushSize);
    imageForm->addRow(u"Size:"_s, m_brushSizeSpin);

    imageLayout->addLayout(imageForm);

    auto *replaceBtn = new QPushButton(u"Replace Image..."_s, m_imageGroup);
    connect(replaceBtn, &QPushButton::clicked, this, &MainWindow::replaceImage);
    imageLayout->addWidget(replaceBtn);

    layout->addWidget(m_imageGroup);

    // --- Text section ---
    m_textGroup = new QGroupBox(u"Text"_s, panel);
    auto *textLayout = new QFormLayout(m_textGroup);

    m_textFontCombo = new QFontComboBox(m_textGroup);
    connect(m_textFontCombo, &QFontComboBox::currentFontChanged, this, [this]() {
        applyTextToolbarToLayer();
    });
    textLayout->addRow(u"Font:"_s, m_textFontCombo);

    m_textSizeSpin = new QSpinBox(m_textGroup);
    m_textSizeSpin->setRange(1, 500);
    m_textSizeSpin->setValue(24);
    m_textSizeSpin->setSuffix(u" pt"_s);
    connect(m_textSizeSpin, &QSpinBox::valueChanged, this, [this]() {
        applyTextToolbarToLayer();
    });
    textLayout->addRow(u"Size:"_s, m_textSizeSpin);

    auto *styleLayout = new QHBoxLayout;
    m_textBoldBtn = new QPushButton(u"B"_s, m_textGroup);
    m_textBoldBtn->setCheckable(true);
    auto boldFont = m_textBoldBtn->font();
    boldFont.setBold(true);
    m_textBoldBtn->setFont(boldFont);
    connect(m_textBoldBtn, &QPushButton::toggled, this, [this]() {
        applyTextToolbarToLayer();
    });
    styleLayout->addWidget(m_textBoldBtn);

    m_textItalicBtn = new QPushButton(u"I"_s, m_textGroup);
    m_textItalicBtn->setCheckable(true);
    auto italicFont = m_textItalicBtn->font();
    italicFont.setItalic(true);
    m_textItalicBtn->setFont(italicFont);
    connect(m_textItalicBtn, &QPushButton::toggled, this, [this]() {
        applyTextToolbarToLayer();
    });
    styleLayout->addWidget(m_textItalicBtn);
    textLayout->addRow(u"Style:"_s, styleLayout);

    m_textColorBtn = new QPushButton(m_textGroup);
    m_textColorBtn->setFixedHeight(28);
    connect(m_textColorBtn, &QPushButton::clicked, this, [this]() {
        const QModelIndex current = m_layerTree->currentIndex();
        auto *layer = m_model->layerForIndex(current);
        if (!layer || layer->type != Layer::TextLayer || layer->textRuns.isEmpty())
            return;
        const QColor c = QColorDialog::getColor(layer->textRuns.first().color, this);
        if (c.isValid()) {
            for (auto &run : layer->textRuns)
                run.color = c;
            layer->originalTySh = QVariant();
            m_textColorBtn->setStyleSheet(u"background-color: %1"_s.arg(c.name()));
            emit m_model->dataChanged(current, current);
            m_canvas->updateLayers();
        }
    });
    textLayout->addRow(u"Color:"_s, m_textColorBtn);

    auto *editTextBtn = new QPushButton(u"Edit Text..."_s, m_textGroup);
    connect(editTextBtn, &QPushButton::clicked, this, [this]() {
        const QModelIndex current = m_layerTree->currentIndex();
        if (current.isValid())
            editTextLayer(current);
    });
    textLayout->addRow(editTextBtn);

    layout->addWidget(m_textGroup);

    // --- Shape section ---
    m_shapeGroup = new QGroupBox(u"Shape"_s, panel);
    auto *shapeLayout = new QFormLayout(m_shapeGroup);

    m_shapeFillBtn = new QPushButton(m_shapeGroup);
    m_shapeFillBtn->setFixedHeight(28);
    connect(m_shapeFillBtn, &QPushButton::clicked, this, [this]() {
        const QModelIndex current = m_layerTree->currentIndex();
        auto *layer = m_model->layerForIndex(current);
        if (!layer || layer->type != Layer::ShapeLayer)
            return;
        const QColor c = QColorDialog::getColor(layer->shapeFillColor, this);
        if (c.isValid()) {
            layer->shapeFillColor = c;
            layer->originalSoCo = QVariant();
            m_shapeFillBtn->setStyleSheet(u"background-color: %1"_s.arg(c.name()));
            emit m_model->dataChanged(current, current);
            m_canvas->updateLayers();
        }
    });
    shapeLayout->addRow(u"Fill:"_s, m_shapeFillBtn);

    m_shapeStrokeBtn = new QPushButton(m_shapeGroup);
    m_shapeStrokeBtn->setFixedHeight(28);
    connect(m_shapeStrokeBtn, &QPushButton::clicked, this, [this]() {
        const QModelIndex current = m_layerTree->currentIndex();
        auto *layer = m_model->layerForIndex(current);
        if (!layer || layer->type != Layer::ShapeLayer)
            return;
        const QColor c = QColorDialog::getColor(layer->shapeStrokeColor, this);
        if (c.isValid()) {
            layer->shapeStrokeColor = c;
            layer->originalVstk = QVariant();
            m_shapeStrokeBtn->setStyleSheet(u"background-color: %1"_s.arg(c.name()));
            emit m_model->dataChanged(current, current);
            m_canvas->updateLayers();
        }
    });
    shapeLayout->addRow(u"Stroke:"_s, m_shapeStrokeBtn);

    m_shapeStrokeWidthSpin = new QDoubleSpinBox(m_shapeGroup);
    m_shapeStrokeWidthSpin->setRange(0, 100);
    m_shapeStrokeWidthSpin->setValue(0);
    connect(m_shapeStrokeWidthSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        const QModelIndex current = m_layerTree->currentIndex();
        auto *layer = m_model->layerForIndex(current);
        if (!layer || layer->type != Layer::ShapeLayer)
            return;
        layer->shapeStrokeWidth = value;
        layer->originalVstk = QVariant();
        emit m_model->dataChanged(current, current);
        m_canvas->updateLayers();
    });
    shapeLayout->addRow(u"Width:"_s, m_shapeStrokeWidthSpin);

    layout->addWidget(m_shapeGroup);

    layout->addStretch();

    dock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

void MainWindow::updatePropertiesPanel()
{
    const QModelIndex current = m_layerTree->currentIndex();
    const auto *layer = current.isValid() ? m_model->layerForIndex(current) : nullptr;
    const auto type = layer ? layer->type : Layer::ImageLayer;

    m_imageGroup->setVisible(type == Layer::ImageLayer);
    m_textGroup->setVisible(type == Layer::TextLayer);
    m_shapeGroup->setVisible(type == Layer::ShapeLayer);

    if (type == Layer::TextLayer)
        updateTextToolbar();

    if (type == Layer::ShapeLayer) {
        m_shapeFillBtn->setStyleSheet(
            layer->shapeFillColor.isValid()
                ? u"background-color: %1"_s.arg(layer->shapeFillColor.name())
                : QString());
        m_shapeStrokeBtn->setStyleSheet(
            layer->shapeStrokeColor.isValid()
                ? u"background-color: %1"_s.arg(layer->shapeStrokeColor.name())
                : QString());
        m_shapeStrokeWidthSpin->blockSignals(true);
        m_shapeStrokeWidthSpin->setValue(layer->shapeStrokeWidth);
        m_shapeStrokeWidthSpin->blockSignals(false);
    }
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

void MainWindow::addShapeLayer()
{
    const QModelIndex current = m_layerTree->currentIndex();
    QModelIndex parentIdx;
    if (current.isValid()) {
        const auto *layer = m_model->layerForIndex(current);
        if (layer && layer->type == Layer::FolderLayer)
            parentIdx = current;
    }
    const auto idx = m_model->addShapeLayer(u"Shape Layer"_s, parentIdx);
    auto *layer = m_model->layerForIndex(idx);
    if (layer) {
        const QSize cs = m_model->canvasSize();
        const QRectF rect((cs.width() - 200) / 2.0, (cs.height() - 100) / 2.0, 200, 100);
        QPainterPath path;
        path.addRect(rect);
        layer->shapePath = path;
        layer->shapeFillColor = QColor(100, 150, 255);
        layer->shapeStrokeColor = Qt::black;
        layer->shapeStrokeWidth = 2;
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

    QImage canvasImage(m_model->canvasSize(), QImage::Format_ARGB32);
    canvasImage.fill(Qt::transparent);
    QPainter p(&canvasImage);
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
    else if (layer->type == Layer::ShapeLayer)
        editShapeLayer(index);
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

    updatePropertiesPanel();
}

void MainWindow::applyTextToolbarToLayer()
{
    const QModelIndex current = m_layerTree->currentIndex();
    auto *layer = m_model->layerForIndex(current);
    if (!layer || layer->type != Layer::TextLayer || layer->textRuns.isEmpty())
        return;

    const QFont selectedFont = m_textFontCombo->currentFont();
    const int size = m_textSizeSpin->value();
    const bool bold = m_textBoldBtn->isChecked();
    const bool italic = m_textItalicBtn->isChecked();

    for (auto &run : layer->textRuns) {
        run.font.setFamily(selectedFont.family());
        run.font.setPointSize(size);
        run.font.setBold(bold);
        run.font.setItalic(italic);
    }
    layer->originalTySh = QVariant();
    emit m_model->dataChanged(current, current);
    m_canvas->updateLayers();
}

void MainWindow::updateTextToolbar()
{
    const QModelIndex current = m_layerTree->currentIndex();
    const auto *layer = m_model->layerForIndex(current);
    if (!layer || layer->type != Layer::TextLayer || layer->textRuns.isEmpty())
        return;

    const auto &run = layer->textRuns.first();

    m_textFontCombo->blockSignals(true);
    m_textFontCombo->setCurrentFont(run.font);
    m_textFontCombo->blockSignals(false);

    m_textSizeSpin->blockSignals(true);
    m_textSizeSpin->setValue(run.font.pointSize() > 0 ? run.font.pointSize() : 24);
    m_textSizeSpin->blockSignals(false);

    m_textBoldBtn->blockSignals(true);
    m_textBoldBtn->setChecked(run.font.bold());
    m_textBoldBtn->blockSignals(false);

    m_textItalicBtn->blockSignals(true);
    m_textItalicBtn->setChecked(run.font.italic());
    m_textItalicBtn->blockSignals(false);

    m_textColorBtn->setStyleSheet(
        run.color.isValid()
            ? u"background-color: %1"_s.arg(run.color.name())
            : QString());
}

// --- Text/Shape Layer Editing ---

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

    if (layer->textRuns.isEmpty()) {
        QPsdTextLayerItem::Run run;
        run.text = newText;
        run.font = QFont(u"Arial"_s, 24);
        run.color = Qt::black;
        layer->textRuns = {run};
    } else {
        layer->textRuns.first().text = newText;
        while (layer->textRuns.size() > 1)
            layer->textRuns.removeLast();
    }

    layer->originalTySh = QVariant();
    emit m_model->dataChanged(index, index);
    m_canvas->updateLayers();
}

void MainWindow::editShapeLayer(const QModelIndex &index)
{
    auto *layer = m_model->layerForIndex(index);
    if (!layer || layer->type != Layer::ShapeLayer)
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(u"Edit Shape"_s);

    auto *form = new QFormLayout(&dialog);

    QColor fillColor = layer->shapeFillColor;
    auto *fillBtn = new QPushButton(fillColor.isValid() ? fillColor.name() : u"None"_s, &dialog);
    connect(fillBtn, &QPushButton::clicked, &dialog, [&]() {
        const QColor c = QColorDialog::getColor(fillColor, &dialog);
        if (c.isValid()) {
            fillColor = c;
            fillBtn->setText(c.name());
        }
    });
    form->addRow(u"Fill Color:"_s, fillBtn);

    QColor strokeColor = layer->shapeStrokeColor;
    auto *strokeBtn = new QPushButton(strokeColor.isValid() ? strokeColor.name() : u"None"_s, &dialog);
    connect(strokeBtn, &QPushButton::clicked, &dialog, [&]() {
        const QColor c = QColorDialog::getColor(strokeColor, &dialog);
        if (c.isValid()) {
            strokeColor = c;
            strokeBtn->setText(c.name());
        }
    });
    form->addRow(u"Stroke Color:"_s, strokeBtn);

    auto *widthSpin = new QDoubleSpinBox(&dialog);
    widthSpin->setRange(0, 100);
    widthSpin->setValue(layer->shapeStrokeWidth);
    form->addRow(u"Stroke Width:"_s, widthSpin);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    layer->shapeFillColor = fillColor;
    layer->shapeStrokeColor = strokeColor;
    layer->shapeStrokeWidth = widthSpin->value();
    layer->originalVstk = QVariant();
    layer->originalSoCo = QVariant();
    emit m_model->dataChanged(index, index);
    m_canvas->updateLayers();
}

// --- Save/Open with tree support ---

static void collectLayerRecords(const LayerModel *model, const QModelIndex &parent,
                                const QSize &canvasSize,
                                QList<QPsdLayerRecord> &records,
                                QList<QPsdChannelImageData> &channelDataList)
{
    const int count = model->rowCount(parent);
    for (int i = count - 1; i >= 0; --i) {
        const auto idx = model->index(i, 0, parent);
        const auto *layer = model->layerForIndex(idx);
        if (!layer)
            continue;

        if (layer->type == Layer::FolderLayer) {
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

            collectLayerRecords(model, idx, canvasSize, records, channelDataList);

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
            const int pixelCount = canvasSize.width() * canvasSize.height();

            QByteArray rData(pixelCount, '\0');
            QByteArray gData(pixelCount, '\0');
            QByteArray bData(pixelCount, '\0');
            QByteArray aData(pixelCount, '\0');

            QImage sourceImage;
            if (layer->type == Layer::ShapeLayer) {
                QImage shapeImage(canvasSize, QImage::Format_ARGB32);
                shapeImage.fill(Qt::transparent);
                QPainter sp(&shapeImage);
                sp.setRenderHint(QPainter::Antialiasing);
                if (layer->shapeFillColor.isValid())
                    sp.setBrush(layer->shapeFillColor);
                if (layer->shapeStrokeWidth > 0 && layer->shapeStrokeColor.isValid())
                    sp.setPen(QPen(layer->shapeStrokeColor, layer->shapeStrokeWidth));
                else
                    sp.setPen(Qt::NoPen);
                sp.drawPath(layer->shapePath);
                sp.end();
                sourceImage = shapeImage;
            } else if (!layer->image.isNull()) {
                sourceImage = layer->image.convertToFormat(QImage::Format_ARGB32);
            }

            if (!sourceImage.isNull()) {
                for (int y = 0; y < sourceImage.height(); ++y) {
                    const auto *scanline = reinterpret_cast<const quint32 *>(sourceImage.constScanLine(y));
                    const int rowOffset = y * sourceImage.width();
                    for (int x = 0; x < sourceImage.width(); ++x) {
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

            if (layer->type == Layer::TextLayer) {
                if (layer->originalTySh.isValid()) {
                    ali.insert("TySh", layer->originalTySh);
                } else if (!layer->textRuns.isEmpty()) {
                    const auto &run = layer->textRuns.first();

                    QCborMap engineData;

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

                    QCborMap engineDict;
                    QCborMap editor;
                    editor.insert(u"Text"_s, QCborValue(run.text));
                    engineDict.insert(u"Editor"_s, QCborValue(editor));

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
                    styleSheetData.insert(u"Font"_s, QCborValue(0));
                    styleSheetData.insert(u"FontSize"_s, QCborValue(static_cast<double>(run.font.pointSizeF())));
                    QCborMap fillColor;
                    fillColor.insert(u"Type"_s, QCborValue(1));
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

                    QPsdDescriptor textDesc;
                    textDesc.setName(u""_s);
                    textDesc.setClassID("TxLr");
                    QHash<QByteArray, QVariant> textData;
                    textData.insert("Txt ", run.text);
                    textData.insert("EngineData", edBytes);
                    textDesc.setData(textData);

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

                    QPsdTypeToolObjectSetting tysh;
                    tysh.setTransform({1.0, 0.0, 0.0, 1.0, 0.0, 0.0});
                    tysh.setTextData(textDesc);
                    tysh.setWarpData(warpDesc);
                    tysh.setRect(QRect(0, 0, canvasSize.width(), canvasSize.height()));
                    ali.insert("TySh", QVariant::fromValue(tysh));
                }
            }

            if (layer->type == Layer::ShapeLayer) {
                if (layer->originalVmsk.isValid())
                    ali.insert("vmsk", layer->originalVmsk);
                if (layer->originalSoCo.isValid()) {
                    ali.insert("SoCo", layer->originalSoCo);
                } else if (layer->shapeFillColor.isValid()) {
                    QPsdDescriptor colorDesc;
                    colorDesc.setName(u""_s);
                    colorDesc.setClassID("RGBC");
                    QHash<QByteArray, QVariant> colorData;
                    colorData.insert("Rd  ", layer->shapeFillColor.redF() * 255.0);
                    colorData.insert("Grn ", layer->shapeFillColor.greenF() * 255.0);
                    colorData.insert("Bl  ", layer->shapeFillColor.blueF() * 255.0);
                    colorDesc.setData(colorData);

                    QPsdDescriptor socoDesc;
                    socoDesc.setName(u""_s);
                    socoDesc.setClassID("SoCo");
                    QHash<QByteArray, QVariant> socoData;
                    socoData.insert("Clr ", QVariant::fromValue(colorDesc));
                    socoDesc.setData(socoData);
                    ali.insert("SoCo", QVariant::fromValue(socoDesc));
                }
                if (layer->originalVstk.isValid())
                    ali.insert("vstk", layer->originalVstk);
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

    QPsdFileHeader header;
    header.setChannels(3);
    header.setWidth(size.width());
    header.setHeight(size.height());
    header.setDepth(8);
    header.setColorMode(QPsdFileHeader::RGB);

    QList<QPsdLayerRecord> records;
    QList<QPsdChannelImageData> channelDataList;
    collectLayerRecords(m_model, QModelIndex(), size, records, channelDataList);

    QPsdLayerInfo layerInfo;
    layerInfo.setRecords(records);
    layerInfo.setChannelImageData(channelDataList);

    QPsdLayerAndMaskInformation lmi;
    lmi.setLayerInfo(layerInfo);

    const QImage flat = m_model->flattenedImage().convertToFormat(QImage::Format_ARGB32);
    QByteArray compositeData;
    compositeData.reserve(3 * pixelCount);

    for (int y = 0; y < flat.height(); ++y) {
        const auto *scanline = reinterpret_cast<const quint32 *>(flat.constScanLine(y));
        for (int x = 0; x < flat.width(); ++x)
            compositeData.append(static_cast<char>((scanline[x] >> 16) & 0xFF));
    }
    for (int y = 0; y < flat.height(); ++y) {
        const auto *scanline = reinterpret_cast<const quint32 *>(flat.constScanLine(y));
        for (int x = 0; x < flat.width(); ++x)
            compositeData.append(static_cast<char>((scanline[x] >> 8) & 0xFF));
    }
    for (int y = 0; y < flat.height(); ++y) {
        const auto *scanline = reinterpret_cast<const quint32 *>(flat.constScanLine(y));
        for (int x = 0; x < flat.width(); ++x)
            compositeData.append(static_cast<char>(scanline[x] & 0xFF));
    }

    QPsdImageData imageData;
    imageData.setWidth(size.width());
    imageData.setHeight(size.height());
    imageData.setImageData(compositeData);

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

    std::function<void(const QModelIndex &, const QModelIndex &)> walkTree;
    walkTree = [&](const QModelIndex &sourceParent, const QModelIndex &destParent) {
        for (int row = guiModel.rowCount(sourceParent) - 1; row >= 0; --row) {
            const QModelIndex srcIdx = guiModel.index(row, 0, sourceParent);
            const auto folderType = guiModel.folderType(srcIdx);

            if (folderType == QPsdLayerTreeItemModel::FolderType::OpenFolder
                || folderType == QPsdLayerTreeItemModel::FolderType::ClosedFolder) {
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
                walkTree(srcIdx, destIdx);
            } else {
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
                    layer.hasAlpha = false;
                    for (const auto &ci : record->channelInfo()) {
                        if (ci.id() == QPsdChannelInfo::TransparencyMask) {
                            layer.hasAlpha = true;
                            break;
                        }
                    }
                    if (record->additionalLayerInformation().contains("TySh")) {
                        layer.type = Layer::TextLayer;
                        layer.originalTySh = record->additionalLayerInformation().value("TySh");
                        layer.textPosition = item->rect().topLeft();
                        if (item->type() == QPsdAbstractLayerItem::Text) {
                            const auto *textItem = static_cast<const QPsdTextLayerItem *>(item);
                            layer.textRuns = textItem->runs();
                            layer.textOrigin = textItem->textOrigin();
                        }
                    }

                    const auto &ali = record->additionalLayerInformation();
                    if (layer.type == Layer::ImageLayer
                        && item->type() == QPsdAbstractLayerItem::Shape) {
                        const auto *shapeItem = static_cast<const QPsdShapeLayerItem *>(item);
                        const auto pathData = shapeItem->pathInfo();
                        if (!pathData.path.isEmpty()) {
                            layer.type = Layer::ShapeLayer;

                            layer.shapePath = pathData.path.translated(item->rect().topLeft());
                            layer.shapeFillColor = shapeItem->brush().color();
                            if (shapeItem->pen().style() != Qt::NoPen) {
                                layer.shapeStrokeColor = shapeItem->pen().color();
                                layer.shapeStrokeWidth = shapeItem->pen().widthF();
                            }

                            if (ali.contains("vmsk"))
                                layer.originalVmsk = ali.value("vmsk");
                            if (ali.contains("vstk"))
                                layer.originalVstk = ali.value("vstk");
                            if (ali.contains("SoCo"))
                                layer.originalSoCo = ali.value("SoCo");
                        }
                    }
                }

                if (layer.type != Layer::TextLayer && layer.type != Layer::ShapeLayer) {
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

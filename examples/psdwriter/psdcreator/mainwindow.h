// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>

class Canvas;
class LayerModel;
class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QGroupBox;
class QSlider;
class QSpinBox;
class QLabel;
class QPushButton;
class QTreeView;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void newFile();
    void openFile();
    void saveFile();

    void onLayerSelectionChanged();
    void onOpacityChanged(int value);
    void onBlendModeChanged(int comboIndex);

    void addLayer();
    void addFolder();
    void addTextLayer();
    void addShapeLayer();
    void removeLayer();
    void replaceImage();

    void pickColor();

    void onLayerDoubleClicked(const QModelIndex &index);

private:
    void setupMenus();
    void setupLayersPanel();
    void setupPropertiesPanel();
    void updatePropertiesPanel();
    void saveToPsd(const QString &filePath);
    void loadFromPsd(const QString &filePath);
    void updateLayerControls();
    void editTextLayer(const QModelIndex &index);
    void editShapeLayer(const QModelIndex &index);
    void applyTextToolbarToLayer();
    void updateTextToolbar();

    LayerModel *m_model = nullptr;
    Canvas *m_canvas = nullptr;
    QTreeView *m_layerTree = nullptr;

    // Properties panel - Common
    QGroupBox *m_commonGroup = nullptr;
    QSlider *m_opacitySlider = nullptr;
    QLabel *m_opacityLabel = nullptr;
    QComboBox *m_blendCombo = nullptr;

    // Properties panel - Image
    QGroupBox *m_imageGroup = nullptr;
    QSpinBox *m_brushSizeSpin = nullptr;

    // Properties panel - Text
    QGroupBox *m_textGroup = nullptr;
    QFontComboBox *m_textFontCombo = nullptr;
    QSpinBox *m_textSizeSpin = nullptr;
    QPushButton *m_textColorBtn = nullptr;
    QPushButton *m_textBoldBtn = nullptr;
    QPushButton *m_textItalicBtn = nullptr;

    // Properties panel - Shape
    QGroupBox *m_shapeGroup = nullptr;
    QPushButton *m_shapeFillBtn = nullptr;
    QPushButton *m_shapeStrokeBtn = nullptr;
    QDoubleSpinBox *m_shapeStrokeWidthSpin = nullptr;
};

#endif // MAINWINDOW_H

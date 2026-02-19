// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>

class Canvas;
class LayerModel;
class QComboBox;
class QSlider;
class QSpinBox;
class QLabel;
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
    void removeLayer();
    void replaceImage();

    void pickColor();

    void onLayerDoubleClicked(const QModelIndex &index);

private:
    void setupMenus();
    void setupToolbar();
    void setupLayersPanel();
    void saveToPsd(const QString &filePath);
    void loadFromPsd(const QString &filePath);
    void updateLayerControls();
    void editTextLayer(const QModelIndex &index);

    LayerModel *m_model = nullptr;
    Canvas *m_canvas = nullptr;
    QTreeView *m_layerTree = nullptr;
    QSlider *m_opacitySlider = nullptr;
    QLabel *m_opacityLabel = nullptr;
    QComboBox *m_blendCombo = nullptr;
    QSpinBox *m_brushSizeSpin = nullptr;
};

#endif // MAINWINDOW_H

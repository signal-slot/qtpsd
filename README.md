# PSD Parser Module for Qt 6

A Qt 6 module for parsing, rendering, and exporting Adobe Photoshop PSD files. Provides a core parsing library, rendering components, and a plugin-based exporter with support for multiple UI frameworks.

## Features

- **Layer Parsing**: Extracts layers (text, shapes, images, folders, adjustment layers) in a tree structure
- **Effects Support**: Layer effects including shadows, bevels, glows, and more
- **Color Modes**: Bitmap, Grayscale, Indexed, RGB, CMYK, Lab, Multichannel, and Duotone (8/16/32-bit depths)
- **Rendering**: QGraphicsView-based rendering with 74% visual similarity to Photoshop ([similarity report](docs/similarity_report.md))
- **Export**: Plugin-based export to Flutter, Qt Quick, Slint, SwiftUI, React Native, LVGL, images, JSON, and custom templates
- **Plugin Architecture**: Extensible system for layer information, effects, descriptors, and exporters

## Modules

| Module | Description |
|--------|-------------|
| **PsdCore** | Core library for parsing PSD file structure |
| **PsdGui** | Rendering layer: converts parsed data to QImage, layer item hierarchy |
| **PsdWidget** | QGraphicsView-based scene for PSD visualization |
| **PsdExporter** | Export library with plugin infrastructure |

## Requirements

- Qt 6.8 or later

## Installation

```console
$ git clone https://github.com/signal-slot/qtpsd.git
$ cd qtpsd
$ mkdir build && cd build
$ cmake .. -DCMAKE_INSTALL_PREFIX=/path/to/your/qt/project
$ cmake --build .
$ cmake --install .
```

## Applications

### PSD Exporter

A GUI application for viewing and exporting PSD files:

```console
$ cmake .. -DCMAKE_INSTALL_PREFIX=/path/to/your/qt/project -DQT_BUILD_EXAMPLES=ON
$ cmake --build .
$ ./src/apps/psdexporter/psdexporter
```

### psd2png

Command-line tool for rendering PSD files to PNG:

```console
$ ./examples/psdwidget/psd2png/psd2png input.psd -o output.png
$ ./examples/psdwidget/psd2png/psd2png input.psd --image-data  # use baked image data
$ ./examples/psdwidget/psd2png/psd2png input.psd --layers       # print layer info
```

### psdinfo

Command-line utility to display PSD file metadata (dimensions, color mode, bit depth, channels):

```console
$ ./examples/psdcore/psdinfo/psdinfo input.psd
```

## Using QtPsd in Your Project

```cmake
find_package(Qt6 COMPONENTS PsdCore PsdGui PsdWidget PsdExporter REQUIRED)
target_link_libraries(your_app PRIVATE
    Qt::PsdCore
    Qt::PsdGui
    Qt::PsdWidget
    Qt::PsdExporter
)
```

## License

This module is licensed under the LGPLv3, GPLv2 or GPLv3 License.

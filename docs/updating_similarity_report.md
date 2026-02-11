# Updating the Similarity Report (psd-zoo)

## Policy

- Host: generate `Image Data`, `QPsdView`, and exporter outputs (`QtQuick`, `Slint`).
- Docker: convert exported `MainWindow.ui.qml` / `MainWindow.slint` to PNG screenshots only.
- Host: calculate similarity and regenerate `docs/similarity_report_psd-zoo.md`.

## 1) Host: refresh Image Data / QPsdView images

```bash
QTPSD_IMAGE_OUTPUT_PATH=docs \
QTPSD_IMAGE_SOURCES=psd-zoo \
./build/Qt_6-Debug/tests/auto/psdgui/image_data_to_image/tst_image_data_to_image generateImages

QTPSD_VIEW_OUTPUT_PATH=docs \
QTPSD_VIEW_SOURCES=psd-zoo \
QT_QPA_PLATFORM=offscreen \
./build/Qt_6-Debug/tests/auto/psdwidget/tst_qpsdview
```

## 2) Host: export QtQuick/Slint sources

```bash
./scripts/export_qtquick_slint_exports.sh ./build/Qt_6-Debug
```

This fills:
- `docs/exports/psd-zoo/**/QtQuick/MainWindow.ui.qml`
- `docs/exports/psd-zoo/**/Slint/MainWindow.slint`

## 3) Docker: capture exported UI to PNG

```bash
docker build -t qtpsd-sim -f docker/similarity/Dockerfile .
docker run --rm -v "$PWD":/workspace qtpsd-sim \
  /workspace/docker/similarity/run_similarity.sh psd-zoo
```

This fills:
- `docs/images/qtquick/psd-zoo/**/*.png`
- `docs/images/slint/psd-zoo/**/*.png`

## 4) Host: regenerate similarity report

```bash
QT_QPA_PLATFORM=offscreen \
QTPSD_SIMILARITY_OUTPUT_PATH=docs \
QTPSD_SIMILARITY_SOURCE=psd-zoo \
QTPSD_SIMILARITY_RUN_EXPORT=0 \
QTPSD_SIMILARITY_EXPORTERS=qtquick,slint \
./build/Qt_6-Debug/tests/auto/psdexporter/similarity/tst_psdexporter_similarity generateReport
```

## Sanity check

- `find docs/images/qtquick/psd-zoo -type f | wc -l`
- `find docs/images/slint/psd-zoo -type f | wc -l`
- Confirm `docs/similarity_report_psd-zoo.md` has no `MISSING` for QtQuick/Slint rows.

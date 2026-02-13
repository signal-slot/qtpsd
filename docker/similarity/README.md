# Similarity Docker Environment

This image is used only for screenshot capture:
- Input: host-exported QtQuick/Slint/Flutter files under `docs/exports/<source-id>/`
- Output: PNG captures under `docs/images/{qtquick,slint,flutter}/<source-id>/`
- QtQuick capture uses `qml` (not `qmlscene`)

`run_similarity.sh` does not run comparison and does not run `psdexporter`.

## Build

```bash
docker build -t qtpsd-sim -f docker/similarity/Dockerfile .
```

By default, Flutter is resolved from the current stable channel at build time.
Set `FLUTTER_VERSION` to pin a specific version.

```bash
docker build --build-arg FLUTTER_VERSION=3.35.4 -t qtpsd-sim -f docker/similarity/Dockerfile .
```

## Run capture

```bash
docker run --rm -v "$PWD":/workspace qtpsd-sim \
  /workspace/docker/similarity/run_similarity.sh psd-zoo
```

The script expects these files to exist first:
- `docs/exports/psd-zoo/**/QtQuick/MainWindow.ui.qml`
- `docs/exports/psd-zoo/**/Slint/MainWindow.slint`
- `docs/exports/psd-zoo/**/Flutter/main_window.dart`

# Similarity Docker Environment

This image is used only for screenshot capture:
- Input: host-exported QtQuick/Slint files under `docs/exports/<source-id>/`
- Output: PNG captures under `docs/images/qtquick/<source-id>/` and `docs/images/slint/<source-id>/`
- QtQuick capture uses `qml` (not `qmlscene`)

`run_similarity.sh` does not run comparison and does not run `psdexporter`.

## Build

```bash
docker build -t qtpsd-sim -f docker/similarity/Dockerfile .
```

## Run capture

```bash
docker run --rm -v "$PWD":/workspace qtpsd-sim \
  /workspace/docker/similarity/run_similarity.sh psd-zoo
```

The script expects these files to exist first:
- `docs/exports/psd-zoo/**/QtQuick/MainWindow.ui.qml`
- `docs/exports/psd-zoo/**/Slint/MainWindow.slint`

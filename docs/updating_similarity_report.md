# Updating the Similarity Report (psd-zoo)

## Policy

- Host: generate `Image Data`, `QPsdView`, and exporter outputs (`QtQuick`, `Slint`, `Flutter`, `LVGL`).
- Docker: convert exported `MainWindow.ui.qml` / `MainWindow.slint` / `main_window.dart` / `MainScreen.xml` to PNG screenshots only.
- Host: calculate similarity and regenerate `docs/similarity_report_psd-zoo.md`.

## One-command update

```bash
./scripts/update_similarity_report.sh
```

This script runs:
1. `tst_image_data_to_image generateImages`
2. `tst_qpsdview`
3. `scripts/export_qtquick_slint_exports.sh`
4. `docker/similarity/run_similarity.sh` (inside `qtpsd-sim`)
5. `tst_psdexporter_similarity generateReport`

Report only (reuse existing PNG/export artifacts):

```bash
./scripts/update_similarity_report.sh --report-only
```

## Common options

```bash
# Rebuild Docker image before capture
./scripts/update_similarity_report.sh --rebuild-docker

# Use a different build directory
./scripts/update_similarity_report.sh --build-dir ./build/Qt_6-Release

# Limit number of PSDs processed
QTPSD_SIMILARITY_LIMIT=20 ./scripts/update_similarity_report.sh
```

## Outputs

- Exported UI sources: `docs/exports/psd-zoo/**/{QtQuick,Slint,Flutter,LVGL}/...`
- Captured PNGs:
  - `docs/images/qtquick/psd-zoo/**/*.png`
  - `docs/images/slint/psd-zoo/**/*.png`
  - `docs/images/flutter/psd-zoo/**/*.png`
  - `docs/images/lvgl/psd-zoo/**/*.png`
- Report: `docs/similarity_report_psd-zoo.md`

## Sanity check

- `find docs/images/qtquick/psd-zoo -type f | wc -l`
- `find docs/images/slint/psd-zoo -type f | wc -l`
- `find docs/images/flutter/psd-zoo -type f | wc -l`
- `find docs/images/lvgl/psd-zoo -type f | wc -l`
- Confirm `docs/similarity_report_psd-zoo.md` has links and percentages for `QPsdView`, `QtQuick`, `Slint`, `Flutter`, `LVGL`.

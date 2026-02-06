# Updating the Similarity Report

The similarity report compares QPsdView rendering output against Photoshop reference images.

## Prerequisites

From the build directory:

```bash
export LD_LIBRARY_PATH=~/io/qt/release/6/gcc_64/lib:$(pwd)/lib:$LD_LIBRARY_PATH
export QT_QPA_PLATFORM=offscreen
```

## Generate Report

```bash
QTPSD_SIMILARITY_SUMMARY_PATH=../docs ./tests/auto/psdwidget/tst_qpsdview
```

## Output Files

- `docs/similarity_report.md` - Comparison table with similarity percentages
- `docs/images/psdview/` - QPsdView rendered images
- `docs/images/imagedata/` - Image data rendered images
- `docs/images/psd2png/` - Reference images from Photoshop

## Notes

- Text rendering results may vary depending on installed system fonts
- Run from the build directory (e.g., `qtpsd/build`)

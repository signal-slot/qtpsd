#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <input_main_window.dart> <output.png>" >&2
  exit 2
fi

input_dart="$(realpath -m "$1")"
output_png="$(realpath -m "$2")"

if [[ ! -f "${input_dart}" ]]; then
  echo "Flutter entry file is missing: ${input_dart}" >&2
  exit 1
fi

if ! command -v flutter >/dev/null 2>&1; then
  echo "flutter command is not found" >&2
  exit 1
fi

export_dir="$(dirname "${input_dart}")"
entry_file="$(basename "${input_dart}")"

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/qtpsd-flutter-XXXXXX")"
log_file="${work_dir}/flutter.log"
pub_timeout_seconds="${QTPSD_FLUTTER_PUB_TIMEOUT:-180}"
test_timeout_seconds="${QTPSD_FLUTTER_TEST_TIMEOUT:-240}"
keep_work_dir=0

cleanup() {
  if [[ "${keep_work_dir}" -eq 0 ]]; then
    rm -rf "${work_dir}"
  fi
}
trap cleanup EXIT

mkdir -p "${work_dir}/lib/export/assets/images"
cp -a "${export_dir}/." "${work_dir}/lib/export/"
mkdir -p "${work_dir}/assets"
rm -f "${work_dir}/assets/images"
ln -s "${work_dir}/lib/export/assets/images" "${work_dir}/assets/images"
mkdir -p "$(dirname "${output_png}")"

# Generated Flutter exports can contain empty placeholder nodes:
#   (
#   ),
# Remove them before compiling the temporary capture project.
while IFS= read -r dart_file; do
  perl -0pi -e 's/^\h*\(\h*\n\h*\),\h*\n//mg' "${dart_file}"
  # Some exports emit a named-record literal where Flutter expects Decoration.
  perl -0pi -e 's/(\b(?:decoration|foregroundDecoration):\s*)\(/\1BoxDecoration(/g' "${dart_file}"
done < <(find "${work_dir}/lib/export" -type f -name '*.dart' | sort)

cat >"${work_dir}/pubspec.yaml" <<'YAML'
name: qtpsd_capture
description: Temporary project for QtPsd screenshot capture
publish_to: "none"
environment:
  sdk: ">=3.3.0 <4.0.0"
dependencies:
  flutter:
    sdk: flutter
dev_dependencies:
  flutter_test:
    sdk: flutter
flutter:
  uses-material-design: true
  assets:
    - assets/images/
YAML

cat >"${work_dir}/test_capture.dart" <<DART
import 'dart:io';
import 'dart:typed_data';
import 'dart:ui' as ui;
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:qtpsd_capture/export/${entry_file}' as exported;

Future<void> _loadFonts() async {
  final fontFiles = [
    '/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF/Roboto-Regular.ttf',
    '/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF/Roboto-Bold.ttf',
    '/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF/Roboto-Italic.ttf',
    '/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF/Roboto-BoldItalic.ttf',
    '/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF/Roboto-Light.ttf',
    '/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF/Roboto-Medium.ttf',
    '/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf',
    '/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf',
    '/usr/share/fonts/truetype/noto/NotoSans-Italic.ttf',
    '/usr/share/fonts/truetype/noto/NotoSans-BoldItalic.ttf',
    '/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf',
    '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
    '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',
  ];
  for (final path in fontFiles) {
    final file = File(path);
    if (await file.exists()) {
      try {
        final bytes = await file.readAsBytes();
        await ui.loadFontFromList(Uint8List.fromList(bytes));
      } catch (_) {}
    }
  }
}

Future<void> _flushFrames() async {
  final binding = WidgetsBinding.instance;
  for (var i = 0; i < 8; i++) {
    binding.scheduleFrame();
    await binding.endOfFrame;
    await Future<void>.delayed(const Duration(milliseconds: 50));
  }
}

void main() {
  WidgetsFlutterBinding.ensureInitialized();

  test('capture exported MainWindow', () async {
    await _loadFonts();

    final boundaryKey = GlobalKey();

    runApp(
      MaterialApp(
        debugShowCheckedModeBanner: false,
        home: Material(
          child: Center(
            child: RepaintBoundary(
              key: boundaryKey,
              child: exported.MainWindow(),
            ),
          ),
        ),
      ),
    );

    await _flushFrames();

    final context = boundaryKey.currentContext;
    if (context == null) {
      fail('Failed to find capture boundary context');
    }

    final boundary = context!.findRenderObject() as RenderRepaintBoundary;
    if (boundary.size.isEmpty) {
      fail('Rendered Flutter widget has empty size');
    }

    final image = await boundary.toImage(pixelRatio: 1.0);
    final bytes = await image.toByteData(format: ui.ImageByteFormat.png);
    image.dispose();
    if (bytes == null) {
      fail('Failed to encode Flutter screenshot as PNG');
    }

    final outPath = const String.fromEnvironment('QTPSD_OUT_PNG');
    if (outPath.isEmpty) {
      fail('QTPSD_OUT_PNG is not set');
    }

    final outFile = File(outPath);
    outFile.parent.createSync(recursive: true);
    outFile.writeAsBytesSync(bytes!.buffer.asUint8List(), flush: true);
    expect(bytes.lengthInBytes, greaterThan(0));
  });
}
DART

if ! (
  cd "${work_dir}"
  export FLUTTER_SUPPRESS_ANALYTICS=true
  export PUB_ENVIRONMENT=bot.cli
  timeout "${pub_timeout_seconds}" flutter pub get >"${log_file}" 2>&1
  timeout "${test_timeout_seconds}" flutter test --no-pub test_capture.dart \
    --dart-define=QTPSD_OUT_PNG="${output_png}" \
    >>"${log_file}" 2>&1
); then
  echo "Flutter capture failed: ${input_dart}" >&2
  keep_work_dir=1
  echo "Temporary capture project is kept at: ${work_dir}" >&2
  tail -n 200 "${log_file}" >&2 || true
  exit 1
fi

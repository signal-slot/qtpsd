#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <input.qml> <output.png>" >&2
  exit 2
fi

if [[ -z "${DISPLAY:-}" ]]; then
  echo "DISPLAY is not set. Run this script under Xvfb or a desktop session." >&2
  exit 1
fi

input_qml="$1"
output_png="$2"
capture_delay="${QML_CAPTURE_DELAY:-0.25}"

viewer_bin="${QML_VIEWER_BIN:-}"
if [[ -z "${viewer_bin}" ]]; then
  for candidate in qml6 qml; do
    if command -v "${candidate}" >/dev/null 2>&1; then
      viewer_bin="${candidate}"
      break
    fi
  done
fi

if [[ -z "${viewer_bin}" ]]; then
  echo "No QML previewer found (tried qml6, qml)" >&2
  exit 1
fi

if ! command -v xdotool >/dev/null 2>&1; then
  echo "xdotool is required" >&2
  exit 1
fi

if ! command -v import >/dev/null 2>&1; then
  echo "ImageMagick 'import' is required" >&2
  exit 1
fi

mkdir -p "$(dirname "${output_png}")"

log_file="${TMPDIR:-/tmp}/qml-preview-$$.log"
"${viewer_bin}" "${input_qml}" >"${log_file}" 2>&1 &
viewer_pid=$!

cleanup() {
  kill "${viewer_pid}" >/dev/null 2>&1 || true
  wait "${viewer_pid}" >/dev/null 2>&1 || true
  rm -f "${log_file}"
}
trap cleanup EXIT

window_id=""
for _ in $(seq 1 200); do
  window_id="$(xdotool search --onlyvisible --pid "${viewer_pid}" 2>/dev/null | head -n1 || true)"
  if [[ -n "${window_id}" ]]; then
    break
  fi
  sleep 0.05
done

if [[ -z "${window_id}" ]]; then
  echo "Failed to detect QML preview window" >&2
  echo "QML previewer log:" >&2
  cat "${log_file}" >&2 || true
  exit 1
fi

sleep "${capture_delay}"
import -window "${window_id}" "${output_png}"

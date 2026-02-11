#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <input.slint> <output.png>" >&2
  exit 2
fi

if [[ -z "${DISPLAY:-}" ]]; then
  echo "DISPLAY is not set. Run this script under Xvfb or a desktop session." >&2
  exit 1
fi

input_slint="$1"
output_png="$2"
viewer_bin="${SLINT_VIEWER_BIN:-slint-viewer}"
capture_delay="${SLINT_CAPTURE_DELAY:-0.25}"

if ! command -v "${viewer_bin}" >/dev/null 2>&1; then
  echo "slint-viewer is not found: ${viewer_bin}" >&2
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

log_file="${TMPDIR:-/tmp}/slint-viewer-$$.log"
"${viewer_bin}" "${input_slint}" >"${log_file}" 2>&1 &
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
  echo "Failed to detect slint-viewer window" >&2
  echo "slint-viewer log:" >&2
  cat "${log_file}" >&2 || true
  exit 1
fi

sleep "${capture_delay}"
import -window "${window_id}" "${output_png}"

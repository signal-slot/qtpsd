#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <input MainScreen.xml> <output.png>" >&2
  exit 2
fi

input_xml="$(realpath -m "$1")"
output_png="$(realpath -m "$2")"
export_dir="$(dirname "${input_xml}")"

if [[ ! -f "${input_xml}" ]]; then
  echo "LVGL XML file is missing: ${input_xml}" >&2
  exit 1
fi

# Find the lvgl_capture binary
lvgl_capture=""
for candidate in \
  /usr/local/bin/lvgl_capture \
  "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)/tools/lvgl_capture/build/lvgl_capture"
do
  if [[ -x "${candidate}" ]]; then
    lvgl_capture="${candidate}"
    break
  fi
done

if [[ -z "${lvgl_capture}" ]]; then
  echo "lvgl_capture binary not found" >&2
  exit 1
fi

mkdir -p "$(dirname "${output_png}")"
"${lvgl_capture}" "${export_dir}" "${output_png}"

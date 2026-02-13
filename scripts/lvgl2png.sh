#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <input MainScreen.xml> <output.png>" >&2
  exit 2
fi

input_xml="$(realpath -m "$1")"
output_png="$(realpath -m "$2")"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
renderer_py="${script_dir}/lvgl2png.py"

if [[ ! -f "${input_xml}" ]]; then
  echo "LVGL XML file is missing: ${input_xml}" >&2
  exit 1
fi

if [[ ! -f "${renderer_py}" ]]; then
  echo "Renderer script is missing: ${renderer_py}" >&2
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 command is not found" >&2
  exit 1
fi

mkdir -p "$(dirname "${output_png}")"
python3 "${renderer_py}" "${input_xml}" "${output_png}"

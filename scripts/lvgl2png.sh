#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <input main_screen.c> <output.png>" >&2
  exit 2
fi

input_c="$(realpath -m "$1")"
output_png="$(realpath -m "$2")"
export_dir="$(dirname "${input_c}")"

if [[ ! -f "${input_c}" ]]; then
  echo "Generated LVGL source is missing: ${input_c}" >&2
  exit 1
fi

# Locate the capture harness build (see tools/lvgl_capture)
capture_env=""
for candidate in \
  "${LVGL_CAPTURE_ENV:-}" \
  "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)/tools/lvgl_capture/build/capture_env.sh" \
  /opt/lvgl_capture_build/capture_env.sh
do
  if [[ -n "${candidate}" && -f "${candidate}" ]]; then
    capture_env="${candidate}"
    break
  fi
done

if [[ -z "${capture_env}" ]]; then
  echo "lvgl_capture build not found; build tools/lvgl_capture first" >&2
  exit 1
fi

# shellcheck source=/dev/null
source "${capture_env}"

tmp_dir="$(mktemp -d)"
trap 'rm -rf "${tmp_dir}"' EXIT

cc -O1 -o "${tmp_dir}/capture" \
  -I"${LVGL_SRC_DIR}" \
  -I"${export_dir}" \
  -DLV_CONF_PATH="\"${LV_CONF}\"" \
  "${input_c}" \
  -Wl,--whole-archive "${HARNESS_LIB}" -Wl,--no-whole-archive \
  "${LVGL_LIB}" \
  -lm -lpthread

mkdir -p "$(dirname "${output_png}")"
"${tmp_dir}/capture" "${export_dir}" "${output_png}"

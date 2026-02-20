#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"

build_dir="${1:-${repo_root}/build/Qt_6-Debug}"
source_root="${2:-${repo_root}/tests/auto/3rdparty/psd-zoo}"
export_root="${3:-${repo_root}/docs/exports/psd-zoo}"

psdexporter_bin=""
for candidate in \
  "${build_dir}/lib64/qt6/bin/psdexporter" \
  "${build_dir}/bin/psdexporter"; do
  if [[ -x "${candidate}" ]]; then
    psdexporter_bin="${candidate}"
    break
  fi
done

if [[ -z "${psdexporter_bin}" ]]; then
  echo "Missing psdexporter binary under ${build_dir}" >&2
  exit 1
fi

if [[ ! -d "${source_root}" ]]; then
  echo "Missing PSD source directory: ${source_root}" >&2
  exit 1
fi

limit="${QTPSD_SIMILARITY_LIMIT:-0}"
if ! [[ "${limit}" =~ ^[0-9]+$ ]]; then
  echo "QTPSD_SIMILARITY_LIMIT must be a non-negative integer" >&2
  exit 1
fi

mkdir -p "${export_root}"

mapfile -d '' psd_files < <(find "${source_root}" -type f -name '*.psd' -print0 | sort -z)

if [[ "${limit}" -gt 0 && "${#psd_files[@]}" -gt "${limit}" ]]; then
  psd_files=("${psd_files[@]:0:${limit}}")
fi

max_jobs="${QTPSD_SIMILARITY_JOBS:-$(nproc)}"

echo "Export source: ${source_root}"
echo "Export output: ${export_root}"
echo "PSD count: ${#psd_files[@]}"
echo "Max parallel jobs: ${max_jobs}"

fail_file="$(mktemp)"
echo 0 > "${fail_file}"
trap 'rm -f "${fail_file}"' EXIT

qt_plugin_path="${build_dir}/lib64/qt6/plugins"

run_export() {
  local exporter_type="$1"
  local psd_path="$2"
  local out_dir="$3"
  local -a env_args=("QT_QPA_PLATFORM=offscreen")
  if [[ -d "${qt_plugin_path}" ]]; then
    env_args+=("QT_PLUGIN_PATH=${qt_plugin_path}${QT_PLUGIN_PATH:+:${QT_PLUGIN_PATH}}")
  fi
  if ! env "${env_args[@]}" "${psdexporter_bin}" \
      --input "${psd_path}" \
      --type "${exporter_type}" \
      --outdir "${out_dir}" \
      --resolution original 2>/dev/null; then
    echo "Export failed: ${exporter_type} ${psd_path}" >&2
    # Atomically increment fail count via flock
    flock "${fail_file}" bash -c "echo \$(( \$(cat '${fail_file}') + 1 )) > '${fail_file}'"
  fi
}

job_count=0
for psd_path in "${psd_files[@]}"; do
  rel_path="${psd_path#"${source_root}/"}"
  rel_no_ext="${rel_path%.psd}"

  qtquick_out="${export_root}/${rel_no_ext}/QtQuick"
  slint_out="${export_root}/${rel_no_ext}/Slint"
  flutter_out="${export_root}/${rel_no_ext}/Flutter"
  lvgl_out="${export_root}/${rel_no_ext}/LVGL"

  rm -rf "${qtquick_out}" "${slint_out}" "${flutter_out}" "${lvgl_out}"
  mkdir -p "${qtquick_out}" "${slint_out}" "${flutter_out}" "${lvgl_out}"

  for exporter_args in \
    "QtQuick|${psd_path}|${qtquick_out}" \
    "Slint|${psd_path}|${slint_out}" \
    "Flutter|${psd_path}|${flutter_out}" \
    "LVGL|${psd_path}|${lvgl_out}"; do
    IFS='|' read -r etype epath eout <<< "${exporter_args}"
    run_export "${etype}" "${epath}" "${eout}" &
    job_count=$((job_count + 1))
    if [[ "${job_count}" -ge "${max_jobs}" ]]; then
      wait -n 2>/dev/null || true
      job_count=$((job_count - 1))
    fi
  done
done
wait

fail_count="$(cat "${fail_file}")"
if [[ "${fail_count}" -gt 0 ]]; then
  echo "Host export completed with ${fail_count} failed exporter runs." >&2
else
  echo "Host export completed."
fi

#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  ./scripts/update_similarity_report.sh [options]

Options:
  --build-dir <path>      Build directory (default: ./build/Qt_6-Debug)
  --source-id <id>        Similarity source id (default: psd-zoo)
  --source-root <path>    PSD source root (default: ./tests/auto/3rdparty/<source-id>)
  --export-root <path>    Export root (default: ./docs/exports/<source-id>)
  --report-only           Update report only (skip image generation/export/capture)
  --docker-image <name>   Docker image for capture (default: qtpsd-sim)
  --dockerfile <path>     Dockerfile path (default: ./docker/similarity/Dockerfile)
  --rebuild-docker        Force docker image rebuild before capture
  -h, --help              Show this help

Environment:
  QTPSD_SIMILARITY_LIMIT         Optional PSD file limit
  QTPSD_SIMILARITY_EXPORTERS     Exporter list for report (default: qtquick,slint,flutter,lvgl)
EOF
}

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"

build_dir="${repo_root}/build/Qt_6-Debug"
source_id="psd-zoo"
source_root=""
export_root=""
docker_image="qtpsd-sim"
dockerfile="${repo_root}/docker/similarity/Dockerfile"
rebuild_docker=0
report_only=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      build_dir="$2"
      shift 2
      ;;
    --source-id)
      source_id="$2"
      shift 2
      ;;
    --source-root)
      source_root="$2"
      shift 2
      ;;
    --export-root)
      export_root="$2"
      shift 2
      ;;
    --report-only)
      report_only=1
      shift
      ;;
    --docker-image)
      docker_image="$2"
      shift 2
      ;;
    --dockerfile)
      dockerfile="$2"
      shift 2
      ;;
    --rebuild-docker)
      rebuild_docker=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

build_dir="$(cd -- "${build_dir}" && pwd)"
docs_dir="${repo_root}/docs"
source_root="${source_root:-${repo_root}/tests/auto/3rdparty/${source_id}}"
export_root="${export_root:-${docs_dir}/exports/${source_id}}"
dockerfile="$(cd -- "$(dirname -- "${dockerfile}")" && pwd)/$(basename -- "${dockerfile}")"

image_data_test="${build_dir}/tests/auto/psdgui/image_data_to_image/tst_image_data_to_image"
psdview_test="${build_dir}/tests/auto/psdwidget/tst_qpsdview"
similarity_test="${build_dir}/tests/auto/psdexporter/similarity/tst_psdexporter_similarity"
export_script="${repo_root}/scripts/export_qtquick_slint_exports.sh"

required_bins=("${similarity_test}")
if [[ "${report_only}" -eq 0 ]]; then
  required_bins+=("${image_data_test}" "${psdview_test}" "${export_script}")
fi

for required in "${required_bins[@]}"; do
  if [[ ! -x "${required}" ]]; then
    echo "Missing executable: ${required}" >&2
    exit 1
  fi
done

if [[ ! -d "${source_root}" ]]; then
  echo "Missing PSD source directory: ${source_root}" >&2
  exit 1
fi

if [[ "${report_only}" -eq 0 && ! -f "${dockerfile}" ]]; then
  echo "Missing Dockerfile: ${dockerfile}" >&2
  exit 1
fi

run_cmd() {
  echo "+ $*"
  "$@"
}

ensure_docker_image() {
  if [[ "${rebuild_docker}" -eq 1 ]]; then
    run_cmd docker build -t "${docker_image}" -f "${dockerfile}" "${repo_root}"
    return
  fi
  if docker image inspect "${docker_image}" >/dev/null 2>&1; then
    return
  fi
  run_cmd docker build -t "${docker_image}" -f "${dockerfile}" "${repo_root}"
}

qt_plugin_path="${build_dir}/lib64/qt6/plugins"
qt_plugin_env=()
if [[ -d "${qt_plugin_path}" ]]; then
  qt_plugin_env=("QT_PLUGIN_PATH=${qt_plugin_path}${QT_PLUGIN_PATH:+:${QT_PLUGIN_PATH}}")
fi

echo "== Similarity pipeline =="
echo "Source id:      ${source_id}"
echo "Build dir:      ${build_dir}"
echo "PSD source:     ${source_root}"
echo "Export root:    ${export_root}"
echo "Docker image:   ${docker_image}"
echo "Report only:    ${report_only}"

if [[ "${report_only}" -eq 0 ]]; then
  echo "== Step 1/5: Host image generation (Image Data, QPsdView) =="
  run_cmd env "${qt_plugin_env[@]}" \
    QTPSD_IMAGE_OUTPUT_PATH="${docs_dir}" \
    QTPSD_IMAGE_SOURCES="${source_id}" \
    QT_QPA_PLATFORM=offscreen \
    "${image_data_test}" generateImages

  run_cmd env "${qt_plugin_env[@]}" \
    QTPSD_VIEW_OUTPUT_PATH="${docs_dir}" \
    QTPSD_VIEW_SOURCES="${source_id}" \
    QT_QPA_PLATFORM=offscreen \
    "${psdview_test}"

  echo "== Step 2/5: Host export (QtQuick/Slint/Flutter/LVGL sources) =="
  run_cmd env "${qt_plugin_env[@]}" \
    "${export_script}" "${build_dir}" "${source_root}" "${export_root}"

  echo "== Step 3/5: Clean previous captured PNGs for ${source_id} =="
  rm -rf \
    "${docs_dir}/images/qtquick/${source_id}" \
    "${docs_dir}/images/slint/${source_id}" \
    "${docs_dir}/images/flutter/${source_id}" \
    "${docs_dir}/images/lvgl/${source_id}"

  echo "== Step 4/5: Docker capture to PNG (QtQuick/Slint/Flutter/LVGL) =="
  ensure_docker_image
  run_cmd docker run --rm --user "$(id -u):$(id -g)" -v "${repo_root}:/workspace" "${docker_image}" \
    /workspace/docker/similarity/run_similarity.sh "${source_id}" /workspace
fi

echo "== Step 5/5: Host similarity calculation and report generation =="
run_cmd env "${qt_plugin_env[@]}" \
  QT_QPA_PLATFORM=offscreen \
  QTPSD_SIMILARITY_OUTPUT_PATH="${docs_dir}" \
  QTPSD_SIMILARITY_SOURCE="${source_id}" \
  QTPSD_SIMILARITY_RUN_EXPORT=0 \
  QTPSD_SIMILARITY_EXPORTERS="${QTPSD_SIMILARITY_EXPORTERS:-qtquick,slint,flutter,lvgl}" \
  "${similarity_test}" generateReport

echo "Completed: ${docs_dir}/similarity_report_${source_id}.md"

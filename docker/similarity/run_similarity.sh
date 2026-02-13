#!/usr/bin/env bash
set -euo pipefail

source_id="${1:-psd-zoo}"
workspace_root="${2:-/workspace}"
export_root="${workspace_root}/docs/exports/${source_id}"
qtquick_output_root="${workspace_root}/docs/images/qtquick/${source_id}"
slint_output_root="${workspace_root}/docs/images/slint/${source_id}"
flutter_output_root="${workspace_root}/docs/images/flutter/${source_id}"
qml_capture_script="${workspace_root}/scripts/qml2png.sh"
slint_capture_script="${workspace_root}/scripts/slint2png.sh"
flutter_capture_script="${workspace_root}/scripts/flutter2png.sh"
managed_xvfb=0
xvfb_display=""
xvfb_num=""
xvfb_log=""
xvfb_pid=""

# Ubuntu Qt6 tools are often installed under /usr/lib/qt6/bin.
if [[ -d "/usr/lib/qt6/bin" ]]; then
  export PATH="/usr/lib/qt6/bin:${PATH}"
fi

if [[ ! -d "${export_root}" ]]; then
  echo "Missing export directory: ${export_root}" >&2
  exit 1
fi

if [[ ! -x "${qml_capture_script}" ]]; then
  echo "Missing script: ${qml_capture_script}" >&2
  exit 1
fi

if [[ ! -x "${slint_capture_script}" ]]; then
  echo "Missing script: ${slint_capture_script}" >&2
  exit 1
fi

if [[ ! -x "${flutter_capture_script}" ]]; then
  echo "Missing script: ${flutter_capture_script}" >&2
  exit 1
fi

start_xvfb() {
  Xvfb "${xvfb_display}" -screen 0 1280x1024x24 -nolisten tcp >"${xvfb_log}" 2>&1 &
  xvfb_pid=$!
  for _ in $(seq 1 200); do
    if [[ -S "/tmp/.X11-unix/X${xvfb_num}" ]]; then
      return 0
    fi
    sleep 0.05
  done
  echo "Failed to start Xvfb on ${xvfb_display}" >&2
  sed -n '1,80p' "${xvfb_log}" >&2 || true
  return 1
}

stop_xvfb() {
  if [[ -n "${xvfb_pid}" ]]; then
    kill "${xvfb_pid}" >/dev/null 2>&1 || true
    wait "${xvfb_pid}" >/dev/null 2>&1 || true
    xvfb_pid=""
  fi
}

ensure_xvfb_running() {
  if [[ "${managed_xvfb}" -eq 0 ]]; then
    return 0
  fi
  if [[ -n "${xvfb_pid}" ]] && kill -0 "${xvfb_pid}" >/dev/null 2>&1 && [[ -S "/tmp/.X11-unix/X${xvfb_num}" ]]; then
    return 0
  fi
  stop_xvfb
  start_xvfb
}

if [[ -z "${DISPLAY:-}" ]]; then
  if ! command -v Xvfb >/dev/null 2>&1; then
    echo "Xvfb is required in the container" >&2
    exit 1
  fi
  managed_xvfb=1
  xvfb_display="${QTPSD_XVFB_DISPLAY:-:99}"
  xvfb_num="${xvfb_display#:}"
  xvfb_log="${TMPDIR:-/tmp}/xvfb-${xvfb_num}.log"
  start_xvfb
  trap stop_xvfb EXIT
  export DISPLAY="${xvfb_display}"
fi

capture_one() {
  local input="$1"
  local output="$2"
  local script="$3"
  mkdir -p "$(dirname "${output}")"
  ensure_xvfb_running
  if "${script}" "${input}" "${output}"; then
    return 0
  fi
  if [[ "${managed_xvfb}" -eq 1 ]]; then
    echo "Retry after restarting Xvfb: ${input}" >&2
    stop_xvfb
    start_xvfb
    "${script}" "${input}" "${output}"
    return $?
  fi
  return 1
}

capture_all() {
  local kind="$1"
  local input_name="$2"
  local output_root="$3"
  local script="$4"

  local count=0
  while IFS= read -r input_path; do
    local rel_no_ext
    rel_no_ext="${input_path#"${export_root}/"}"
    rel_no_ext="${rel_no_ext%/${kind}/${input_name}}"
    local output_path="${output_root}/${rel_no_ext}.png"

    echo "[${kind}] ${rel_no_ext}"
    capture_one "${input_path}" "${output_path}" "${script}"
    count=$((count + 1))
  done < <(find "${export_root}" -type f -path "*/${kind}/${input_name}" | sort)

  echo "Captured ${count} ${kind} screenshots."
}

echo "Export root: ${export_root}"
echo "QtQuick png output: ${qtquick_output_root}"
echo "Slint png output: ${slint_output_root}"
echo "Flutter png output: ${flutter_output_root}"

export QT_QPA_PLATFORM=xcb
export QML_VIEWER_BIN="${QML_VIEWER_BIN:-qml}"
capture_all "QtQuick" "MainWindow.ui.qml" "${qtquick_output_root}" "${qml_capture_script}"
capture_all "Slint" "MainWindow.slint" "${slint_output_root}" "${slint_capture_script}"
capture_all "Flutter" "main_window.dart" "${flutter_output_root}" "${flutter_capture_script}"

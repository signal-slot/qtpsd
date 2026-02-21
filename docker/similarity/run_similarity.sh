#!/usr/bin/env bash
set -euo pipefail

source_id="${1:-psd-zoo}"
workspace_root="${2:-/workspace}"
export_root="${workspace_root}/docs/exports/${source_id}"
qtquick_output_root="${workspace_root}/docs/images/qtquick/${source_id}"
slint_output_root="${workspace_root}/docs/images/slint/${source_id}"
flutter_output_root="${workspace_root}/docs/images/flutter/${source_id}"
lvgl_output_root="${workspace_root}/docs/images/lvgl/${source_id}"
qml_capture_script="${workspace_root}/scripts/qml2png.sh"
slint_capture_script="${workspace_root}/scripts/slint2png.sh"
flutter_capture_script="${workspace_root}/scripts/flutter2png.sh"
lvgl_capture_script="${workspace_root}/scripts/lvgl2png.sh"
managed_xvfb=0
xvfb_display=""
xvfb_num=""
xvfb_log=""
xvfb_pid=""
compmgr_pid=""

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

if [[ ! -x "${lvgl_capture_script}" ]]; then
  echo "Missing script: ${lvgl_capture_script}" >&2
  exit 1
fi

start_xvfb() {
  Xvfb "${xvfb_display}" -screen 0 1280x1024x24 -nolisten tcp +extension Composite >"${xvfb_log}" 2>&1 &
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
  if [[ -n "${compmgr_pid}" ]]; then
    kill "${compmgr_pid}" >/dev/null 2>&1 || true
    wait "${compmgr_pid}" >/dev/null 2>&1 || true
    compmgr_pid=""
  fi
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
  if command -v xcompmgr >/dev/null 2>&1; then
    xcompmgr >/dev/null 2>&1 &
    compmgr_pid=$!
    sleep 0.3
  fi
fi

capture_one() {
  local input="$1"
  local output="$2"
  local script="$3"
  mkdir -p "$(dirname "${output}")"
  if "${script}" "${input}" "${output}"; then
    return 0
  fi
  return 1
}

max_capture_jobs="${QTPSD_CAPTURE_JOBS:-4}"
fail_file="$(mktemp)"
echo 0 > "${fail_file}"
ok_file="$(mktemp)"
echo 0 > "${ok_file}"
orig_trap="$(trap -p EXIT | sed "s/^trap -- '//;s/' EXIT$//")"
trap "${orig_trap}; rm -f '${fail_file}' '${ok_file}'" EXIT

run_capture() {
  local kind="$1" input="$2" output="$3" script="$4" rel="$5"
  if capture_one "${input}" "${output}" "${script}"; then
    flock "${ok_file}" bash -c "echo \$(( \$(cat '${ok_file}') + 1 )) > '${ok_file}'"
  else
    echo "FAILED: [${kind}] ${rel}" >&2
    flock "${fail_file}" bash -c "echo \$(( \$(cat '${fail_file}') + 1 )) > '${fail_file}'"
  fi
}

echo "Export root: ${export_root}"
echo "QtQuick png output: ${qtquick_output_root}"
echo "Slint png output: ${slint_output_root}"
echo "Flutter png output: ${flutter_output_root}"
echo "LVGL png output: ${lvgl_output_root}"
echo "Max parallel capture jobs: ${max_capture_jobs}"

export QT_QPA_PLATFORM=xcb
export QML_VIEWER_BIN="${QML_VIEWER_BIN:-qml}"

job_count=0
for capture_args in \
  "QtQuick|MainWindow.ui.qml|${qtquick_output_root}|${qml_capture_script}" \
  "Slint|MainWindow.slint|${slint_output_root}|${slint_capture_script}" \
  "Flutter|main_window.dart|${flutter_output_root}|${flutter_capture_script}" \
  "LVGL|MainScreen.xml|${lvgl_output_root}|${lvgl_capture_script}"; do
  IFS='|' read -r kind input_name output_root_dir script <<< "${capture_args}"

  while IFS= read -r input_path; do
    rel_no_ext="${input_path#"${export_root}/"}"
    rel_no_ext="${rel_no_ext%/${kind}/${input_name}}"
    output_path="${output_root_dir}/${rel_no_ext}.png"

    echo "[${kind}] ${rel_no_ext}"
    run_capture "${kind}" "${input_path}" "${output_path}" "${script}" "${rel_no_ext}" &
    job_count=$((job_count + 1))
    if [[ "${job_count}" -ge "${max_capture_jobs}" ]]; then
      wait -n 2>/dev/null || true
      job_count=$((job_count - 1))
    fi
  done < <(find "${export_root}" -type f -path "*/${kind}/${input_name}" | sort)
done
# Drain remaining capture jobs (don't use bare 'wait' — it blocks on Xvfb too)
while [[ "${job_count}" -gt 0 ]]; do
  wait -n 2>/dev/null || true
  job_count=$((job_count - 1))
done

total_ok="$(cat "${ok_file}")"
total_fail="$(cat "${fail_file}")"
echo "Captured ${total_ok} screenshots (${total_fail} failed)."

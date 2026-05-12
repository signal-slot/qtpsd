#!/usr/bin/env bash
# Build and launch a Qt for MCUs project on the Qt-on-Qt desktop simulator.
#
# Auto-discovers Qt for MCUs SDK (QUL_ROOT) and system Qt (QT_DIR); both can
# be overridden by exporting the matching environment variable before invoking.
#
# Usage:
#   scripts/qtmcu-preview.sh <path/to/MainWindow.qmlproject>
#
# Environment overrides:
#   QUL_ROOT   — Qt for MCUs SDK root (contains bin/qmlprojectexporter)
#   QT_DIR     — Desktop Qt 6 prefix (must contain lib{,64}/cmake/Qt6)
#   CMAKE      — cmake binary (default: from PATH)
#   NINJA      — ninja binary (default: from PATH)
#   QULPREVIEW — qulpreview binary (default: ${QUL_ROOT}/qulpreview/qulpreview)

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <path/to/MainWindow.qmlproject>" >&2
    exit 2
fi

project_file="$1"
if [[ ! -f "${project_file}" ]]; then
    echo "Project file not found: ${project_file}" >&2
    exit 1
fi

# Find highest-versioned QtMCUs install under each candidate base directory.
# Locale-independent: uses `find` + `LC_ALL=C sort -V` (no `ls`).
pick_qul_root() {
    local base="$1"
    [[ -d "${base}" ]] || return 0
    find "${base}" -mindepth 1 -maxdepth 1 -type d -print 2>/dev/null \
        | LC_ALL=C sort -V -r
}

if [[ -z "${QUL_ROOT:-}" ]]; then
    bases=()
    [[ -n "${HOME:-}" ]] && bases+=("${HOME}/Qt/QtMCUs" "${HOME}/io/qt/release/QtMCUs")
    bases+=("/opt/Qt/QtMCUs" "/opt/QtMCUs")
    for base in "${bases[@]}"; do
        while IFS= read -r candidate; do
            if [[ -x "${candidate}/bin/qmlprojectexporter" ]]; then
                QUL_ROOT="${candidate}"
                break 2
            fi
        done < <(pick_qul_root "${base}")
    done
fi

if [[ -z "${QUL_ROOT:-}" || ! -x "${QUL_ROOT}/bin/qmlprojectexporter" ]]; then
    echo "QUL_ROOT not found. Set QUL_ROOT to a Qt for MCUs install (must contain bin/qmlprojectexporter)." >&2
    exit 1
fi

if [[ -z "${QT_DIR:-}" ]]; then
    for qmake_bin in qmake6 qmake-qt6 qmake; do
        command -v "${qmake_bin}" >/dev/null 2>&1 || continue
        candidate="$("${qmake_bin}" -query QT_INSTALL_PREFIX 2>/dev/null || true)"
        if [[ -n "${candidate}" ]] && { [[ -d "${candidate}/lib/cmake/Qt6" ]] || [[ -d "${candidate}/lib64/cmake/Qt6" ]]; }; then
            QT_DIR="${candidate}"
            break
        fi
    done
fi

if [[ -z "${QT_DIR:-}" ]] || { [[ ! -d "${QT_DIR}/lib/cmake/Qt6" ]] && [[ ! -d "${QT_DIR}/lib64/cmake/Qt6" ]]; }; then
    echo "QT_DIR not found. Set QT_DIR to a desktop Qt 6 prefix (must contain lib{,64}/cmake/Qt6)." >&2
    exit 1
fi

CMAKE="${CMAKE:-$(command -v cmake || true)}"
NINJA="${NINJA:-$(command -v ninja || true)}"
QULPREVIEW="${QULPREVIEW:-${QUL_ROOT}/qulpreview/qulpreview}"

for tool in "${CMAKE}" "${NINJA}" "${QULPREVIEW}"; do
    if [[ -z "${tool}" || ! -x "${tool}" ]]; then
        echo "Required tool not found or not executable: ${tool}" >&2
        exit 1
    fi
done

profile_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/qtmcu"
if [[ ! -f "${profile_dir}/linux-gcc.json" ]]; then
    echo "qulpreview profile missing: ${profile_dir}/linux-gcc.json" >&2
    exit 1
fi

echo "QUL_ROOT=${QUL_ROOT}"
echo "QT_DIR=${QT_DIR}"
echo "CMAKE=${CMAKE}"
echo "NINJA=${NINJA}"

export QUL_ROOT QT_DIR CMAKE NINJA

# qulpreview's deploy recipe runs qmlprojectexporter → cmake configure → cmake
# build → launch the binary detached. The detached launch step makes
# qulpreview emit `[ERROR] ... process failed to start` on success too
# (because it doesn't see the exit code of the detached child). Treat that
# specific case as success while preserving any real build failure.
tmp_log="$(mktemp)"
trap 'rm -f "${tmp_log}"' EXIT

set +e
"${QULPREVIEW}" -d "${profile_dir}" -p linux-gcc -w deploy "${project_file}" 2>&1 | tee "${tmp_log}"
rc=${PIPESTATUS[0]}
set -e

if [[ "${rc}" -eq 0 ]]; then
    exit 0
fi

# Compute the project's expected binary path. qulpreview names it after the
# project basename (sans .qmlproject) and places it under OUT_DIR (= project
# dir's `preview/`). If that binary exists and is runnable, deploy did succeed.
project_dir="$(dirname "${project_file}")"
project_base="$(basename "${project_file}" .qmlproject)"
out_bin="${project_dir}/preview/${project_base}"
if [[ -x "${out_bin}" ]] && grep -q "process failed to start" "${tmp_log}"; then
    exit 0
fi

exit "${rc}"

#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${FREESSM_QT_BUILD_DIR:?Set FREESSM_QT_BUILD_DIR to the completed build directory}
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/freessm-policy.XXXXXXXX")
trap 'rm -rf -- "$task_dir"' EXIT HUP INT TERM
"${CXX:-g++}" -std=c++11 -Wall -Wextra -Werror -I"$root/src" "$root/tests/test_diagnostic_safety.cpp" -o "$task_dir/test"
"$task_dir/test"
app="$build_dir/FreeSSM"
[ -x "$app" ] || app="$build_dir/release/FreeSSM"
QT_QPA_PLATFORM=offscreen "$app" --help > "$task_dir/help" 2>&1
grep -q -- '--enable-write' "$task_dir/help"
grep -q -- '--demo' "$task_dir/help"
if QT_QPA_PLATFORM=offscreen "$app" --read-only --enable-write > "$task_dir/conflict" 2>&1; then
    echo 'Se aceptaron opciones de seguridad incompatibles' >&2
    exit 1
fi
grep -q -- 'cannot be combined' "$task_dir/conflict"
if QT_QPA_PLATFORM=offscreen "$app" --demo --j2534-probe > "$task_dir/probe-conflict" 2>&1; then
    echo 'Se aceptaron demo y prueba J2534 al mismo tiempo' >&2
    exit 1
fi
grep -q -- 'must be used alone' "$task_dir/probe-conflict"
echo 'Política inicial conservadora y opciones incompatibles rechazadas OK'

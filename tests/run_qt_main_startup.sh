#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${FREESSM_QT_BUILD_DIR:?Set FREESSM_QT_BUILD_DIR to the completed build directory}
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/freessm-main-startup.XXXXXXXX")
trap 'rm -rf -- "$task_dir"' EXIT HUP INT TERM
"${CXX:-g++}" -std=c++11 -fPIC -Dmain=freessmMain -I"$root/src" -I"$build_dir" \
    $(pkg-config --cflags Qt5Widgets Qt5PrintSupport) -c "$root/src/main.cpp" -o "$task_dir/main.o"
for asset in "$build_dir"/FreeSSM_*.qm "$build_dir"/background.png; do
    [ ! -f "$asset" ] || cp "$asset" "$task_dir/"
done
set --
for object in "$build_dir"/release/*.o; do
    [ "$(basename "$object")" = main.o ] || set -- "$@" "$object"
done
"${CXX:-g++}" -std=c++11 -fPIC -I"$root/src" -I"$build_dir" \
    $(pkg-config --cflags Qt5Widgets Qt5PrintSupport) \
    "$root/tests/test_qt_main_startup.cpp" "$task_dir/main.o" "$@" \
    $(pkg-config --libs Qt5Widgets Qt5PrintSupport) -ldl -lrt -lpthread -o "$task_dir/test"
for path in cancel demo full gui-engine gui-transmission; do
    FREESSM_STARTUP_TEST=$path QT_QPA_PLATFORM=offscreen timeout 20s "$task_dir/test"
done
FREESSM_STARTUP_TEST=cli-full QT_QPA_PLATFORM=offscreen timeout 20s "$task_dir/test" --enable-write
FREESSM_STARTUP_TEST=cli-demo QT_QPA_PLATFORM=offscreen timeout 20s "$task_dir/test" --demo
echo 'Arranque real: selector, cancelación, demo y confirmación OK'

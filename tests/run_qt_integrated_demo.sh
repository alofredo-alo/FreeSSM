#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${FREESSM_QT_BUILD_DIR:?Set FREESSM_QT_BUILD_DIR to the completed build directory}
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/freessm-integrated-demo.XXXXXXXX")
trap 'rm -rf -- "$task_dir"' EXIT HUP INT TERM
set --
for object in "$build_dir"/release/*.o; do
    [ "$(basename "$object")" = main.o ] || set -- "$@" "$object"
done
"${CXX:-g++}" -std=c++11 -fPIC -I"$root/src" -I"$build_dir" \
    $(pkg-config --cflags Qt5Widgets Qt5PrintSupport) \
    "$root/tests/test_qt_integrated_demo.cpp" "$@" \
    $(pkg-config --libs Qt5Widgets Qt5PrintSupport) -ldl -lrt -lpthread -o "$task_dir/test"
QT_QPA_PLATFORM=offscreen timeout 45s "$task_dir/test"

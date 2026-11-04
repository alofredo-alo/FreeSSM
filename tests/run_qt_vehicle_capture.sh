#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${FREESSM_QT_BUILD_DIR:-}
if [ -z "$build_dir" ]; then
    echo 'Set FREESSM_QT_BUILD_DIR to a completed qmake build directory.' >&2
    exit 1
fi
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/freessm-qt-capture.XXXXXXXX")
trap 'rm -rf -- "$task_dir"' EXIT HUP INT TERM
set --
for object in "$build_dir"/release/*.o; do
    [ "$(basename "$object")" = main.o ] || set -- "$@" "$object"
done
"${CXX:-g++}" -std=c++11 -fPIC -I"$root/src" -I"$build_dir" \
    $(pkg-config --cflags Qt5Widgets Qt5PrintSupport) \
    "$root/tests/test_qt_vehicle_capture.cpp" "$@" \
    $(pkg-config --libs Qt5Widgets Qt5PrintSupport) -ldl -lrt -lpthread -o "$task_dir/test"
QT_QPA_PLATFORM=offscreen "$task_dir/test"

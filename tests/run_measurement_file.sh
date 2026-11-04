#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/freessm-measurement.XXXXXXXX")
trap 'rm -rf -- "$task_dir"' EXIT HUP INT TERM
"${CXX:-g++}" -std=c++11 -fPIC -Wall -Wextra -Werror -I"$root/src" \
    $(pkg-config --cflags Qt5Core) "$root/tests/test_measurement_file.cpp" \
    "$root/src/MeasurementFile.cpp" $(pkg-config --libs Qt5Core) -o "$task_dir/test"
"$task_dir/test"

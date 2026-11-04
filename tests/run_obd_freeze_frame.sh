#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/freessm-obd-frame.XXXXXXXX")
trap 'rm -rf -- "$task_dir"' EXIT HUP INT TERM
"${CXX:-g++}" -std=c++11 -Wall -Wextra -Werror -I"$root/src" \
    "$root/tests/test_obd_freeze_frame.cpp" "$root/src/OBDFreezeFrame.cpp" -o "$task_dir/test"
"$task_dir/test"

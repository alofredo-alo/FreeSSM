#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/freessm-ssm2-test.XXXXXXXX")
trap 'rm -rf -- "$build_dir"' EXIT HUP INT TERM
"${CXX:-g++}" -std=c++11 -Wall -Wextra -Werror -I"$root/tests/support" -I"$root/src" \
    "$root/tests/test_ssm2_reply.cpp" "$root/src/SSMP2communication_core.cpp" \
    "$root/src/AbstractDiagInterface.cpp" "$root/src/linux/TimeM.cpp" -o "$build_dir/test_ssm2_reply"
"$build_dir/test_ssm2_reply"

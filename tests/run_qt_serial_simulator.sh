#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${FREESSM_QT_BUILD_DIR:-}
if [ -z "$build_dir" ]; then
    echo 'Set FREESSM_QT_BUILD_DIR to a completed qmake build directory.' >&2
    exit 1
fi
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/freessm-qt-serial.XXXXXXXX")
trap 'rm -rf -- "$task_dir"' EXIT HUP INT TERM
set --
for object in "$build_dir"/release/*.o; do
    [ "$(basename "$object")" = main.o ] || set -- "$@" "$object"
done
"${CXX:-g++}" -std=c++11 -fPIC -I"$root/src" -I"$build_dir" \
    $(pkg-config --cflags Qt5Widgets Qt5PrintSupport) \
    "$root/tests/test_qt_serial_simulator.cpp" "$@" \
    $(pkg-config --libs Qt5Widgets Qt5PrintSupport) -ldl -lrt -lpthread -o "$task_dir/test"
QT_QPA_PLATFORM=offscreen python3 - "$root/tools/fake_ecu.py" "$task_dir/test" <<'PY'
import select
import subprocess
import sys
for option in ([], ['--no-echo']):
    simulator = subprocess.Popen([sys.executable, '-u', sys.argv[1], '--fast', '--dtc', '--vin', *option],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        ready, _, _ = select.select([simulator.stdout], [], [], 5)
        if not ready:
            raise RuntimeError('simulator did not start')
        path = simulator.stdout.readline().strip().rsplit(' ', 1)[-1]
        print('Probando serial simulado:', 'sin eco' if option else 'con eco', flush=True)
        subprocess.run([sys.argv[2], path], check=True, timeout=15)
    finally:
        simulator.terminate()
        simulator.communicate(timeout=5)
PY

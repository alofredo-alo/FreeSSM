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
    "$root/tests/test_qt_report_scan.cpp" "$@" \
    $(pkg-config --libs Qt5Widgets Qt5PrintSupport) -ldl -lrt -lpthread -o "$task_dir/test"
QT_QPA_PLATFORM=offscreen python3 - "$root/tools/fake_ecu.py" "$task_dir/test" <<'PY'
import select
import subprocess
import sys
for option, transmission in (([], True), (['--no-echo'], True), ([], False)):
    simulator = subprocess.Popen([sys.executable, '-u', sys.argv[1], '--fast', '--dtc', '--vin', '--verbose', *(['--transmission'] if transmission else []), *option],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        ready, _, _ = select.select([simulator.stdout], [], [], 5)
        if not ready:
            raise RuntimeError('simulator did not start')
        path = simulator.stdout.readline().strip().rsplit(' ', 1)[-1]
        print('Probando serial simulado:', ('sin eco' if option else 'con eco'), 'con transmisión' if transmission else 'sin transmisión', flush=True)
        subprocess.run([sys.argv[2], path, *([] if transmission else ["--missing-transmission"])], check=True, timeout=60)
    finally:
        simulator.terminate()
        stdout, stderr = simulator.communicate(timeout=5)
        frames = [line[4:] for line in stdout.splitlines() if line.startswith('rx: ')]
        if not frames:
            raise RuntimeError('no simulated request frames captured')
        requests = [bytes.fromhex(frame) for frame in frames]
        addresses = {frame[1] for frame in requests}
        if not addresses.issubset({0x10, 0x18}):
            raise RuntimeError('unexpected fallback address: ' + repr(addresses))
        services = {frame[4] for frame in requests}
        if not services.issubset({0xbf, 0xa0, 0xa8}):
            raise RuntimeError('unexpected service in read-only scan: ' + repr(services))
        print('Solo servicios de identificación y lectura:', ', '.join(hex(x) for x in sorted(services)))
PY

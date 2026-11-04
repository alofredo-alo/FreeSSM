#!/bin/sh
# Linux smoke test only. The stub is not the macOS SDK and does not exercise IOSSIOSPEED.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
[ "$(uname -s)" = Linux ] || { echo 'Esta prueba de compatibilidad requiere Linux.' >&2; exit 1; }
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/freessm-macserial.XXXXXXXX")
trap 'rm -rf -- "$task_dir"' EXIT HUP INT TERM
mkdir -p "$task_dir/IOKit/serial"
printf '%s\n' '#define IOSSIOSPEED 0x80045402UL' > "$task_dir/IOKit/serial/ioss.h"
"${CXX:-g++}" -std=c++11 -pthread -I"$task_dir" -I"$root/src/mac" \
    "$root/tests/test_mac_serial_posix.cpp" "$root/src/mac/serialCOM.cpp" -o "$task_dir/test"
python3 - "$root/tools/fake_ecu.py" "$task_dir/test" <<'PY'
import select
import subprocess
import sys
for option in ([], ['--no-echo']):
    sim = subprocess.Popen([sys.executable, '-u', sys.argv[1], '--fast', *option],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        ready, _, _ = select.select([sim.stdout], [], [], 5)
        if not ready:
            raise RuntimeError('simulator did not start')
        path = sim.stdout.readline().strip().rsplit(' ', 1)[-1]
        subprocess.run([sys.argv[2], path, 'noecho' if option else 'echo'], check=True, timeout=10)
    finally:
        sim.terminate()
        sim.communicate(timeout=5)
PY

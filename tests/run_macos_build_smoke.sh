#!/bin/sh
# Native compile and bundle checks only. Never opens a diagnostic device.
set -eu
if [ "$(uname -s)" != Darwin ]; then
    echo 'Esta prueba requiere macOS y el SDK de Apple.' >&2
    exit 1
fi
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
command -v qmake >/dev/null
command -v lrelease >/dev/null
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/freessm-macos-build.XXXXXXXX")
trap 'rm -rf -- "$task_dir"' EXIT HUP INT TERM
cd "$task_dir"
for language in en de tr es; do
    lrelease "$root/FreeSSM_${language}.ts" -qm "FreeSSM_${language}.qm"
done
qmake "$root/FreeSSM.pro" 'CONFIG+=release'
make -j2
bundle="$task_dir/FreeSSM.app/Contents/MacOS"
test -x "$bundle/FreeSSM"
test -s "$bundle/background.png"
test -s "$bundle/FreeSSM_es.qm"
test -s "$bundle/definitions/5eat_tcu_logger.xml"
test -s "$bundle/definitions/J2534libs.xml"
printf '%s\n' 'Compilación macOS y recursos del bundle: OK (sin hardware).'

#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/freessm-report-test.XXXXXXXX")
trap 'rm -rf -- "$task_dir"' EXIT HUP INT TERM
"${CXX:-g++}" -std=c++11 -Wall -Wextra -Werror -I"$root/src" \
    "$root/tests/test_vehicle_report.cpp" "$root/src/VehicleReport.cpp" "$root/src/VehicleReportCollector.cpp" \
    -o "$task_dir/test_vehicle_report"
"$task_dir/test_vehicle_report" > "$task_dir/report.json"
python3 - "$task_dir/report.json" <<'PY'
import json
import sys
with open(sys.argv[1], encoding='utf-8') as stream:
    report = json.load(stream)
assert report['formato_version'] == '1.0'
assert report['simulado'] is False
assert report['version_aplicacion'] == '1.3.0-beta.1-candidate'
assert report['vehiculo']['modelo_declarado'] == 'Outback "prueba"\nñ'
assert report['vehiculo']['odometro_tablero_km'] == 185430
assert report['vehiculo']['vin']['estado'] == 'no_consultada'
assert report['vehiculo']['vin']['valor'] is None
modules = {item['nombre']: item for item in report['modulos']}
assert modules['motor']['estado_consulta'] == 'completa'
assert modules['motor']['dtc'] == []
assert modules['motor']['definiciones_dtc_no_verificadas'] is True
assert modules['transmision']['definiciones_dtc_no_verificadas'] is False
assert [g['estado'] for g in modules['motor']['cobertura_dtc']] == ['completa', 'completa']
assert modules['motor']['rom_id']['valor'] == 'B1AB217100'
code = modules['transmision']['dtc'][0]
assert code['categoria'] == 'actual_o_temporal'
assert code['descripcion'] == 'Descripción\t\\\x01'
for name, status in [('abs', 'fallida'), ('airbag', 'no_soportada'), ('tablero', 'no_consultada')]:
    assert modules[name]['estado_consulta'] == status
    assert modules[name]['dtc'] is None
print('Informe JSON: datos sintéticos y validaciones OK')
PY

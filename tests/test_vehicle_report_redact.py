#!/usr/bin/env python3
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]

class RedactionTest(unittest.TestCase):
    def test_vin_and_optional_km_no_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source, target = root / 'in.json', root / 'out.json'
            vin = 'JF1ABCDEFG1234567'
            source.write_text(json.dumps({'formato_version': '1.0', 'vehiculo': {
                'vin': {'estado': 'completa', 'valor': vin}, 'odometro_tablero_km': 12345},
                'modulos': [{'detalle': 'VIN ' + vin}]}), encoding='utf-8')
            cmd = [sys.executable, str(ROOT / 'tools/vehicle_report_redact.py'), '--ocultar-km', str(source), str(target)]
            first = subprocess.run(cmd, capture_output=True, text=True)
            self.assertEqual(first.returncode, 0, first.stderr)
            content = target.read_text(encoding='utf-8')
            self.assertNotIn(vin, content)
            result = json.loads(content)
            self.assertIsNone(result['vehiculo']['odometro_tablero_km'])
            self.assertEqual(result['vehiculo']['vin']['estado'], 'completa')
            self.assertEqual(result['vehiculo']['vin']['valor'], '[VIN OCULTO]')
            self.assertTrue(result['datos_ocultados']['vin'])
            self.assertNotEqual(subprocess.run(cmd, capture_output=True).returncode, 0)
            self.assertEqual(target.read_text(encoding='utf-8'), content)

if __name__ == '__main__':
    unittest.main()

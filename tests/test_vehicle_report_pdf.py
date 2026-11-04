#!/usr/bin/env python3
"""PDF rendering checks using only fictional vehicle data."""
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'tools' / 'vehicle_report_pdf.py'


class PdfTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='freessm-pdf-')
        self.addCleanup(self.temp.cleanup)
        self.path = pathlib.Path(self.temp.name)
        self.source = self.path / 'report.json'
        self.output = self.path / 'report.pdf'
        self.report = {
            'formato_version': '1.0', 'simulado': True, 'fecha_captura': '2026-11-04T12:30:00-03:00',
            'vehiculo': {'modelo_declarado': '<Outback>', 'anio_declarado': 2010,
                         'odometro_tablero_km': 185430,
                         'vin': {'estado': 'no_consultada', 'valor': None}},
            'modulos': [
                {'nombre': 'motor', 'estado_consulta': 'completa', 'fuente': 'SSM',
                 'rom_id': {'estado': 'completa', 'valor': 'ROM-PRUEBA'},
                 'cobertura_dtc': [{'categoria': 'actual_o_temporal', 'estado': 'completa'}],
                 'definiciones_dtc_no_verificadas': True, 'dtc': []},
                {'nombre': 'airbag', 'estado_consulta': 'no_consultada', 'fuente': '',
                 'rom_id': {'estado': 'no_consultada', 'valor': None}, 'dtc': None},
            ]}

    def invoke(self):
        self.source.write_text(json.dumps(self.report, ensure_ascii=False), encoding='utf-8')
        return subprocess.run([sys.executable, str(SCRIPT), str(self.source), str(self.output)],
                              capture_output=True, text=True)

    def test_valid_pdf_and_no_overwrite(self):
        result = self.invoke()
        self.assertEqual(result.returncode, 0, result.stderr)
        data = self.output.read_bytes()
        self.assertTrue(data.startswith(b'%PDF-'))
        text = subprocess.run(['pdftotext', str(self.output), '-'], check=True, capture_output=True, text=True).stdout
        self.assertIn('<Outback>', text)
        self.assertIn('DATOS SIMULADOS', text)
        self.assertIn('Sin códigos reportados', text)
        self.assertIn('definiciones DTC experimentales no verificadas', text)
        self.assertIn('No existe un resultado completo', text)
        second = self.invoke()
        self.assertNotEqual(second.returncode, 0)
        self.assertEqual(self.output.read_bytes(), data)

    def test_inconsistent_result_is_rejected(self):
        self.report['modulos'][1]['dtc'] = []
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.output.exists())


if __name__ == '__main__':
    unittest.main()

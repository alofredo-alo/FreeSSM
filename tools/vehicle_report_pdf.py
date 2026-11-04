#!/usr/bin/env python3
"""Render a FreeSSM 1.0 vehicle report JSON as a PDF (requires ReportLab)."""
import argparse
import json
import io
import sys
from pathlib import Path
from xml.sax.saxutils import escape

try:
    from reportlab.lib.styles import getSampleStyleSheet
    from reportlab.lib.units import mm
    from reportlab.platypus import Paragraph, SimpleDocTemplate, Spacer
except ImportError:
    sys.exit('Falta la dependencia Python reportlab; instálela antes de generar el PDF.')

STATUSES = {'completa': 'Completada', 'fallida': 'Fallida',
            'no_soportada': 'No soportada', 'no_consultada': 'No consultada'}
CATEGORIES = {'actual': 'Actual', 'temporal': 'Temporal', 'historico': 'Histórico',
              'memorizado': 'Memorizado', 'actual_o_temporal': 'Actual o temporal',
              'historico_o_memorizado': 'Histórico o memorizado', 'desconocida': 'Desconocida'}


def validate(report):
    if not isinstance(report, dict) or report.get('formato_version') != '1.0':
        raise ValueError('Formato JSON no compatible')
    if not isinstance(report.get('simulado', False), bool):
        raise ValueError('Indicador de simulación inválido')
    if not isinstance(report.get('vehiculo'), dict) or not isinstance(report.get('modulos'), list):
        raise ValueError('Faltan vehículo o módulos')
    for module in report['modulos']:
        if not isinstance(module, dict) or module.get('estado_consulta') not in STATUSES:
            raise ValueError('Estado de módulo inválido')
        if not isinstance(module.get('definiciones_dtc_no_verificadas', False), bool):
            raise ValueError('Indicador de definiciones DTC inválido')
        dtc = module.get('dtc')
        if module['estado_consulta'] == 'completa':
            if not isinstance(dtc, list):
                raise ValueError('Una consulta completa debe incluir una lista de DTC')
            for code in dtc:
                if not isinstance(code, dict) or not code.get('codigo') or code.get('categoria') not in CATEGORIES:
                    raise ValueError('Código o categoría inválidos')
        elif dtc is not None:
            raise ValueError('Una consulta incompleta debe tener DTC null')
        for group in module.get('cobertura_dtc', []):
            if group.get('estado') not in STATUSES or group.get('categoria') not in CATEGORIES:
                raise ValueError('Cobertura DTC inválida')
            if module['estado_consulta'] == 'completa' and group['estado'] not in ('completa', 'no_soportada'):
                raise ValueError('Una consulta completa contiene un grupo incompleto')
    return report


def text(value):
    if value is None or value == '':
        return 'No disponible'
    return escape(str(value)).replace('\n', '<br/>')


def reading(value):
    if not isinstance(value, dict):
        return 'No disponible'
    status = value.get('estado')
    if status == 'completa':
        return text(value.get('valor'))
    return text(STATUSES.get(status, 'No disponible'))


def render(report, output):
    styles = getSampleStyleSheet()
    doc = SimpleDocTemplate(output, pagesize=(210 * mm, 297 * mm),
                            leftMargin=18 * mm, rightMargin=18 * mm,
                            topMargin=16 * mm, bottomMargin=16 * mm,
                            title='Informe de estado del vehículo')
    story = []
    def p(value, style='BodyText'):
        story.append(Paragraph(value, styles[style]))
    def row(label, value):
        p('<b>' + text(label) + ':</b> ' + value)
    p('Informe de estado del vehículo', 'Title')
    if report.get('simulado', False):
        p('DEMO — DATOS SIMULADOS', 'Heading1')
        p('Este informe no corresponde a un vehículo real.')
    p('Resultados de las consultas y módulos indicados en este documento.')
    story.append(Spacer(1, 4 * mm))
    vehicle = report['vehiculo']
    row('Fecha', text(report.get('fecha_captura')))
    if report.get('version_aplicacion'):
        row('Versión de FreeSSM', text(report['version_aplicacion']))
    row('Modelo declarado', text(vehicle.get('modelo_declarado')))
    row('Año declarado', text(vehicle.get('anio_declarado')))
    row('Kilometraje del tablero declarado', text(vehicle.get('odometro_tablero_km')) + ' km' if vehicle.get('odometro_tablero_km') is not None else 'No informado')
    row('VIN de la lectura', reading(vehicle.get('vin')))
    p('Módulos y códigos', 'Heading2')
    if not report['modulos']:
        p('No se incluyeron módulos en el informe.')
    for module in report['modulos']:
        p(text(module.get('nombre')), 'Heading3')
        row('Consulta', text(STATUSES[module['estado_consulta']]))
        row('Fuente', text(module.get('fuente')))
        row('ROM-ID', reading(module.get('rom_id')))
        if module.get('definiciones_dtc_no_verificadas', False):
            p('<b>Advertencia: definiciones DTC experimentales no verificadas; el resultado puede ser incompleto o incorrecto.</b>')
        if module.get('detalle'):
            row('Detalle', text(module['detalle']))
        for group in module.get('cobertura_dtc', []):
            row('Cobertura ' + CATEGORIES[group['categoria']], text(STATUSES[group['estado']]))
        if module['dtc'] is None:
            p('No existe un resultado completo de códigos para este módulo.')
        elif not module['dtc']:
            p('Sin códigos reportados en los grupos consultados.')
        else:
            for code in module['dtc']:
                p('<b>' + text(code['codigo']) + '</b> — ' + text(CATEGORIES[code['categoria']]) + ': ' + text(code.get('descripcion')))
    story.append(Spacer(1, 5 * mm))
    p('El kilometraje declarado no está contrastado electrónicamente. Los módulos no consultados no forman parte del diagnóstico.')
    p('Los códigos almacenados disponibles no constituyen un historial completo del vehículo.')
    doc.build(story)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('entrada', type=Path, help='Informe JSON 1.0')
    parser.add_argument('salida', type=Path, help='Destino PDF (no sobrescribe archivos)')
    args = parser.parse_args()
    try:
        with args.entrada.open(encoding='utf-8') as stream:
            report = validate(json.load(stream))
        # Render before creating the destination to avoid incomplete files on error.
        buffer = io.BytesIO()
        render(report, buffer)
        # Exclusive creation prevents accidentally replacing an existing report.
        with args.salida.open('xb') as output:
            output.write(buffer.getvalue())
    except (OSError, ValueError, TypeError, KeyError, json.JSONDecodeError) as error:
        print('No se pudo generar el informe:', error, file=sys.stderr)
        return 1
    print('Informe generado:', args.salida)
    return 0


if __name__ == '__main__':
    sys.exit(main())

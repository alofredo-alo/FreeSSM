#!/usr/bin/env python3
"""Remove VIN-like identifiers from a FreeSSM JSON report before sharing."""
import argparse
import json
import re
import sys
from pathlib import Path

# VINs exclude I/O/Q. Boundaries avoid replacing substrings of larger identifiers.
VIN = re.compile(r'(?<![A-Za-z0-9])[A-HJ-NPR-Z0-9]{17}(?![A-Za-z0-9])', re.IGNORECASE)


def redact(value):
    if isinstance(value, dict):
        return {key: redact(item) for key, item in value.items()}
    if isinstance(value, list):
        return [redact(item) for item in value]
    if isinstance(value, str):
        return VIN.sub('[VIN OCULTO]', value)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('entrada', type=Path)
    parser.add_argument('salida', type=Path, help='Archivo nuevo; no sobrescribe')
    parser.add_argument('--ocultar-km', action='store_true', help='Quitar el kilometraje declarado')
    args = parser.parse_args()
    try:
        with args.entrada.open(encoding='utf-8') as stream:
            original = json.load(stream)
        if not isinstance(original, dict) or original.get('formato_version') != '1.0' or not isinstance(original.get('vehiculo'), dict):
            raise ValueError('Formato de informe no compatible')
        result = redact(original)
        vin = result['vehiculo'].get('vin')
        if isinstance(vin, dict) and vin.get('valor') is not None:
            vin['valor'] = '[VIN OCULTO]'
        if args.ocultar_km:
            result['vehiculo']['odometro_tablero_km'] = None
        result['datos_ocultados'] = {'vin': True, 'kilometraje_tablero': args.ocultar_km}
        payload = json.dumps(result, ensure_ascii=False, indent=2) + '\n'
        with args.salida.open('x', encoding='utf-8') as stream:
            stream.write(payload)
    except (OSError, ValueError, TypeError) as error:
        print('No se pudo generar la copia:', error, file=sys.stderr)
        return 1
    print('Copia creada:', args.salida)
    return 0


if __name__ == '__main__':
    sys.exit(main())

# Definición de logger TCU 5EAT

El archivo 5eat_tcu_logger.xml proviene de
[TomFLV/5eat-tcu-reverse-engineering](https://github.com/TomFLV/5eat-tcu-reverse-engineering)
y está fijado al commit:

    44b5e8b8fc407883b0cce0123509f93eaf06a59b

Integridad:

    5eat_tcu_logger.xml
    SHA-256 511e47967ca0461a84b4e9ce072906fb6577a76af4f2508f93babfffb224e447

El atributo Git específico del archivo fuerza finales de línea LF también en
Windows para que el XML distribuido conserve ese hash byte a byte.

La definición está bajo licencia MIT. El texto exacto y su nota de alcance están
en 5eat_tcu_logger.LICENSE.txt.

## Alcance de esta integración

FreeSSM aplica esta definición únicamente a Transmission y sólo cuando los cinco
bytes del ROM/unit ID de la TCU coinciden exactamente con uno de los 26 IDs
declarados por el XML.

El importador acepta:

- parámetros ecuparam con target 2;
- direcciones Select Monitor menores que 0x200;
- valores de uno o dos bytes;
- conversiones aritméticas lineales basadas en x;
- los 12 pares explícitos de bytes DTC actuales y confirmados.

Se excluyen intencionalmente los switches y las lecturas RAM directas de las
direcciones 0x804EB2 y 0x804EB6. No se importan ajustes, actuadores, borrado de
memoria ni ninguna escritura.

Si el ID no coincide, el XML falta o falla la validación, FreeSSM conserva las
definiciones internas anteriores y no intenta aproximar direcciones.

## Actualización

Para actualizar el archivo:

1. revisar el diff upstream y su licencia;
2. reemplazar el XML sin modificarlo;
3. actualizar aquí el commit y SHA-256;
4. actualizar el hash esperado en .github/workflows/windows-j2534.yml;
5. ejecutar tests/test_fiveeat_logger.cpp contra los 26 IDs;
6. validar el artefacto en Windows antes de usar nuevas direcciones en un auto.

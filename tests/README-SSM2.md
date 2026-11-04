# Pruebas SSM2 sin vehículo

En Linux, desde la raíz del repositorio:

```bash
./tests/run_ssm2_reply.sh
python3 -B tests/test_fake_ecu.py
```

La primera prueba compila el núcleo de comunicación real con una interfaz
simulada y verifica respuestas con eco, sin eco, incompletas, fallo de escritura
y checksum incorrecto. Requiere `g++`, no requiere Qt. El archivo
`tests/support/QString` y las funciones mínimas de `libFSSM` de la prueba son
sustitutos de compilación: la prueba no valida Qt ni la implementación completa
de `libFSSM`. Genera el ejecutable en un directorio temporal y lo elimina al
terminar. La prueba de respuesta incompleta espera el timeout real de SSM2.

La segunda prueba ejecuta el simulador y conversa con su pseudoterminal; requiere
Python 3 y un sistema con PTY. No usa dispositivos reales. El simulador procede
de Jacob Seman, commit `7c9bfca59` del fork Jbsco/FreeSSM, GPL-3.0 o posterior;
se ha quitado la creación de enlaces en `/dev` y la invocación de `sudo`.

También puede iniciarse manualmente:

```bash
python3 -u tools/fake_ecu.py --fast
python3 -u tools/fake_ecu.py --fast --no-echo
python3 -u tools/fake_ecu.py --fast --dtc --vin
```

Imprime una ruta como `/dev/pts/3`. El selector gráfico actual de FreeSSM en
Linux solo enumera determinados nombres en `/dev`, por lo que esa ruta todavía
no es seleccionable en la interfaz. El simulador puede recibir comandos de
escritura sobre su memoria sintética; no lo conectes a un vehículo ni interpretes
sus resultados como diagnósticos reales.

Las pruebas de este directorio no reemplazan una compilación completa de Qt,
pruebas con OpenPort/J2534 ni validación en un vehículo real.

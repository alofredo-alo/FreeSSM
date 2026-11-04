# Modelo JSON de informe de vehículo

`src/VehicleReport.h` y `src/VehicleReport.cpp` proporcionan un modelo C++11
y un serializador JSON sin dependencias adicionales. La pantalla de DTC puede exportar JSON y PDF de la captura del módulo abierto.
Los datos usados por las pruebas son ficticios.

## Ejecutar las pruebas

Desde la raíz del repositorio en Linux:

```bash
./tests/run_vehicle_report.sh
```

Requiere `g++` y Python 3. Compila el modelo real en un directorio temporal,
genera un JSON sintético y lo parsea con Python. Comprueba la distinción entre
DTC vacíos y consultas no completas, categorías ambiguas, VIN no consultado,
caracteres especiales y rechazo de datos contradictorios. No requiere Qt ni
valida la aplicación completa.

## Contrato del formato 1.0

- `fecha_captura`: texto ISO 8601 con zona horaria o `null`; el productor debe
  suministrar y validar fecha y zona.
- `vehiculo`: modelo y año declarados, kilometraje del tablero ingresado por el
  usuario y VIN. Lo declarado no debe presentarse como lectura electrónica.
- `vin` y `rom_id`: objeto con `estado`, `valor`, `fuente` y `detalle`. El
  valor es `null` si el estado no es `completa`. Un texto vacío con estado
  `completa` representa una respuesta vacía, no un valor desconocido.
- `modulos`: cada elemento contiene nombre, estado de consulta, fuente, detalle,
  ROM-ID, cobertura de grupos DTC y DTC. `dtc: []` indica una consulta completa sin códigos; `dtc: null`
  indica que no existe un resultado completo. No se permite serializar códigos
  para un módulo cuyo estado no sea `completa`.
- El productor debe marcar `completa` solo cuando haya consultado exitosamente
  todos los grupos de DTC que declare cubrir. La cobertura indica qué grupos respondieron o fallaron; si solo se obtiene
  una parte, el módulo no se clasifica como completo y la lista DTC es `null`.
- Las categorías `actual_o_temporal` e `historico_o_memorizado` conservan la
  ambigüedad de las señales originales. No inferir categorías más precisas.
- Un valor manual ausente usa `null`: el modelo utiliza internamente año `0` y
  kilometraje `-1` para indicarlo. Los demás valores negativos se rechazan.
- Las cadenas deben entregarse como UTF-8 válido. El serializador escapa
  comillas, barras invertidas y caracteres de control JSON; no valida UTF-8 ni
  verifica que el VIN, año o fecha sean reales.

El JSON puede contener VIN y kilometraje: guárdalo y compártelo como dato
sensible del vehículo. La anonimización y la selección de destino se implementarán
junto con el flujo real de captura.

## Exportar desde la aplicación

Abrir motor o transmisión e iniciar la lectura de códigos. El botón **Guardar
informe de estado (JSON)** captura el módulo abierto y pide opcionalmente el
kilometraje del tablero y permite guardar JSON y PDF; los demás módulos se marcan
no consultados. Si FreeSSM ya obtuvo un VIN válido al abrir motor, se reutiliza;
en caso contrario se marca como no consultado o no soportado. No se vuelve a
consultar el VIN al exportar. Cancelar el diálogo PDF conserva el JSON ya guardado. La
lectura debe recibir todos los grupos compatibles antes de considerarse
completa; una comprobación del sistema incompleta o un error de comunicación
no se presenta como «sin códigos».

La exportación desde una pantalla de DTC captura solo ese módulo y no obtiene
kilometraje electrónico. Para consultar ambos, usa el botón de la ventana principal. La fecha del informe es la fecha de exportación; las
lecturas provienen de la sesión de códigos activa. Aún no se ha validado el
flujo contra un vehículo real.

## PDF adicional desde JSON

Con `reportlab` disponible en Python:

```bash
python3 tools/vehicle_report_pdf.py informe.json informe.pdf
```

El destino PDF se crea exclusivamente y no sobrescribe archivos existentes.
Se preservan el estado de cada módulo y la cobertura de los grupos DTC. La
herramienta no evalúa integridad o procedencia del archivo JSON de entrada.
Para probar con datos sintéticos se requiere además `pdftotext`:

```bash
python3 -B tests/test_vehicle_report_pdf.py
```

Las señales Qt y el comportamiento de cobertura se prueban con un protocolo
simulado, tras compilar el proyecto con qmake:

```bash
FREESSM_QT_BUILD_DIR=/ruta/al/directorio/de/compilacion ./tests/run_qt_vehicle_capture.sh
```

## Prueba integrada sin vehículo

Con el proyecto compilado en Linux:

```bash
FREESSM_QT_BUILD_DIR=/ruta/al/directorio/de/compilacion ./tests/run_qt_serial_simulator.sh
```

La prueba crea un pseudoterminal, activa el modo de solo lectura y ejercita el
transporte serial y protocolo SSM2 reales, usando un VIN ficticio y dos DTC
ficticios con y sin eco. No valida J2534/CAN ni comunicación física.

## Lectura conjunta de motor y transmisión

En la ventana principal, **Informe del vehículo** solicita el kilometraje del
tablero, consulta motor y transmisión secuencialmente y ofrece JSON y PDF. La
lectura conjunta restringe las solicitudes a las direcciones SSM2 específicas
(0x10 y 0x18 en K-line; 0x7E0 y 0x7E1 en CAN) y activa temporalmente la
política de solo lectura. Si una ECU no responde, se identifica como fallida y
se conserva la lectura de la otra. No prueba direcciones alternativas ni SSM1:
un módulo que use otro destino aparecerá como fallido aunque pueda existir.

El VIN se consulta en motor por K-line solo si la ECU lo anuncia; CAN sigue sin
consultarse por la limitación de la implementación existente. Los demás módulos
figuran como no consultados. Esta función está probada con ECU simuladas, no con
OpenPort o vehículos físicos. Para ejecutar la prueba local, después de compilar:

```bash
FREESSM_QT_BUILD_DIR=/ruta/al/directorio/de/compilacion ./tests/run_qt_report_scan.sh
```

## Copia con VIN oculto

Para preparar una copia separada para revisar antes de compartir:

```bash
python3 tools/vehicle_report_redact.py --ocultar-km informe.json informe-compartir.json
```

El comando oculta el valor del campo VIN y secuencias con forma de VIN en
valores de texto; opcionalmente quita el kilometraje declarado. No sobrescribe
el archivo de salida. No elimina nombres de archivos, fechas, modelo, ROM-ID u
otros datos identificables, y no garantiza anonimato: revisa la copia antes de
compartirla. El campo `datos_ocultados` deja constancia de la transformación.
La prueba es `python3 -B tests/test_vehicle_report_redact.py`.

## Registro CSV de mediciones

Al iniciar la lectura de bloques de medición, FreeSSM reserva un nuevo archivo
`measurements_YYYYMMDD_HHMMSS_XXXXXX.csv` en el directorio de trabajo, sin
sobrescribir el antiguo `measurements.csv` ni otras sesiones. La ruta aparece en
la descripción emergente del botón de inicio/parada. Las columnas de tiempo
contienen segundos Unix y segundos transcurridos; el tiempo transcurrido usa un
reloj monotónico. Para probar la reserva de nombres sin vehículo:

```bash
./tests/run_measurement_file.sh
```

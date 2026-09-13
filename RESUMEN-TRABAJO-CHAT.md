# Bitácora técnica del fork FreeSSM

Documento generado el 12 de septiembre de 2026 a partir del trabajo realizado
durante este chat, del historial Git y del estado real del repositorio.

## 1. Estado ejecutivo

Se preparó el fork `alofredo-alo/FreeSSM` como una base actualizada para
diagnóstico Subaru en Windows, con prioridad en Tactrix OpenPort 2.0 y los
siguientes vehículos:

| Prioridad | Vehículo | Objetivo inicial |
| --- | --- | --- |
| 1 | Subaru Outback 2010 3.6R | Motor y TCU 5EAT; investigar `AT OIL TEMP` y posteriormente VDC/sensor de ángulo de dirección |
| 2 | Subaru Outback 2007 2.5 | Motor y transmisión por SSM2/K-line |
| 3 | Subaru Forester 2015 2.0 | Detectar módulos y extender cobertura moderna/SSM3 |

Resultado principal alcanzado:

- `FreeSSM.exe` se compila para Windows de **64 bits**.
- `j2534_broker.exe` se compila para Windows de **32 bits**.
- El broker de 32 bits carga `op20pt32.dll`, que es la DLL J2534 x86 de Tactrix.
- FreeSSM x64 y el broker x86 se comunican mediante pipes anónimos locales; no
  se abre ningún puerto TCP o UDP.
- La interfaz OpenPort 2.0 fue detectada y abierta correctamente en Windows.
- El usuario confirmó conexión real con el vehículo.
- La lectura de DTC todavía necesita validación: FreeSSM muestra “sin códigos”,
  pero aún no está probado que la tabla de capacidades/direcciones de la TCU sea
  correcta para ese vehículo.
- El soporte moderno de ABS/VDC y del sensor de ángulo de dirección aún no está
  implementado.
- La interfaz y la documentación principal están disponibles en español.
- Existe un paquete portable reproducible generado por GitHub Actions.

La base funcional descrita en este documento corresponde al commit:

```text
5089286ee9f0e7172b60b7a1f029abd4fa532b52
Complete Spanish translation contexts
```

## 2. Repositorio y estrategia Git

Repositorio local:

```text
/home/alopez/Personal/Autos/Taller/FreeSSM
```

Remotos configurados:

```text
origin    https://github.com/alofredo-alo/FreeSSM.git
upstream  https://github.com/Comer352L/FreeSSM.git
tomflv    https://github.com/TomFLV/FreeSSM.git
```

Se corrigió el destino del fork después del cambio de cuenta de GitHub y se dejó
`origin` apuntando a `alofredo-alo/FreeSSM`.

Estado comprobado antes de generar esta bitácora:

```text
master 5089286 [origin/master] Complete Spanish translation contexts
```

La rama de integración quedó en:

```text
integration/next e24aff1 Add Spanish J2534 quick-start documentation
```

`integration/next` fue incorporada completamente a `master`; Git confirma que
`e24aff1` es ancestro de `master`. Los dos commits posteriores de español están
directamente sobre `master`.

Por decisión del owner, el flujo futuro de este fork será trabajar directamente
sobre `master`, evitando ramas temporales para cambios solicitados expresamente.
El workflow de Windows se ejecuta en pushes y pull requests contra `master`, y
también puede iniciarse manualmente.

## 3. Diagnóstico inicial del proyecto original

La rama `master` del upstream original estaba detenida en:

```text
1a0fa0934581b3383adfd2722050503695ca9dab
2024-04-07
Add files via upload (#74)
```

Sin embargo, existían desarrollos posteriores fuera de `master`, en ramas y
pull requests. Se inspeccionaron esos refs y se incorporaron los que aportaban a
la base de integración:

- Ajustes persistentes para TCU Denso E5AT.
- Marca de tiempo en exportaciones CSV.
- Prueba de concepto SSM3 ISO14230.
- Soporte SSM3 para TPMS y diálogo de la unidad TPMS.
- Trabajo previo sobre Windows x64 con una DLL J2534 x86, usado como referencia
  para completar una solución reproducible y empaquetada.

Los merges realizados en `integration/next` fueron:

```text
0956a63 Merge upstream/e5at-permanent-adjustments
e220753 Merge upstream/pr/81
53ff41a Merge upstream/pr/89
```

El delta total actual respecto de `upstream/master` es:

```text
71 archivos modificados
7246 inserciones
595 eliminaciones
```

Ese delta incluye tanto los desarrollos externos integrados como J2534 x64/x86,
seguridad de solo lectura, CI, empaquetado, documentación y traducción española.

### 3.1 Tecnologías y estructura

El proyecto es principalmente una aplicación de escritorio en C++ con Qt:

| Componente | Tecnología |
| --- | --- |
| Aplicación | C++11 con Qt 4/Qt 5; regla condicional C++17 para Qt 6.2+ |
| Interfaz gráfica | Qt Widgets y archivos Qt Designer `.ui` |
| Build | qmake mediante `FreeSSM.pro` |
| Protocolos | SSM1, SSM2, J2534 y cobertura SSM3 parcial |
| Definiciones | XML y tablas compiladas en C++ |
| XML | TinyXML2 incluido en el repositorio |
| Traducciones | Qt Linguist `.ts`/`.qm` |
| Automatización Windows | PowerShell |
| CI | GitHub Actions/YAML |
| Licencia | GNU GPL v3 |

Inventario aproximado de archivos versionados más relevantes:

```text
65 .h
63 .cpp
32 .ui
5  .xml
4  .ts
4  .md antes de esta bitácora
1  .pro
1  .ps1
1  .yml
```

El proyecto conserva bastante compatibilidad histórica. Esto es útil para los
Subaru antiguos, pero aumenta el costo de introducir módulos modernos porque la
lógica de diagnóstico está fuertemente basada en SSM1/SSM2, flag bytes y
direcciones de memoria específicas.

## 4. Actualizaciones externas integradas

### 4.1 Ajustes permanentes Denso E5AT

Se integró la rama `e5at-permanent-adjustments`, que agrega:

- Información sobre si un ajuste es temporal o persistente.
- Comandos para guardar ajustes permanentes.
- Definiciones de ajustes permanentes soportados por determinadas TCU Denso
  E5AT.
- Refactor importante de `CUcontent_Adjustments`.

Esto es relevante para la transmisión 5EAT del Outback 2010, pero **no se ha
probado ninguna escritura sobre el vehículo**. Las primeras sesiones se
mantienen en modo de solo lectura.

### 4.2 Timestamp en CSV

Se integró el PR 81:

- Timestamp en las exportaciones CSV.
- Helper `csvfile.h`.
- Cambios de captura en `CUcontent_MBsSWs`.

Esto permite correlacionar valores de transmisión, temperatura, voltaje y otros
measuring blocks durante una prueba.

### 4.3 TPMS y base SSM3

Se integró el PR 89, que contiene:

- Transporte SSM3 ISO14230.
- Protocolo SSM3 específico para TPMS.
- Lectura de identificadores locales.
- Diálogo TPMS en la interfaz.
- Icono y recursos relacionados.

Esta integración aporta una base técnica para ampliar módulos modernos, pero no
equivale a soporte SSM3 general para todos los módulos del Forester 2015.

## 5. Implementación J2534 x64/x86

### 5.1 Problema resuelto

El OpenPort 2.0 instala normalmente esta biblioteca:

```text
C:\WINDOWS\SysWOW64\op20pt32.dll
```

Es una DLL de 32 bits. Windows no permite cargarla directamente dentro de
`FreeSSM.exe` si FreeSSM es un proceso de 64 bits. El mensaje `[32-bit]` mostrado
en la selección de interfaces describe la arquitectura del driver; no indica
por sí mismo una incompatibilidad.

La solución implementada separa ambos procesos:

```text
FreeSSM.exe x64
    |
    | pipes anónimos locales
    v
j2534_broker.exe x86
    |
    | LoadLibrary / SAE J2534 04.04
    v
op20pt32.dll x86
    |
    v
Tactrix OpenPort 2.0
```

### 5.2 Selección automática según arquitectura

`FreeSSM.pro` selecciona el backend en compilación:

- Si `QT_ARCH` es `x86_64`, compila el cliente del broker y
  `J2534_API_broker.cpp`.
- Si el target es x86, conserva la carga directa de la DLL del proveedor.
- En instalaciones x64 incluye `j2534_broker.exe` junto a la aplicación.

Archivos principales:

```text
src/windows/j2534_broker.cpp
src/windows/J2534_broker_client.cpp
src/windows/J2534_broker_client.h
src/windows/J2534_API_broker.cpp
src/windows/J2534_API.cpp
src/windows/J2534_API.h
src/J2534DiagInterface.cpp
src/J2534misc.cpp
scripts/build-j2534-broker.ps1
```

### 5.3 Funciones transportadas por el broker

El backend implementa las operaciones que FreeSSM necesita:

- Enumeración de DLL J2534 en el registro de Windows de 32 bits.
- Carga de la DLL seleccionada.
- `PassThruOpen` y `PassThruClose`.
- `PassThruConnect` y `PassThruDisconnect`.
- `PassThruReadVersion` y `PassThruGetLastError`.
- Lectura y escritura de mensajes.
- Inicio y detención de filtros.
- Mensajes periódicos.
- `PassThruIoctl`, incluido `READ_VBATT` y configuración de canal.
- Programación de voltaje expuesta por API, aunque no forma parte de la prueba
  inicial y está fuera del alcance de diagnóstico seguro actual.

El ejecutable de broker acepta internamente comandos como `LIST`, `LOAD`,
`OPEN`, `VBATT`, `CONNECT`, `FILTER`, `WRITE`, `READ`, `IOCTL`, `VERSION`,
`DISCONNECT`, `CLOSE` y `QUIT`.

### 5.4 Detección de drivers

FreeSSM diferencia DLL registradas en las vistas x86 y x64 del registro y
muestra la arquitectura junto al nombre del dispositivo. Esto evita interpretar
una DLL x86 como si debiera cargarse dentro del proceso x64.

El registro validado en el equipo Windows fue:

```text
HKLM\SOFTWARE\PassThruSupport.04.04\
  Tactrix Inc. - OpenPort 2.0 J2534 ISO/CAN/VPW/PWM

Name             = OpenPort 2.0 J2534 ISO/CAN/VPW/PWM
Vendor           = Tactrix Inc.
FunctionLibrary  = C:\WINDOWS\SysWOW64\op20pt32.dll
CAN              = 1
ISO9141          = 1
ISO14230         = 1
ISO15765         = 1
J1850PWM         = 0
J1850VPW         = 0
```

La máscara de protocolos informada por el broker fue `60`, equivalente a
ISO9141 + ISO14230 + CAN + ISO15765.

### 5.5 Probe sin vehículo

Se agregó el argumento:

```powershell
Start-Process -FilePath .\FreeSSM.exe -ArgumentList '--j2534-probe' `
  -NoNewWindow -Wait
```

El probe:

- Enumera drivers J2534.
- Muestra nombre, DLL, arquitectura, versión de API y protocolos.
- Carga la DLL.
- Ejecuta `PassThruOpen`.
- Lee versión.
- Ejecuta `PassThruClose`.
- Devuelve `RESULT: PASS` cuando el ciclo termina correctamente.

No llama `PassThruConnect`, no abre un canal al bus del vehículo y no transmite
tramas.

## 6. Modo global de solo lectura

Se agregó:

```powershell
.\FreeSSM.exe --read-only
```

El título de la aplicación debe mostrar `[READ-ONLY]`.

La protección no se limita a ocultar botones. La política global
`DiagnosticSafety` también bloquea rutas de escritura en las capas de protocolo.
En este modo:

- Se deshabilitan Ajustes.
- Se deshabilitan Pruebas de sistema/actuadores.
- Se deshabilita Borrar memoria y Borrar memoria 2.
- Las selecciones equivalentes por línea de comandos son rechazadas.
- Se bloquean escrituras SSM1 y SSM2 en las capas inferiores.
- Se evita el comando automático de detención de actuadores que FreeSSM podía
  enviar al preparar una unidad de control.

Esta protección fue agregada para obtener una primera captura del vehículo sin
borrar evidencia ni cambiar configuraciones. No convierte automáticamente en
segura cualquier función futura: toda ruta nueva de escritura debe respetar la
misma política.

## 7. Compilación y paquete Windows reproducible

Se creó:

```text
.github/workflows/windows-j2534.yml
```

Toolchain usado:

| Artefacto | Arquitectura | Toolchain |
| --- | --- | --- |
| `FreeSSM.exe` | x64 | Qt 5.15.2 + MinGW 8.1 x64, C++11 |
| `j2534_broker.exe` | x86 | Visual Studio 2022 Build Tools, C++14, runtime `/MT` |

El script del broker:

- Busca Visual Studio mediante `vswhere.exe`.
- Carga explícitamente el entorno `vcvarsall.bat x86`.
- Compila con optimización `/O2`.
- Enlaza `Advapi32.lib`.
- Construye en un directorio temporal.
- Valida el encabezado PE.
- Falla si `Machine` no es `0x014c`/i386.
- Calcula SHA-256 del ejecutable resultante.

El workflow de CI:

1. Instala Python 3.13.
2. Instala `aqtinstall==3.3.0`.
3. Descarga Qt 5.15.2 y MinGW 8.1 x64.
4. Ejecuta qmake y actualiza traducciones con `lupdate`.
5. Falla si la traducción española contiene mensajes `unfinished`.
6. Genera los `.qm` con `lrelease`.
7. Compila FreeSSM x64.
8. Compila y valida el broker x86.
9. Ensambla un runtime portable determinista.
10. Genera `BUILD-INFO.txt` con commit y hashes.
11. Publica `FreeSSM-J2534-x64` como artefacto por 14 días.

El paquete incluye explícitamente:

- `FreeSSM.exe`.
- `j2534_broker.exe`.
- DLL Qt y runtimes MinGW requeridos.
- `platforms/qwindows.dll`.
- `printsupport/windowsprintersupport.dll`.
- Plugins opcionales disponibles de styles e imageformats.
- Definiciones.
- Fuentes Liberation Sans.
- Licencia.
- README principal y guías J2534 en español e inglés.
- Traducciones de aplicación inglesa, alemana, turca y española.
- Traducciones nativas Qt disponibles.

Durante el desarrollo se corrigieron consecutivamente:

- Colisión de macros de Windows en el probe.
- Descubrimiento incorrecto de plugins Qt.
- Inclusión explícita de plugins requeridos.
- Dependencia de resultados variables de `windeployqt`.
- Ensamblado determinista del runtime y validación de archivos obligatorios.

Commits asociados:

```text
5f45074 Add 64-bit Windows J2534 broker support
df889e8 Fix Windows macro collision in J2534 probe
fe9d661 Fix Qt plugin discovery in Windows package
fd760cb Package required Qt plugins explicitly
6073d9f Make Windows runtime packaging deterministic
```

## 8. Traducción completa al español

Antes de este trabajo FreeSSM ofrecía inglés, alemán y turco. Se añadió español
como cuarto idioma:

- `QLocale::Spanish` en la lista de locales soportados.
- Selección manual **Spanish** en Preferencias.
- Detección por idioma y no por coincidencia exacta de región; por ejemplo,
  `es_CL` puede resolver a español aunque la traducción no esté ligada a una
  región específica.
- Cambio de idioma inmediato después de confirmar Preferencias.
- `FreeSSM_es.ts` como catálogo fuente.
- `FreeSSM_es.qm` dentro del artefacto portable.

Estado validado del catálogo:

```text
422 mensajes
0 unfinished
0 traducciones vacías
placeholders Qt conservados
```

El primer catálogo tenía 22 traducciones ubicadas en contextos Qt incorrectos.
El workflow `34481117286` detectó el problema y falló correctamente. Se regeneró
el catálogo canónico con `lupdate`, se movieron las traducciones a sus contextos
reales y el commit `5089286` completó la corrección.

Las etiquetas técnicas dinámicas provenientes de tablas de definiciones SSM
mantienen fallback a inglés cuando no existe una tabla española específica. No
se tradujeron mecánicamente nombres técnicos cuya interpretación pudiera afectar
un diagnóstico.

Commits asociados:

```text
5888e8e Add Spanish interface translation
5089286 Complete Spanish translation contexts
```

## 9. Documentación creada o actualizada

Se reemplazó la entrada principal por un README orientado al fork y de acceso
rápido:

```text
README.md
```

Contiene:

- Explicación directa de la arquitectura x64/x86.
- Instalación del driver Tactrix.
- Descarga del artefacto desde Actions.
- Requisito de extraer el ZIP completo.
- Selección del idioma español.
- Probe de interfaz sin vehículo.
- Primera sesión protegida con `--read-only`.
- Matriz inicial de vehículos.
- Advertencia sobre la falta de soporte moderno VDC/ABS.

También se crearon/actualizaron:

```text
README.J2534-Windows.es.md
README.J2534-Windows.md
README.txt
```

La guía española incluye compilación local, prueba del registro, orden seguro de
conexión, captura del Outback 2010 y límites actuales.

Commit asociado:

```text
e24aff1 Add Spanish J2534 quick-start documentation
```

## 10. Validaciones realizadas

### 10.1 GitHub Actions

Ejecución final exitosa:

```text
Workflow run: 34482384434
Artifact ID:  10154335814
Artifact:     FreeSSM-J2534-x64
```

Enlace de la ejecución:

```text
https://github.com/alofredo-alo/FreeSSM/actions/runs/34482384434
```

Enlace directo temporal del artefacto:

```text
https://github.com/alofredo-alo/FreeSSM/actions/runs/34482384434/artifacts/10154335814
```

Digest informado para el artefacto:

```text
sha256:77bc391ce2374bb7f75ff3d77967c798c08a96da1cc37555a0a1706944a693c5
```

Ese artefacto corresponde al commit `5089286`, tiene retención de 14 días y
caduca aproximadamente el 24 de septiembre de 2026. Si ya no existe, se debe
ejecutar nuevamente el workflow mediante **Run workflow**.

### 10.2 BUILD-INFO validado en Windows

El usuario confirmó este contenido desde el equipo Windows:

```text
commit=5089286ee9f0e7172b60b7a1f029abd4fa532b52
FreeSSM.exe.sha256=10168532692816516BDC0DDC4226905120620255A7665BAD6B80DCC372C7073C
j2534_broker.exe.sha256=F46B4ECEE7080EC47A75D2E41BD65EC038F2604A21F213669D3EF6B92467CD38
```

Esto asegura que la prueba se hizo con el binario esperado y no con una descarga
anterior.

### 10.3 OpenPort 2.0 inspeccionado en Linux

La inspección USB en el equipo Linux confirmó:

```text
USB ID:       0403:cc4d
Fabricante:   Tactrix
Producto:     OpenPort 2.0
Serie:        TAhJALxt
Dispositivo:  /dev/ttyACM0
Driver Linux: cdc_acm
Enlace:       /dev/serial/by-id/usb-Tactrix_OpenPort_2.0_TAhJALxt-if00
```

El dispositivo enumeró de forma estable y sin resets USB observados. Esto
descartó un cable exclusivamente de carga y confirmó el camino USB físico. No se
instaló ni ejecutó el SSM3 antiguo en Linux.

### 10.4 Broker y DLL Tactrix validados en Windows

Se ejecutó manualmente:

```powershell
@(
    "LIST"
    "LOAD C:\WINDOWS\SysWOW64\op20pt32.dll"
    "OPEN"
    "VERSION"
    "CLOSE"
    "QUIT"
) | .\j2534_broker.exe
```

Resultado recibido:

```text
READY j2534-broker 32-bit
OK 1
OpenPort 2.0 J2534 ISO/CAN/VPW/PWM  C:\WINDOWS\SysWOW64\op20pt32.dll  60
OK C:\WINDOWS\SysWOW64\op20pt32.dll
OK 1
OK 1.17.4877  1.02.4870 Feb 3 2017 23:36:05  04.04
OK
```

Esto prueba de forma concreta:

- Lectura correcta del registro J2534 x86.
- Inicio del broker x86.
- Carga de `op20pt32.dll` x86.
- `PassThruOpen` exitoso.
- Lectura de firmware/DLL/API.
- API J2534 04.04.
- `PassThruClose` exitoso.

### 10.5 Conexión con el vehículo

El usuario confirmó posteriormente que consiguió conectar FreeSSM al vehículo.
Por lo tanto ya están validados:

- USB OpenPort.
- Driver Tactrix.
- Registro J2534.
- Puente x64 a x86.
- Apertura del dispositivo.
- Creación de al menos una sesión de comunicación con el vehículo.

La aplicación indicó “sin códigos”. Esa respuesta todavía no valida que la
lectura de DTC de la TCU sea completa.

## 11. Estado del diagnóstico del Outback 2010 3.6R

Síntomas informados:

- Problema relacionado con sensor de dirección.
- Luz `AT OIL TEMP` parpadeando.
- FreeSSM conecta, pero muestra “sin códigos”.

### 11.1 Qué cubre actualmente FreeSSM por CAN

La ruta SSM2/ISO15765 usa inicialmente:

```text
ECM/Engine: 0x7E0
TCU:        0x7E1
CAN:        500 kbit/s
```

Para el Outback 2010 se recomendó desmarcar **Prefer ISO-14230 over ISO-15765**
para que la prueba prefiera CAN.

### 11.2 Por qué “sin códigos” no es concluyente

FreeSSM no ejecuta necesariamente una consulta UDS/OBD genérica para enumerar
todos los DTC de todos los módulos. En la ruta existente:

1. Lee identificación y flag bytes de la unidad.
2. Las definiciones interpretan esos flag bytes.
3. Se construye una lista de direcciones de memoria para códigos actuales,
   temporales, históricos o memorizados.
4. `readAddresses_permanent()` consulta esas direcciones.
5. `processDCsRawdata()` transforma bits/datos en códigos definidos.

Por eso “sin códigos” puede significar una de estas dos situaciones:

- La memoria seleccionada realmente no contiene DTC.
- La TCU responde y entrega datos, pero las capacidades o direcciones de DTC
  conocidas por FreeSSM no coinciden con esa variante/ROM.

Antes de cambiar la implementación se necesita registrar la identificación real
de la TCU y confirmar qué grupos de códigos fueron consultados.

### 11.3 Separación entre TCU y VDC/ABS

El sensor de ángulo de dirección pertenece normalmente al dominio VDC/ABS, no a
la ECM y no necesariamente a la memoria de la TCU. Por lo tanto:

- Una TCU sin DTC visible no descarta un DTC en VDC/ABS.
- La luz `AT OIL TEMP` requiere revisar la TCU y las condiciones de alimentación
  y red, además de correlacionar otros módulos.
- El diálogo ABS/VDC actual de FreeSSM está basado en SSM1 antiguo y no debe
  usarse como evidencia válida para el Outback 2010.
- No se deben adivinar IDs CAN ni servicios de escritura sobre el vehículo.

### 11.4 Captura solicitada y todavía pendiente

Ejecutar siempre:

```powershell
.\FreeSSM.exe --read-only
```

En **Transmission** recopilar:

- Pantalla completa de **Information**.
- `System Type`.
- ROM ID.
- DTC actuales/temporales.
- DTC históricos/memorizados.
- Temperatura ATF y otros measuring blocks disponibles.

Después repetir en **Engine**.

FreeSSM ya tenía una captura interna accesible desde la ventana principal con
`Ctrl+Alt+Enter`. La secuencia indicada fue cerrar la ventana de Transmission,
dejar visible la ventana principal y pulsar una vez la combinación. El archivo
esperado es `dump.dat`, `dump2.dat`, etc. Para localizar el último:

```powershell
$dump = Get-ChildItem "$HOME\dump*.dat" |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

$dump | Select-Object FullName, Length, LastWriteTime
Get-Content $dump.FullName
```

La captura actual incluye SYS ID, ROM ID y flag bytes de ECM/TCU. Todavía no
incluye el raw completo de memoria DTC. Si los datos de identificación muestran
que FreeSSM seleccionó una tabla equivocada, el próximo cambio será agregar una
captura read-only específica de capacidades, direcciones consultadas y bytes DTC.

También quedó pendiente medir voltaje mediante J2534:

```powershell
@(
    "LOAD C:\WINDOWS\SysWOW64\op20pt32.dll"
    "OPEN"
    "VBATT"
    "VERSION"
    "CLOSE"
    "QUIT"
) | .\j2534_broker.exe
```

Un valor estático razonable con motor detenido suele estar alrededor de
12.0–13.0 V, pero la interpretación final debe considerar contacto, carga de la
batería y caída durante arranque. No se recibió todavía el resultado `VBATT`.

### 11.5 Riesgo operativo

Mientras `AT OIL TEMP` siga parpadeando:

- No borrar códigos antes de capturar todas las memorias.
- No ejecutar ajustes ni pruebas de actuadores.
- No desconectar módulos para “probar” sin registrar antes DTC y voltajes.
- Evitar exigir la transmisión hasta identificar la causa.
- No concluir todavía que la falla es sensor, cableado o módulo; faltan DTC VDC,
  DTC TCU confiables, live data y pruebas eléctricas.

## 12. Alcance real por vehículo

| Vehículo | Transporte probable/inicial | Estado actual | Pendiente principal |
| --- | --- | --- | --- |
| Outback 2007 2.5 | SSM2 por K-line/ISO9141, 4800 baud | Backend J2534 preparado para motor y transmisión | Prueba física y captura de identificación/DTC |
| Outback 2010 3.6R | SSM2 por ISO15765 CAN, 500 kbit/s | OpenPort y conexión validados; ECM/TCU direccionados | Validar lectura DTC TCU y luego implementar VDC moderno |
| Forester 2015 2.0 | Plataforma posterior, con cobertura SSM3 necesaria | Base SSM3/TPMS parcial integrada | Descubrir módulos, servicios, IDs y definiciones reales |

No debe declararse compatibilidad total con estos tres vehículos todavía. La
compatibilidad actual es del transporte J2534 y de las rutas de motor/TCU ya
existentes, no de todos los módulos Subaru.

## 13. SSM3 05-2015 usado como referencia

Se inspeccionó en modo de solo lectura:

```text
/home/alopez/Torrents/SSM3 05-2015 v1.45.59.9/
```

No se ejecutó `Setup.exe`, no se instalaron drivers y no se modificó ningún
archivo.

Contenido potencialmente útil localizado:

```text
Select Monitor/Sdr/BasicDB*.dat
Select Monitor/Sdr/InformationBase.dat
Select Monitor/VariantCode.dat
Select Monitor/ES_Variant/*.csv
Select Monitor/Locale/ESP.csv
Select Monitor/Help/SSMIII_UserGuide_ESP.pdf
Cfap/*.sdb
Cfap/ESP.csv
Roughness/bin/ESP.csv
FlashWrite/Driver_Sub/NSMIF32.dll
FlashWrite/EcuData/*.pak
```

Usos posibles:

- Identificar nombres y tipos de módulos.
- Correlacionar variantes/ROM.
- Investigar identificadores locales y definiciones de datos.
- Obtener terminología española coherente.
- Comparar flujo de selección de vehículo y módulo.
- Guiar capturas read-only para VDC/ABS y vehículos 2015.

Limitaciones:

- Aún no se extrajeron direcciones ni servicios de diagnóstico de estas bases.
- Los archivos son de software propietario; no deben copiarse directamente al
  fork GPL sin revisar licencia y procedencia.
- Los paquetes `.pak` de reprogramación no forman parte del objetivo actual y no
  deben enviarse a una ECU durante el desarrollo de diagnóstico.
- El enfoque seguro es documentar comportamiento interoperable, validar contra
  capturas propias y reimplementar sólo lo necesario con código original.

## 14. Archivos principales modificados o agregados

### J2534 y seguridad

```text
src/DiagnosticSafety.h
src/J2534DiagInterface.cpp
src/J2534DiagInterface.h
src/J2534misc.cpp
src/J2534misc.h
src/windows/J2534_API.cpp
src/windows/J2534_API.h
src/windows/J2534_API_broker.cpp
src/windows/J2534_broker_client.cpp
src/windows/J2534_broker_client.h
src/windows/j2534_broker.cpp
src/main.cpp
src/CmdLine.cpp
src/ControlUnitDialog.cpp
src/SSMP1base.cpp
src/SSMP1communication.cpp
src/SSMP2communication_core.cpp
```

### Build y CI

```text
FreeSSM.pro
scripts/build-j2534-broker.ps1
.github/workflows/windows-j2534.yml
```

### Español y documentación

```text
FreeSSM_es.ts
src/Languages.h
src/FreeSSM.cpp
src/Preferences.cpp
README.md
README.J2534-Windows.es.md
README.J2534-Windows.md
README.txt
```

### Integraciones externas relevantes

```text
src/CUcontent_Adjustments.cpp
src/CUcontent_MBsSWs.cpp
src/CUcontent_LocalIdentifiers.cpp
src/LocalIdentifier.h
src/SSM3protocolTPMS.cpp
src/TPMSdialog.cpp
src/csvfile.h
```

## 15. Historial cronológico de commits del trabajo

```text
0956a63  Merge e5at-permanent-adjustments en integration/next
e220753  Merge PR 81/timestamp CSV en integration/next
53ff41a  Merge PR 89/SSM3 TPMS en integration/next
5f45074  Add 64-bit Windows J2534 broker support
df889e8  Fix Windows macro collision in J2534 probe
fe9d661  Fix Qt plugin discovery in Windows package
fd760cb  Package required Qt plugins explicitly
6073d9f  Make Windows runtime packaging deterministic
e24aff1  Add Spanish J2534 quick-start documentation
5888e8e  Add Spanish interface translation
5089286  Complete Spanish translation contexts
```

Fechas de los commits propios principales: 9 y 10 de septiembre de 2026.

## 16. Pendientes recomendados en orden

1. Obtener `VBATT`, captura de **Transmission → Information**, ambos grupos de
   DTC y el último `dump*.dat` del Outback 2010.
2. Confirmar si live data de TCU funciona de forma estable, especialmente
   temperatura ATF, voltaje y estados relevantes.
3. Agregar logging read-only de requests/responses DTC y de la tabla de
   direcciones seleccionada, sin imprimir datos sensibles innecesarios.
4. Corregir la selección de capacidades/DTC para la ROM de la TCU si se confirma
   un falso “sin códigos”.
5. Identificar de forma pasiva el módulo VDC/ABS y documentar sus IDs/servicios
   exactos antes de programar soporte.
6. Implementar primero identificación, DTC y live data VDC exclusivamente de
   lectura.
7. Agregar el valor del sensor de ángulo de dirección y validarlo con volante
   centrado, giro controlado y comparación eléctrica/mecánica.
8. Repetir la matriz de pruebas en Outback 2007 y Forester 2015.
9. Crear fixtures o reproducciones offline de tramas para no depender del auto en
   cada cambio y evitar regresiones.
10. Sólo después de validar lecturas, evaluar borrado de DTC o funciones de
    escritura módulo por módulo.

## 17. Criterio de finalización del objetivo inicial

El objetivo “J2534 funcionando” está parcialmente cumplido: la arquitectura, el
driver, el broker y la conexión real ya funcionan. Para declarar compatibilidad
de diagnóstico con cada vehículo todavía se requiere:

- Identificación consistente del módulo.
- Lectura confiable de DTC actuales e históricos.
- Live data estable.
- Ausencia de escrituras accidentales en `--read-only`.
- Pruebas repetibles por vehículo y módulo.
- Para el Outback 2010, soporte moderno VDC/ABS y lectura del sensor de ángulo de
  dirección.

Hasta completar esos puntos, el fork debe considerarse una versión de integración
para pruebas controladas y no una herramienta terminada para todos los módulos.

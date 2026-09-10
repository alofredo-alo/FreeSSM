# FreeSSM J2534 en Windows de 64 bits

[Volver al README principal](README.md) ·
[English version](README.J2534-Windows.md)

Este fork ejecuta FreeSSM como aplicación Qt de 64 bits y carga las bibliotecas
J2534 04.04 de 32 bits, incluido Tactrix OpenPort 2.0, mediante un broker separado
de 32 bits. El broker usa pipes anónimos: no abre ni escucha puertos TCP/UDP.

`FreeSSM.exe` y `j2534_broker.exe` deben permanecer en la misma carpeta. Instala
el driver vigente del fabricante para OpenPort 2.0. No copies `op20pt32.dll` a la
carpeta de FreeSSM ni intentes registrarla manualmente.

## Descargar e instalar

1. Instala el paquete oficial
   [OpenPort drivers and J2534 DLL](https://www.tactrix.com/index.php?Itemid=61&id=38&option=com_content&view=category)
   de Tactrix.
2. Abre
   [Actions / Windows J2534 package](https://github.com/alofredo-alo/FreeSSM/actions/workflows/windows-j2534.yml).
3. Entra en la ejecución exitosa más reciente y descarga
   `FreeSSM-J2534-x64` desde la sección **Artifacts**.
4. Extrae el ZIP completo. No ejecutes `FreeSSM.exe` directamente dentro del ZIP.
5. Verifica que `FreeSSM.exe`, `j2534_broker.exe`, las DLL Qt y las carpetas
   `platforms`, `printsupport` y `definitions` permanezcan juntas.
6. FreeSSM usa español automáticamente si coincide con el idioma de Windows. Si
   inicia en otro idioma, abre **Preferences**, elige **Spanish** en **Language**
   y confirma con **OK**.

Los artefactos de GitHub Actions son temporales. Si ya caducaron, usa
**Run workflow** para generar uno nuevo.

## Compilación reproducible

El workflow `.github/workflows/windows-j2534.yml` genera el artefacto completo
`FreeSSM-J2534-x64` con Qt 5.15.2 y MinGW 8.1 x64. El broker se compila con el
compilador x86 de Visual Studio y el workflow valida que su máquina PE sea i386.

Para compilar FreeSSM localmente, usa una consola Qt 5.15.2 MinGW 8.1 x64:

```powershell
qmake.exe FreeSSM.pro "CONFIG+=release"
mingw32-make.exe translation
mingw32-make.exe -j2 release
```

Luego compila el broker desde PowerShell. Se requiere Visual Studio 2022 Build
Tools con la carga de trabajo **Desktop development with C++** para x86/x64:

```powershell
./scripts/build-j2534-broker.ps1 -OutputPath ./j2534_broker.exe
```

También se puede usar un compilador MinGW i686:

```powershell
i686-w64-mingw32-g++.exe -std=c++11 -O2 -static `
  -o j2534_broker.exe src/windows/j2534_broker.cpp -ladvapi32
```

## Orden seguro de prueba

### 1. Probar solamente la interfaz USB

Conecta el OpenPort 2.0 al computador, sin conectarlo todavía al vehículo. Desde
PowerShell enumera el driver registrado y prueba `PassThruOpen`, lectura de
versión y `PassThruClose`:

```powershell
Start-Process -FilePath .\FreeSSM.exe -ArgumentList '--j2534-probe' `
  -NoNewWindow -Wait
```

Este probe no llama `PassThruConnect`, no crea un canal al bus del vehículo, no
transmite tramas, no borra memoria, no ejecuta actuadores y no cambia ajustes.

Si no encuentra el driver, inspecciona ambas vistas del registro sin modificarlas:

```powershell
reg.exe query "HKLM\SOFTWARE\PassThruSupport.04.04" /s /reg:32
reg.exe query "HKLM\SOFTWARE\PassThruSupport.04.04" /s /reg:64
```

### 2. Probar la comunicación de lectura

Con el encendido apagado, conecta la interfaz al OBD. Luego pon el encendido en
`ON`, con el motor detenido. En Preferencias selecciona el dispositivo J2534 y
ejecuta la prueba de interfaz. Sólo se envían solicitudes Subaru de identificación
y lectura; no se borra ni ajusta ninguna unidad de control.

### 3. Iniciar la sesión protegida

Cierra FreeSSM y vuelve a abrirlo en modo de sólo lectura:

```powershell
.\FreeSSM.exe --read-only
```

Confirma que `[READ-ONLY]` aparezca en el título. En este modo FreeSSM bloquea las
escrituras de protocolo y deshabilita Ajustes, Pruebas de sistema, Borrar memoria
y Borrar memoria 2. También evita la orden automática de detención de actuadores
que normalmente se envía al conectar.

### 4. Capturar la línea base del Outback 2010

Abre primero **Transmission** y guarda la identificación completa de la TCU,
todos los DTC actuales e históricos y su estado. Repite después con **Engine**.
No uses una sesión normal con escritura habilitada durante esta captura.

## Alcance por vehículo

| Vehículo | Primer transporte | Objetivo actual |
| --- | --- | --- |
| Outback 2007 2.5 | SSM2 por K-line, 4800 baud | Motor y transmisión automática |
| Outback 2010 3.6R | SSM2 por ISO15765 CAN, 500 kbit/s | Motor y TCU 5EAT; máxima prioridad |
| Forester 2015 2.0 | Cobertura posterior, cercana a SSM3 | Primero detectar interfaz; los módulos todavía no están garantizados |

La ruta CAN moderna implementada actualmente direcciona Motor y Transmisión. El
valor del sensor de ángulo de dirección y sus DTC pertenecen al dominio VDC/ABS.
El diálogo ABS/VDC actual de FreeSSM implementa SSM1 antiguo y no debe considerarse
válido para el Outback 2010.

El soporte VDC se agregará cuando se obtengan las direcciones y servicios exactos
desde una captura de sólo lectura o documentación técnica confiable. No se
probarán direcciones CAN supuestas sobre un vehículo real.

## Línea base para AT OIL TEMP

Si `AT OIL TEMP` parpadea después de arrancar, puede indicar una falla detectada
por el sistema de control de transmisión y no sólo temperatura alta del fluido.
Antes de desconectar componentes o borrar memoria conserva:

- voltaje de batería con contacto y durante arranque;
- si la luz parpadea con el vehículo completamente frío;
- todos los DTC TCU, VDC y ECM, incluido su estado;
- datos actuales o freeze-frame disponibles;
- comportamiento del valor de ángulo de dirección, cuando se implemente VDC.

No se puede concluir si la causa es sensor, cableado, alimentación, red CAN o un
módulo hasta obtener esos datos.

# FreeSSM con J2534 para Windows x64

[![Windows J2534 package](https://github.com/alofredo-alo/FreeSSM/actions/workflows/windows-j2534.yml/badge.svg?branch=master)](https://github.com/alofredo-alo/FreeSSM/actions/workflows/windows-j2534.yml)

Fork de FreeSSM orientado a diagnóstico Subaru mediante interfaces SAE J2534,
incluido Tactrix OpenPort 2.0. La aplicación es de 64 bits y utiliza un broker de
32 bits para cargar las DLL J2534 de proveedores que no ofrecen una DLL x64.

> Estado: versión de integración para pruebas controladas. Motor y transmisión
> son los primeros objetivos. El soporte moderno de ABS/VDC y sensor de ángulo
> de dirección todavía no está implementado.

## Instalación rápida en Windows

1. Instala el paquete oficial
   [OpenPort drivers and J2534 DLL](https://www.tactrix.com/index.php?Itemid=61&id=38&option=com_content&view=category)
   de Tactrix. No copies `op20pt32.dll` manualmente ni la registres por tu cuenta.
2. Abre
   [Actions / Windows J2534 package](https://github.com/alofredo-alo/FreeSSM/actions/workflows/windows-j2534.yml),
   entra en la ejecución exitosa más reciente y descarga el artefacto
   `FreeSSM-J2534-x64`.
3. Extrae el ZIP completo en una carpeta local. No ejecutes la aplicación dentro
   del ZIP.
4. Mantén `FreeSSM.exe` y `j2534_broker.exe` en la misma carpeta. No se requiere
   instalar FreeSSM ni ejecutarlo como administrador.
5. Si Windows no está configurado en español, abre **Preferences**, selecciona
   **Spanish** en **Language** y confirma con **OK**. El cambio es inmediato.

Los artefactos de Actions caducan. Si no aparece uno disponible, ejecuta el
workflow manualmente con **Run workflow** y descarga el resultado cuando termine.

## Primera prueba: interfaz sin vehículo

Conecta solamente el OpenPort 2.0 por USB y ejecuta desde PowerShell dentro de la
carpeta extraída:

```powershell
Start-Process -FilePath .\FreeSSM.exe -ArgumentList '--j2534-probe' `
  -NoNewWindow -Wait
```

El resultado esperado debe identificar el driver de 32 bits y finalizar con
`RESULT: PASS`. Esta prueba abre y cierra la interfaz, pero no crea un canal hacia
el vehículo ni transmite mensajes.

Si no aparece ningún driver:

```powershell
reg.exe query "HKLM\SOFTWARE\PassThruSupport.04.04" /s /reg:32
reg.exe query "HKLM\SOFTWARE\PassThruSupport.04.04" /s /reg:64
```

## Primera sesión en el vehículo

1. Con el encendido apagado, conecta la interfaz al OBD.
2. Pon el encendido en `ON`, con el motor detenido.
3. En Preferencias selecciona la interfaz J2534 y ejecuta la prueba.
4. Cierra FreeSSM y abre una sesión protegida:

```powershell
.\FreeSSM.exe --read-only
```

Confirma `[READ-ONLY]` en el título. Este modo bloquea borrado de memoria,
ajustes, pruebas de actuadores y escrituras de protocolo.

Para el Outback 2010 3.6R, lee primero **Transmission** y guarda identificación,
DTC actuales/históricos y sus estados. Después repite en **Engine**. No borres
códigos y no uses el diálogo ABS/VDC actual: corresponde al protocolo SSM1
antiguo y no es válido para ese vehículo.

## Cobertura inicial

| Vehículo | Transporte inicial | Cobertura actual |
| --- | --- | --- |
| Outback 2007 2.5 | SSM2 por K-line, 4800 baud | Motor y transmisión automática |
| Outback 2010 3.6R | SSM2 por ISO15765 CAN, 500 kbit/s | Motor y TCU 5EAT; prioridad principal |
| Forester 2015 2.0 | Plataforma posterior, cercana a SSM3 | Detección de interfaz; módulos aún no garantizados |

## Qué incorpora este fork

- FreeSSM x64 con broker J2534 x86 mediante pipes anónimos, sin puertos TCP/UDP.
- Detección separada de drivers J2534 registrados en las vistas x86 y x64.
- Diagnóstico detallado de carga de DLL, apertura, versión y cierre.
- Modo global `--read-only` para la captura inicial.
- Interfaz seleccionable en español, inglés, alemán y turco.
- Paquete portable reproducible con Qt 5.15.2, runtimes MinGW, plugins y hashes.

## Documentación

- [Guía completa J2534 para Windows en español](README.J2534-Windows.es.md)
- [J2534 Windows guide in English](README.J2534-Windows.md)
- [README original de FreeSSM](README.txt)

## Compilación

El workflow reproducible está en
[`windows-j2534.yml`](https://github.com/alofredo-alo/FreeSSM/actions/workflows/windows-j2534.yml).
Para una compilación local y detalles del broker consulta la guía completa.

## Licencia y responsabilidad

FreeSSM se distribuye bajo GNU GPL v3, sin garantía. No es un producto de Subaru
ni está respaldado por Subaru. El diagnóstico y cualquier modificación del
vehículo se realizan bajo responsabilidad del usuario. Conserva todos los DTC y
datos base antes de desconectar componentes o borrar memoria.

# FreeSSM para Subaru: J2534 e informe de estado

[![Windows J2534 package](https://github.com/alofredo-alo/FreeSSM/actions/workflows/windows-j2534.yml/badge.svg?branch=master)](https://github.com/alofredo-alo/FreeSSM/actions/workflows/windows-j2534.yml)

Fork de FreeSSM orientado a diagnóstico Subaru mediante interfaces SAE J2534,
incluido Tactrix OpenPort 2.0. La aplicación es de 64 bits y utiliza un broker de
32 bits para cargar las DLL J2534 de proveedores que no ofrecen una DLL x64.

> Estado: versión de integración para pruebas controladas. Motor y transmisión
> son los primeros objetivos. El soporte moderno de ABS/VDC y sensor de ángulo
> de dirección todavía no está implementado.

La transmisión 5EAT dispone ahora de parámetros y bloques DTC seleccionados por
coincidencia exacta del ROM ID de la TCU contra el logger incluido. Esta mejora
es exclusivamente de lectura y no modifica todavía ABS/VDC.

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

Para el Outback 2010 3.6R, abre primero **Transmission → Information**. Si el ROM
ID está cubierto, la descripción mostrará **5EAT logger ROM** seguido por el ID.
Guarda esa pantalla, lee DTC actuales/memorizados y prueba datos en vivo. Prioriza
Battery Voltage, ATF Temperature, ATF Temperature 2 si aparece, Gear Position,
Turbine Revolution Speed y corrientes/presiones de solenoides. Después repite en
**Engine**. No borres códigos y no uses el diálogo ABS/VDC actual: corresponde
al protocolo SSM1 antiguo y no es válido para ese vehículo.

La ROM de campo B1AB217100 usa un sondeo específico y aparecerá como
**5EAT DTC probe (unverified) 5eat-dtc-field-probe-v3**. La misma línea muestra
la cantidad de flag bytes SSM2 y los valores `f28`, `f29`, `f39` y `f50` que
anuncian bancos DTC. Conserva los datos genéricos y añade 122
parámetros “5EAT DTC probe ... raw” para registrar, en decimal, cada byte actual
y confirmado. El sondeo v3 recorre los 61 pares del banco DTC SSM2 estándar
(`0x08E–0x0AD`, `0x0F0–0x0F3`, `0x123–0x12A`, `0x150–0x154`,
`0x160–0x164` y `0x174–0x17A`, junto con sus memorias). Sólo consulta
direcciones Select Monitor menores que 0x200; ejecútalo siempre con
`--read-only`.

Para el Outback 2010, deja desmarcada en Preferencias la opción **Preferir
ISO-14230 (K-Line) sobre ISO-15765 (CAN)**. Al reconectar, la barra inferior debe
indicar `SSM2 / ISO-15765` y `500 Kbaud`; si indica `SSM2 / ISO-14230` y `4800
baud`, FreeSSM cayó a K-Line porque CAN no respondió o la preferencia sigue
activa. No borres memoria para hacer esta prueba.

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
- Logger 5EAT con 26 ROM IDs, parámetros específicos y 12 pares de bloques DTC.
- Selección exacta por los cinco bytes de ROM ID, sin inferir direcciones.
- Sondeo de campo read-only, limitado exactamente a la ROM B1AB217100, con los
  122 bytes DTC crudos estándar visibles y decodificación provisional.
- Consultas TCU por CAN limitadas a una dirección SSM por trama, igual que la
  ruta de compatibilidad de RomRaider; K-Line y otros módulos conservan el
  agrupamiento anterior.
- Interfaz seleccionable en español, inglés, alemán y turco.
- Paquete portable reproducible con Qt 5.15.2, runtimes MinGW, plugins y hashes.

## Informe de estado del vehículo

El botón **Informe del vehículo** consulta motor y transmisión SSM2 en modo de
solo lectura y permite guardar el resultado en JSON y PDF. El VIN se incluye
cuando la ECU lo anuncia y FreeSSM puede leerlo; el kilometraje del tablero se
ingresa manualmente. El informe identifica módulos no consultados y fallos de
comunicación. La lectura conjunta usa direcciones SSM2 específicas y puede
marcar como fallido un módulo que responda por otra dirección o protocolo.

Esta función está probada con ECU simuladas; aún necesita validación con
OpenPort y vehículos reales. No confirma el kilometraje electrónico ni cubre
ABS/VDC, airbags, tablero o el historial completo de fallas.

La adaptación para macOS es **experimental**: compiló en macOS 15 Intel mediante
[GitHub Actions](https://github.com/alofredo-alo/FreeSSM/actions/runs/41814022624)
y el backend serial pasó pruebas con pseudoterminales en Linux. Apple Silicon,
los adaptadores y los vehículos aún no están validados. Consulta
[README.macOS-experimental.md](README.macOS-experimental.md).

## Agradecimientos y procedencia

Este fork se apoya en el trabajo y la documentación de la comunidad. Gracias a:

- [Comer352L y colaboradores de FreeSSM](https://github.com/Comer352L/FreeSSM),
  por el proyecto original, los protocolos y las definiciones de diagnóstico.
- [TomFLV](https://github.com/TomFLV/FreeSSM/commit/e15979e), por la base del
  broker J2534 para FreeSSM x64, y por la
  [definición de logger 5EAT](https://github.com/TomFLV/5eat-tcu-reverse-engineering)
  incluida en este fork bajo MIT; consulta su [licencia](definitions/5eat_tcu_logger.LICENSE.txt)
  y [procedencia y hash](definitions/README-5EAT-LOGGER.md).
- [Jacob Seman / Jbsco](https://github.com/Jbsco/FreeSSM), por el
  [simulador SSM2](https://github.com/Jbsco/FreeSSM/commit/7c9bfca) y la
  [corrección de respuestas sin eco](https://github.com/Jbsco/FreeSSM/commit/4b452a468ef15a655e9a2c557ceb6fea4bd86d7f).
  El simulador adaptado conserva su aviso GPL y funciona sin `sudo`.
- [Marcin Koziej](https://github.com/marcinkoziej/FreeSSM/commit/0392f3a),
  por el backend serial POSIX que sirve de base al port macOS experimental.
- [RomRaider](https://www.romraider.com/) y sus colaboradores, por el trabajo
  comunitario en definiciones y diagnóstico Subaru, y por la referencia para
  la compatibilidad de lecturas TCU. [SubieScope](https://github.com/marvin-socialista/subiescope)
  fue una referencia de diseño para el flujo de diagnóstico y el informe.
- [Lee Thomason y colaboradores de TinyXML-2](https://github.com/leethomason/tinyxml2),
  cuya biblioteca se distribuye en `src/tinyxml2` con sus avisos originales; y
  [rudolfovich](https://gist.github.com/rudolfovich/f250900f1a833e715260a66c87369d15),
  por la referencia del helper CSV indicada en `src/csvfile.h`.

Los avisos de autoría y licencias de cada archivo siguen siendo aplicables.
Las herramientas y marcas Subaru, Tactrix y demás titulares pertenecen a sus
respectivos propietarios; este proyecto no cuenta con su respaldo oficial.

## Documentación

- [Bitácora técnica completa del trabajo realizado](RESUMEN-TRABAJO-CHAT.md)
- [Guía completa J2534 para Windows en español](README.J2534-Windows.es.md)
- [J2534 Windows guide in English](README.J2534-Windows.md)
- [README original de FreeSSM](README.txt)
- [Formato y pruebas del informe de estado](tests/README-VehicleReport.md)
- [Soporte macOS experimental](README.macOS-experimental.md)

## Compilación

El workflow reproducible está en
[`windows-j2534.yml`](https://github.com/alofredo-alo/FreeSSM/actions/workflows/windows-j2534.yml).
Para una compilación local y detalles del broker consulta la guía completa.

## Licencia y responsabilidad

FreeSSM se distribuye bajo GNU GPL v3, sin garantía. No es un producto de Subaru
ni está respaldado por Subaru. El diagnóstico y cualquier modificación del
vehículo se realizan bajo responsabilidad del usuario. Conserva todos los DTC y
datos base antes de desconectar componentes o borrar memoria.

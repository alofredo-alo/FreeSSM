# FreeSSM 1.3.0-beta.1

Primera beta pública para pruebas. El paquete Windows se distribuye con su
huella SHA256. Las pruebas con hardware y vehículos siguen pendientes de
validación específica.

## Qué ofrece

- Interfaz FreeSSM para diagnóstico Subaru y adaptadores compatibles, incluido
  el trabajo J2534 de este fork y broker para DLL de proveedor de 32 bits.
- Selector inicial: demo, solo lectura recomendado y diagnóstico completo con
  confirmación. Las funciones de escritura conservan el comportamiento original
  cuando se elige diagnóstico completo.
- Demo en la interfaz real, con motor y transmisión SSM2 simulados, códigos y
  mediciones ficticias; no necesita adaptador y marca los informes como simulados.
- Informe de estado de motor y transmisión con ROM ID, VIN cuando esté disponible,
  cobertura y códigos disponibles; exportación JSON y PDF.
- Selección de definiciones de transmisión 5EAT por ROM ID y advertencia en el
  informe cuando se aplica la definición experimental de sondeo.

## Límites conocidos

- Las pruebas automatizadas cubren Linux y el arranque del paquete Windows en
  GitHub Actions; aún falta probarlo en un Windows de usuario y con hardware.
  La compatibilidad con adaptador y vehículo depende de pruebas específicas.
- El informe no verifica kilometraje electrónico ni alteración del odómetro; el
  kilometraje del tablero es ingresado por el usuario. Los códigos disponibles
  no son un historial completo y una lectura vacía no garantiza ausencia de fallas.
- ABS/VDC moderno, airbag, tablero y otros módulos no están cubiertos por el
  informe conjunto. En demo solo se simulan motor y transmisión.
- El sondeo DTC para ROM B1AB217100 es experimental y no está verificado; no
  interpretar la ausencia de advertencia en otras definiciones como validación.
- Los decodificadores OBD readiness y freeze frame son prototipos probados con
  datos sintéticos; aún no realizan consultas ni se presentan al usuario.
- El selector de modo y los textos nuevos de la demo están en español; los
  catálogos inglés, alemán y turco conservan traducciones pendientes.
- macOS permanece experimental; la compatibilidad efectiva con dispositivos y
  arquitecturas debe comprobarse antes de anunciar un paquete soportado.


Para detalles y procedimiento de validación consulta `VALIDACION-BETA-WINDOWS.md`.

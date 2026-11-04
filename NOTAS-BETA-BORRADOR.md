# FreeSSM 1.3.0-beta.1 — borrador de notas

Este documento prepara una posible primera publicación beta. La versión y el
contenido definitivos deben corresponder al commit y a los paquetes realmente
verificados. **Aún no se ha publicado una beta ni validado este candidato en
Windows limpio.**

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

- La compilación y pruebas Linux cubren simulación; este candidato concreto debe
  compilarse y probarse en Windows y macOS. La compatibilidad del adaptador y
  vehículo reales depende de pruebas específicas de hardware, no de la demo.
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

## Antes de publicar

Verificar que el commit seleccionado coincide con `BUILD-INFO.txt`, repetir la
compilación del paquete, contrastar hashes y completar la guía
`VALIDACION-BETA-WINDOWS.md`. Registrar los resultados reales y marcar cualquier
función o plataforma pendiente. Publicar como pre-release solo cuando se decida
expresamente hacerlo.

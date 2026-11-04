# Candidato beta: validación y pendientes

Fecha: 2026-11-04. Versión identificada en la aplicación: `1.3.0-beta.1`.
La primera beta es una herramienta de diagnóstico en desarrollo; el informe de
estado es una función de ella. Las pruebas pendientes se mantienen a continuación.

## Estado y límites

- Selector obligatorio sin modo preseleccionado: demo, solo lectura recomendado
  y diagnóstico completo con confirmación. `--read-only`, `--enable-write` y
  `--demo` son excluyentes; diagnóstico completo siempre exige confirmación.
- Demo en la interfaz real de FreeSSM con un adaptador SSM2 en memoria para
  motor y transmisión; el informe JSON/PDF se identifica como simulado. No abre
  hardware; Linux probado, Windows/macOS pendientes.
- Informe conjunto motor/transmisión SSM2: VIN cuando está disponible, ROM,
  cobertura y DTC, exportación JSON/PDF. Los kilómetros del tablero son declarados,
  no una lectura de la ECU ni una comprobación de manipulación.
- Las definiciones DTC del sondeo experimental 5EAT se advierten en el informe.
  Que no aparezca la advertencia no significa que la definición esté validada.
- Los decodificadores OBD readiness y freeze frame solo procesan respuestas
  sintéticas; no solicitan datos al vehículo ni están integrados a la interfaz.
- Las pruebas con simulador y compilación Linux no validan J2534, el vehículo ni
  el paquete Windows de estos cambios. macOS y Windows deben reconstruirse y
  validarse para el commit exacto que se elija como candidato.

## Validación antes de publicar

1. Congelar el código elegido y registrar commit, versión y hashes de artefactos;
   CI Windows y macOS deben terminar bien para ese mismo commit.
2. En Windows limpio, extraer el artefacto completo; comprobar inicio, idioma,
   selector, demo, `[READ-ONLY]`, ayuda, cierre y exportación JSON/PDF demo. Revisar las
   DLL incluidas y `BUILD-INFO.txt`. Registrar versión de Windows y resultados.
3. Con permiso específico, probar el driver y OpenPort sin vehículo mediante
   `FreeSSM.exe --j2534-probe`, registrando únicamente resultado y versiones.
4. Con permiso específico y protocolo acordado, probar solo lectura en vehículos
   representativos, guardar resultados anonimizados y comparar DTC y VIN con una
   referencia cuando esté disponible. La ausencia de DTC no valida cobertura.
5. Revisar notas de versión, matriz real de soporte, dependencias, licencias y
   limitaciones; seleccionar los pendientes adicionales y repetir pruebas si se
   cambia el código. Solo entonces decidir si se publica una beta.

## Pendientes para seleccionar

Estimaciones orientativas de trabajo de ingeniería, sin contar acceso a hardware;
las dependencias y pruebas reales pueden aumentar el esfuerzo.

| Pendiente | Esfuerzo | Valor y condición |
| --- | --- | --- |
| Mejorar mensajes de conexión, módulos ausentes y guía de diagnóstico | S: 0,5–1 día | Facilita uso y soporte; probar con simulador. |
| Añadir identificación de versión y copia de datos anonimizados al soporte | S: 0,5–1 día | Facilita incidencias; revisar datos personales y rutas. |
| Automatizar empaquetado Linux y comprobar instalación limpia | M: 1–3 días | Ampliar distribución; definir formato y compatibilidad. |
| Integrar readiness OBD al informe | M: 2–4 días | Ya existe decodificador; falta transporte, interfaz y validación. |
| Integrar freeze frame OBD | M: 2–5 días | Ya existe decodificador; falta transporte, cobertura de PID y validación. |
| Exportar selección y registro de datos en vivo de forma más simple | M: 2–5 días | Requiere revisión del flujo existente y pruebas prolongadas. |
| Lectura electrónica de kilometraje por módulo | XL: investigación | No asumir que existe o es comparable; requiere evidencia por ECU y versión. |
| ABS/VDC moderno y sensor de ángulo | XL: investigación | Requiere protocolo, definiciones y validación específica; no prometer cobertura. |
| Soporte macOS distribuible | L: 3–8 días o más | Hay compilación Intel previa, faltan paquete, firma y pruebas de adaptador. |

S = menos de 1 día aproximadamente; M = varios días; L/XL dependen de evidencia y
hardware. La validación de seguridad y compatibilidad no se omite por ser costosa.

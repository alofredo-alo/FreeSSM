# Validación del candidato en Windows limpio

Esta guía prepara la verificación; no afirma que se haya ejecutado. Usar el
artefacto construido desde el commit elegido, no otro artefacto anterior. No
conectar vehículo ni interfaz durante esta secuencia.

1. Registrar versión de Windows, nombre de artefacto, commit y fecha de CI.
2. Extraer todo el ZIP a una carpeta local sin instalar ni ejecutar como admin.
3. En PowerShell desde esa carpeta:

```powershell
Get-Content .\BUILD-INFO.txt
Get-FileHash -Algorithm SHA256 .\FreeSSM.exe, .\j2534_broker.exe, .\definitions\5eat_tcu_logger.xml
$prueba = Start-Process -FilePath .\FreeSSM.exe `
  -ArgumentList '--read-only','--enable-write' -Wait -PassThru
$prueba.ExitCode
```

Comparar hashes con `BUILD-INFO.txt`; la última ejecución debe rechazar opciones
incompatibles y terminar con código distinto de cero. Iniciar `FreeSSM.exe` y
comprobar que aparece el selector, cancelar y verificar que termina. Reiniciar y
seleccionar demo: confirmar que se abre la ventana habitual de FreeSSM con
DATOS SIMULADOS, entrar en Motor y Transmisión, consultar DTC y mediciones, y
exportar el informe habitual en JSON y PDF; verificar que ambos indiquen que son
simulados. Comprobar que Preferencias y los módulos sin simulación estén desactivados. Reiniciar, seleccionar solo lectura y
verificar `[READ-ONLY]`. Comprobar que diagnóstico completo presenta advertencia
y que cancelarla no abre la aplicación. Revisar cambio de idioma en la interfaz
principal, apertura y cierre sin errores. Documentar fallos
sin enviar VIN ni identificadores reales. La prueba del adaptador y cualquier
comunicación con un vehículo requieren autorización y protocolo separados.

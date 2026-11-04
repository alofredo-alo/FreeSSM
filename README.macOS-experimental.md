# FreeSSM en macOS: soporte experimental

El port serial POSIX deriva de `marcinkoziej/FreeSSM`, commit
`0392f3aa8`, GPL-3.0 o posterior. Se adaptó al fork actual y se modificó
la enumeración de `/dev/cu.*` para no abrir dispositivos al listarlos.
El backend J2534 utiliza el cargador POSIX del fork actual y busca las
bibliotecas configuradas junto al ejecutable del bundle.

**Estado:** la [compilación en macOS 15 Intel](https://github.com/alofredo-alo/FreeSSM/actions/runs/41814022624)
generó el bundle y sus recursos, pero no se ha probado la aplicación con cables
ni vehículos. El código serial también se probó en Linux con un stub temporal
para el encabezado IOKit y una ECU simulada, a 4800 baud, con y sin eco y desde
un hilo de trabajo. Esto no valida la comunicación física, `IOSSIOSPEED`, Apple
Silicon, J2534 ni drivers. Las bibliotecas J2534 de Windows no son bibliotecas
nativas de macOS.

## Compilación de validación en un Mac

Requisitos: Xcode Command Line Tools y Qt 5.15 con `qmake` y `lrelease`
disponibles en el PATH. Desde un terminal, reemplaza la ruta por el checkout:

```bash
source_dir="/ruta/al/checkout/FreeSSM"
build_dir="$HOME/freessm-macos-build"
mkdir -p "$build_dir"
cd "$build_dir"
for language in en de tr es; do
    lrelease "$source_dir/FreeSSM_${language}.ts" -qm "FreeSSM_${language}.qm" || exit 1
done
qmake "$source_dir/FreeSSM.pro" 'CONFIG+=release' || exit 1
make -j2
```

Qmake debe generar `FreeSSM.app` con traducciones, fuentes y `definitions/*.xml`
en `Contents/MacOS`. El script de prueba pasó en un runner Intel de macOS 15;
registrar versión de macOS, arquitectura, Qt y resultado en otras plataformas
antes de afirmar soporte para ellas. No conectar un vehículo
para esta comprobación de compilación.

La comprobación parcial disponible en Linux es:

```bash
./tests/run_mac_serial_compat.sh
```

No publica, instala, firma ni notariza la aplicación. Las pruebas de hardware
requieren una planificación y autorización separadas.

También puede usarse `./tests/run_macos_build_smoke.sh`: compila en un
directorio temporal y comprueba que el ejecutable y los recursos principales
estén dentro del bundle. El script pasó en GitHub Actions sobre macOS 15 Intel, sin hardware.

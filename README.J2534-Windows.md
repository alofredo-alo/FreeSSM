# FreeSSM J2534 on 64-bit Windows

[Español](README.J2534-Windows.es.md) · [Main README](README.md)

This fork runs FreeSSM as a 64-bit Qt application while loading 32-bit J2534
04.04 vendor libraries, including Tactrix OpenPort 2.0, in a separate 32-bit
broker. The broker uses anonymous pipes: it does not listen on a TCP/UDP port.

`FreeSSM.exe` and `j2534_broker.exe` must remain in the same directory. Install
the vendor's current OpenPort 2.0 driver; do not copy `op20pt32.dll` into the
FreeSSM directory or register it manually.

## Reproducible build

The GitHub workflow `.github/workflows/windows-j2534.yml` builds a complete
`FreeSSM-J2534-x64` artifact with Qt 5.15.2/MinGW 8.1 x64. It builds the broker
with the Visual Studio x86 compiler and verifies that its PE machine is i386.

For a local build, build FreeSSM from a Qt 5.15.2 MinGW 8.1 x64 shell:

```powershell
qmake.exe FreeSSM.pro "CONFIG+=release"
mingw32-make.exe -j2 release
```

Then build the broker from PowerShell. Visual Studio 2022 Build Tools with the
"Desktop development with C++" x86/x64 workload is required:

```powershell
./scripts/build-j2534-broker.ps1 -OutputPath ./j2534_broker.exe
```

An i686 MinGW compiler is an alternative:

```powershell
i686-w64-mingw32-g++.exe -std=c++11 -O2 -static `
  -o j2534_broker.exe src/windows/j2534_broker.cpp -ladvapi32
```

## Safe test order

1. Connect only the OpenPort 2.0 USB interface. Do not connect it to a vehicle.
2. From PowerShell, enumerate the registered DLL and test `PassThruOpen`, version
   retrieval and `PassThruClose`:

```powershell
Start-Process -FilePath ./FreeSSM.exe -ArgumentList '--j2534-probe' `
  -NoNewWindow -Wait
```

This probe does not call `PassThruConnect`, create a vehicle bus channel, transmit
a frame, clear memory, run an actuator or change an adjustment.

If no driver appears, inspect both registry views without modifying them:

```powershell
reg.exe query "HKLM\SOFTWARE\PassThruSupport.04.04" /s /reg:32
reg.exe query "HKLM\SOFTWARE\PassThruSupport.04.04" /s /reg:64
```

3. Connect to the vehicle with ignition ON and engine OFF. In Preferences, select
   the detected J2534 interface and run the interface test. It sends only Subaru
   identification/read requests; it does not clear or adjust a control unit.
4. Close FreeSSM and relaunch the diagnostic session in enforced read-only mode:

```powershell
./FreeSSM.exe --read-only
```

   Confirm that `[READ-ONLY]` appears in the window title. In this mode FreeSSM
   blocks protocol writes and disables Adjustments, System tests, Clear memory and
   Clear memory 2. It also avoids the normal connect-time actuator-stop routine.
5. On the 2010 Outback 3.6R, open Transmission first and save the complete TCU
   identification and all current/history DTCs. Then repeat for Engine. Do not use
   a normal writable session during baseline capture.

## Vehicle scope for the first test

| Vehicle | First transport | Current target |
| --- | --- | --- |
| Outback 2007 2.5 | SSM2 over K-line, 4800 baud | Engine and automatic transmission |
| Outback 2010 3.6R | SSM2 over ISO15765 CAN, 500 kbit/s | Engine and 5EAT TCU; highest priority |
| Forester 2015 2.0 | Later SSM3-era coverage | Interface detection first; module coverage is not yet guaranteed |

The existing modern CAN path addresses Engine and Transmission. The 2010
steering-angle value and its DTCs belong to the VDC/ABS domain; FreeSSM's current
ABS/VDC dialog is the older SSM1 implementation and must not be treated as valid
for that car. VDC support will be added only after identifying the exact module
address/services from a read-only capture or trustworthy service data. Guessing
CAN diagnostic addresses on a live vehicle is explicitly out of scope.

## AT OIL TEMP baseline

A flashing AT OIL TEMP lamp immediately after engine start indicates a detected
transmission-control fault, not merely high fluid temperature. Preserve codes and
freeze-frame/current data before disconnecting components or clearing memory.
Capture battery voltage, whether the lamp flashes from cold, every TCU/VDC/ECM
DTC including status, and whether the steering-angle value changes smoothly and
returns near zero with the wheels centered.

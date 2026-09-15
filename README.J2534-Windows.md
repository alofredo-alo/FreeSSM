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
5. On the 2010 Outback 3.6R, open Transmission first. Under Information, verify
   that the description includes **5EAT logger ROM** followed by the displayed
   TCU ROM ID. Save the identification, all current/memorized DTCs and initial
   live data. Prioritize battery voltage, both available ATF temperatures, gear
   position, turbine speed and solenoid currents/pressures. Then repeat for
   Engine. Do not use a writable session during baseline capture.

The marker means that FreeSSM selected an exact five-byte ROM ID from the bundled
5EAT logger. The matching definition supplies the live parameters and 12
current/confirmed DTC byte pairs. Experimental direct-RAM lock-up parameters and
switches are excluded, and this integration adds no write operation.

ROM B1AB217100 is handled separately and must display
**5EAT DTC probe (unverified) 5eat-dtc-field-probe-v3 ROM B1AB217100**. The
same line reports the SSM2 flag-byte count and DTC capability bytes `f28`,
`f29`, `f39` and `f50`. In a --read-only session,
capture both DTC tables, then add all 122 live-data parameters whose names start
with “5EAT DTC probe”. They expose the current/confirmed bytes as decimal values
and include each hexadecimal address in the name. A nonzero undefined bit is
shown as “???” with its address and bit; do not treat it as a validated code
until the field capture is reviewed. Probe v3 covers all 61 standard SSM2 DTC
pairs: current ranges `0x08E-0x0AD`, `0x0F0-0x0F3`, `0x123-0x12A`,
`0x150-0x154`, `0x160-0x164` and `0x174-0x17A`, plus their confirmed-memory
partners.

On the 2010 Outback, leave **Prefer ISO-14230 (K-Line) over ISO-15765 (CAN)**
unchecked in Preferences, close any connected Transmission window and reconnect.
The expected status bar is `SSM2 / ISO-15765` at `500 Kbaud`. If it ends on
`SSM2 / ISO-14230` at `4800 baud`, either that preference is still enabled or
the CAN attempt failed and FreeSSM used its K-Line fallback. Save that status
bar and do not clear memory or run actuator tests. This build sends one SSM
address per ISO-15765 request to the TCU, matching RomRaider's compatibility
workaround for TCUs that reject long CAN queries.

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

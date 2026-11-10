# Project workflow

## Local firmware validation

Use the existing project Python environment and the companion PlatformIO target
for routine firmware validation in this Windows checkout:

```powershell
$orbPython = Join-Path $PWD '.pio\build-venv\Scripts\python.exe'
& $orbPython -m platformio run -e esp32-s3-amoled-175-companion
if ($LASTEXITCODE -ne 0) { throw 'Orb build failed. Do not flash.' }
```

The project environment currently uses Python 3.12. Do not substitute the global
`platformio.exe`, the global PlatformIO Python 3.11 environment, or the standalone
`esp32-s3-amoled-175` target unless the task specifically needs them. PlatformIO
keeps separate dependency databases by Python major/minor version and separate
outputs by target. Matching both the interpreter and target lets the user and
agent reuse each other's incremental builds; this was verified on 2026-11-09.

Preserve the existing environment and build outputs. Do not clean or recreate
them for routine validation. Report the target, result, and whether compilation
was needed. A successful build does not authorize flashing the device.

## Device and manual procedure

The current device serial port is **COM6**. Use that port for device commands
unless the user specifies another port or enumeration shows it has changed.
When preserving a failed boot, read `http://theorb.local/diag` and `/health`
first; updated firmware also has a read-only `/time` snapshot. Opening
native USB serial on this device has triggered a USB reset even with DTR/RTS
held inactive, so do not promise that attaching a serial probe preserves state.
Follow [the manual build, flash, and release procedure](docs/MANUAL_BUILD_FLASH_RELEASE.md)
when performing those tasks.

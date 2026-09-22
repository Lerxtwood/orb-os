param(
    [string]$IdfPath = "C:\Users\chris\esp\esp-idf-v5.5.4",
    [string]$IdfPython = "C:\Users\chris\.espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe"
)
$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path "$PSScriptRoot\..\..").Path
$env:PATH = "$(Split-Path $IdfPython);$env:PATH"
. "$IdfPath\export.ps1"
if ($LASTEXITCODE -ne 0) { throw "ESP-IDF environment setup failed" }
& $IdfPython "$IdfPath\tools\idf.py" -C "$repoRoot\.pio\companion\PrintSphere" -B "$repoRoot\.pio\companion\ps-build" -D PRINTSPHERE_HW_VARIANT=amoled_1_75 -D PRINTSPHERE_RELEASE_VERSION=2.0.22-orbtest build
if ($LASTEXITCODE -ne 0) { throw "PrintSphere build failed" }

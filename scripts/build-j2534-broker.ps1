[CmdletBinding()]
param(
    [string]$OutputPath = (Join-Path $PSScriptRoot "..\j2534_broker.exe")
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$source = Join-Path $repoRoot "src\windows\j2534_broker.cpp"
$output = [IO.Path]::GetFullPath($OutputPath)
$outputDir = Split-Path -Parent $output
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe was not found. Install Visual Studio Build Tools with the C++ x86/x64 workload."
}

$vsRoot = (& $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath | Select-Object -First 1)
if (-not $vsRoot) {
    throw "No Visual Studio C++ x86 toolchain was found."
}

$vcvars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvarsall.bat"
if (-not (Test-Path $vcvars)) {
    throw "vcvarsall.bat was not found under $vsRoot."
}

# Import the x86 developer environment into this PowerShell process.
$environment = & $env:ComSpec /d /c "call `"$vcvars`" x86 >nul && set"
if ($LASTEXITCODE -ne 0) {
    throw "vcvarsall.bat failed for the x86 target."
}
foreach ($line in $environment) {
    $separator = $line.IndexOf('=')
    if ($separator -gt 0) {
        [Environment]::SetEnvironmentVariable(
            $line.Substring(0, $separator),
            $line.Substring($separator + 1),
            "Process"
        )
    }
}

$buildDir = Join-Path ([IO.Path]::GetTempPath()) `
    ("freessm-j2534-broker-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $buildDir | Out-Null
try {
    Push-Location $buildDir
    try {
        & cl.exe /nologo /EHsc /O2 /MT /std:c++14 /DWIN32_LEAN_AND_MEAN `
            "/Fo:$buildDir\j2534_broker.obj" "/Fe:$output" $source Advapi32.lib
        if ($LASTEXITCODE -ne 0) {
            throw "The x86 J2534 broker build failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}
finally {
    Remove-Item -LiteralPath $buildDir -Recurse -Force -ErrorAction SilentlyContinue
}

$image = [IO.File]::ReadAllBytes($output)
if ($image.Length -lt 64) {
    throw "The generated broker is not a valid PE image."
}
$peOffset = [BitConverter]::ToInt32($image, 0x3c)
if ($peOffset -lt 0 -or ($peOffset + 6) -gt $image.Length) {
    throw "The generated broker has an invalid PE header offset."
}
$machine = [BitConverter]::ToUInt16($image, $peOffset + 4)
if ($machine -ne 0x014c) {
    throw ("The generated broker is not x86 (PE machine 0x{0:X4})." -f $machine)
}

$hash = (Get-FileHash -Algorithm SHA256 $output).Hash
Write-Host "Built x86 J2534 broker: $output"
Write-Host "SHA256: $hash"

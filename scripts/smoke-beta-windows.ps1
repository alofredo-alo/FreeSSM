# SPDX-License-Identifier: GPL-3.0-or-later
# Smoke-test the assembled Windows package in the CI desktop, without hardware.
param([Parameter(Mandatory = $true)][string]$PackagePath)
$ErrorActionPreference = 'Stop'
$package = (Resolve-Path $PackagePath).Path
$app = Join-Path $package 'FreeSSM.exe'
if (-not (Test-Path $app)) { throw 'Missing FreeSSM.exe' }

function Assert-Window([string[]]$Arguments, [string]$ExpectedTitle) {
    $options = @{ FilePath = $app; WorkingDirectory = $package; PassThru = $true }
    if ($Arguments.Count -gt 0) { $options.ArgumentList = $Arguments }
    $process = Start-Process @options
    try {
        $deadline = (Get-Date).AddSeconds(30)
        do {
            Start-Sleep -Milliseconds 250
            $process.Refresh()
            if ($process.HasExited) { throw "App exited early ($($process.ExitCode)); expected window: $ExpectedTitle" }
            $title = $process.MainWindowTitle
        } while (-not $title -and (Get-Date) -lt $deadline)
        if (-not $title.Contains($ExpectedTitle)) {
            throw "Unexpected window title '$title'; expected '$ExpectedTitle'"
        }
        Write-Host "Window OK: $title"
        if (-not $process.CloseMainWindow()) { throw "Cannot close window: $title" }
        if (-not $process.WaitForExit(10000)) { throw "Process did not exit after closing: $title" }
        if ($process.ExitCode -ne 0) { throw "Closing $title returned $($process.ExitCode)" }
    } finally {
        $process.Refresh()
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    }
}

Assert-Window -Arguments @() -ExpectedTitle 'Seleccionar modo'
Assert-Window -Arguments @('--demo') -ExpectedTitle 'DATOS SIMULADOS'
Assert-Window -Arguments @('--read-only') -ExpectedTitle 'READ-ONLY'
Assert-Window -Arguments @('--enable-write') -ExpectedTitle 'Diagnóstico completo'
$invalid = Start-Process -FilePath $app -WorkingDirectory $package -ArgumentList '--read-only','--enable-write' -Wait -PassThru
if ($invalid.ExitCode -eq 0) { throw 'Conflicting security modes were accepted' }
Write-Host 'Packaged Windows startup smoke OK; no diagnostic hardware used.'

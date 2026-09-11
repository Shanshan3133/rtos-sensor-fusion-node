[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build\arm-check'
$cubeRoot = 'C:\ST'

Write-Host '[1/3] Running Python telemetry tests'
Push-Location $projectRoot
try {
    python -m unittest discover -s tests -v
    python tools\spectrum_monitor.py --self-test
    python tools\fft_precision_report.py --check

    Write-Host '[2/3] Locating STM32 ARM GCC'
    $compiler = Get-ChildItem -LiteralPath $cubeRoot -Recurse `
        -Filter arm-none-eabi-gcc.exe -ErrorAction Stop |
        Select-Object -First 1 -ExpandProperty FullName
    if (-not $compiler) {
        throw 'arm-none-eabi-gcc.exe was not found below C:\ST.'
    }
    Write-Host "Compiler: $compiler"

    Write-Host '[3/3] Compiling portable C modules with warnings as errors'
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
    $flags = @(
        '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Iinclude', '-mcpu=cortex-m4', '-mthumb',
        '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard', '-c'
    )
    $sources = @(
        'core\health_monitor.c',
        'core\spectrum.c',
        'core\telemetry.c',
        'tests\test_core.c'
    )
    foreach ($source in $sources) {
        $objectName = ([IO.Path]::GetFileNameWithoutExtension($source)) + '.o'
        & $compiler @flags $source '-o' (Join-Path $buildDir $objectName)
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $source" }
    }
    Write-Host 'All checks passed. Note: ARM objects were compiled, not executed.'
}
finally {
    Pop-Location
}

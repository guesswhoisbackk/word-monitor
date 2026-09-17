param(
    [ValidateSet("all", "cyd-st7789", "cyd-ili9341")]
    [string]$Environment = "all"
)

$ErrorActionPreference = "Stop"
$env:PYTHONUTF8 = "1"
$env:PYTHONIOENCODING = "utf-8"

$projectRoot = Split-Path -Parent $PSScriptRoot
# Prefer a project-local virtual environment; fall back to a global pio.
$pioLocal = Join-Path $projectRoot ".venv\Scripts\pio.exe"
$pythonLocal = Join-Path $projectRoot ".venv\Scripts\python.exe"
$pio = if (Test-Path -LiteralPath $pioLocal) { $pioLocal } else { "pio" }
$python = if (Test-Path -LiteralPath $pythonLocal) { $pythonLocal } else { "python" }
$platformIoHome = Join-Path $env:USERPROFILE ".platformio"
$esptool = Join-Path $platformIoHome "packages\tool-esptoolpy\esptool.py"
$bootApp = Join-Path $platformIoHome "packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"
$outputDir = Join-Path $projectRoot "build"
$configHeader = Join-Path $projectRoot "include\app_config.hpp"

foreach ($required in @($esptool, $bootApp)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required PlatformIO tool not found: $required (run one build via 'pio run' first)"
    }
}
if ($pio -eq "pio" -and -not (Get-Command pio -ErrorAction SilentlyContinue)) {
    throw "pio not found. Create .venv (see docs/HANDOFF.md) or install PlatformIO on PATH."
}

$versionText = Get-Content -Raw -LiteralPath $configHeader
$versionMatch = [regex]::Match($versionText, 'kVersion\[\]\s*=\s*"([^"]+)"')
if (-not $versionMatch.Success) {
    throw "Could not read kVersion from $configHeader"
}
$version = $versionMatch.Groups[1].Value

$profiles = if ($Environment -eq "all") {
    @("cyd-st7789", "cyd-ili9341")
} else {
    @($Environment)
}

New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

foreach ($profile in $profiles) {
    Write-Host "[$profile] Building source"
    & $pio run --project-dir $projectRoot -e $profile
    if ($LASTEXITCODE -ne 0) {
        throw "$profile build failed."
    }

    $profileDir = Join-Path $projectRoot ".pio\build\$profile"
    $mergedBin = Join-Path $outputDir "WordMonitor-v$version-$profile-merged.bin"
    $otaBin = Join-Path $outputDir "WordMonitor-v$version-$profile-firmware.bin"

    Write-Host "[$profile] Creating merged BIN for address 0x0"
    & $python $esptool --chip esp32 merge_bin `
        -o $mergedBin `
        --flash_mode dio `
        --flash_freq 40m `
        --flash_size 4MB `
        0x1000 (Join-Path $profileDir "bootloader.bin") `
        0x8000 (Join-Path $profileDir "partitions.bin") `
        0xE000 $bootApp `
        0x10000 (Join-Path $profileDir "firmware.bin")
    if ($LASTEXITCODE -ne 0) {
        throw "$profile merged BIN creation failed."
    }

    Copy-Item -LiteralPath (Join-Path $profileDir "firmware.bin") -Destination $otaBin -Force
}

Write-Host "Done: $outputDir"

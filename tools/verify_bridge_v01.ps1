<#
.SYNOPSIS
Builds and verifies Amber Bridge v0.1 in Debug and Release x64.
.DESCRIPTION
Omit ROM parameters for loader-only verification. Program and sound ROM slots
must each be consecutive from slot 1; trailing slots may be omitted.
.EXAMPLE
.\tools\verify_bridge_v01.ps1
.EXAMPLE
.\tools\verify_bridge_v01.ps1 -ProgramRom1 C:\roms\game1.bin -ProgramRom2 C:\roms\game2.bin -SoundRom1 C:\roms\sound.bin
.EXAMPLE
.\tools\verify_bridge_v01.ps1 -ProgramRom1 C:\roms\game1.bin -ProgramRom2 C:\roms\game2.bin -ProgramRom3 C:\roms\game3.bin -ProgramRom4 C:\roms\game4.bin
#>
[CmdletBinding()]
param(
    [string]$ProgramRom1,
    [string]$ProgramRom2,
    [string]$ProgramRom3,
    [string]$ProgramRom4,
    [string]$SoundRom1,
    [string]$SoundRom2,
    [string]$SoundRom3,
    [string]$SoundRom4
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$solution = Join-Path $root 'solutions\AmberOasisCores.sln'
$requiredJpm = @('GetDLLVersion','Initialise','LoadROM','LoadSoundROM','Reset','Run','Shutdown')

function Assert-Exports([string]$Dll, [string[]]$Expected, [switch]$AllowOthers) {
    if (-not (Test-Path $Dll)) { throw "Missing DLL: $Dll" }
    $dump = & dumpbin.exe /nologo /exports $Dll 2>&1
    if ($LASTEXITCODE -ne 0) { throw "dumpbin failed for $Dll`n$dump" }
    $names = @($dump | ForEach-Object { if ($_ -match '^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)(?:\s+=.*)?\s*$') { $Matches[1] } })
    foreach ($name in $Expected) { if ($names -notcontains $name) { throw "Missing export '$name' in $Dll" } }
    if (-not $AllowOthers) {
        $unexpected = @($names | Where-Object { $Expected -notcontains $_ })
        if ($unexpected.Count) { throw "Unexpected exports in ${Dll}: $($unexpected -join ', ')" }
    }
}

function Resolve-RomSlots([string]$Kind, [string[]]$Values) {
    $resolved = @()
    $gap = $false
    for ($index = 0; $index -lt $Values.Count; $index++) {
        $parameterName = "${Kind}Rom$($index + 1)"
        $value = $Values[$index]
        if ([string]::IsNullOrWhiteSpace($value)) {
            $gap = $true
            continue
        }
        if ($gap) { throw "$Kind ROM parameters must be supplied consecutively starting with ${Kind}Rom1; $parameterName was supplied after an omitted slot." }
        if (-not (Test-Path -LiteralPath $value -PathType Leaf)) { throw "$parameterName does not exist: $value" }
        $resolved += (Resolve-Path -LiteralPath $value).Path
    }
    return $resolved
}

$programRoms = @(Resolve-RomSlots 'Program' @($ProgramRom1,$ProgramRom2,$ProgramRom3,$ProgramRom4))
$soundRoms = @(Resolve-RomSlots 'Sound' @($SoundRom1,$SoundRom2,$SoundRom3,$SoundRom4))
$runtimeArgs = @()
foreach ($rom in $programRoms) { $runtimeArgs += '--program-rom'; $runtimeArgs += $rom }
foreach ($rom in $soundRoms) { $runtimeArgs += '--sound-rom'; $runtimeArgs += $rom }

foreach ($configuration in @('Debug','Release')) {
    & msbuild.exe $solution /m "/p:Configuration=$configuration" /p:Platform=x64
    if ($LASTEXITCODE -ne 0) { throw "$configuration x64 build failed" }
    $bin = Join-Path $root "build\bin\x64\$configuration"
    $bridge = Join-Path $bin 'AmberBridge.dll'
    $jpm = Join-Path $bin 'AmberOasis.JPMSystem6.dll'
    $diagnostic = Join-Path $bin 'AmberBridgeDiagnostic.exe'
    Assert-Exports $bridge @('AmberGetApi')
    Assert-Exports $jpm $requiredJpm -AllowOthers
    & $diagnostic
    if ($LASTEXITCODE -ne 0) { throw "$configuration loader diagnostic failed" }
    if ($runtimeArgs.Count -gt 0) {
        & $diagnostic @runtimeArgs
        if ($LASTEXITCODE -ne 0) { throw "$configuration runtime diagnostic failed" }
    }
}

Write-Host 'Amber Bridge v0.1 verification succeeded for Debug and Release x64.'

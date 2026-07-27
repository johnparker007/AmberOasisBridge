[CmdletBinding()]
param(
    [string]$ProgramRom1,
    [string]$ProgramRom2,
    [string]$ProgramRom3,
    [string]$ProgramRom4
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

$roms = @($ProgramRom1,$ProgramRom2,$ProgramRom3,$ProgramRom4)
$provided = @($roms | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
if ($provided.Count -ne 0 -and $provided.Count -ne 4) { throw 'Provide either no program ROMs or all four program ROM parameters.' }

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
    if ($provided.Count -eq 4) {
        & $diagnostic @roms
        if ($LASTEXITCODE -ne 0) { throw "$configuration runtime diagnostic failed" }
    }
}

Write-Host 'Amber Bridge v0.1 verification succeeded for Debug and Release x64.'

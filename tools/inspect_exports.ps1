[CmdletBinding()]
param([Parameter(Mandatory=$true,Position=0)][string[]]$DllPath)
$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
if (-not $dumpbin) { throw "dumpbin.exe was not found. Run from a Visual Studio Developer PowerShell." }
foreach ($path in $DllPath) {
  if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "DLL not found: $path" }
  Write-Output "===== $path ====="
  & $dumpbin.Source /nologo /exports (Resolve-Path -LiteralPath $path)
  if ($LASTEXITCODE -ne 0) { throw "dumpbin failed for $path with exit code $LASTEXITCODE" }
}

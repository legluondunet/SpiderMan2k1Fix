# Windows 11, native 32-bit CDB DirectInput8 trace.
# Does not change SpideyPC.exe, the fix DLL or INI.
param(
    [Parameter(Mandatory=$true)][string]$GameDir,
    [string]$CdbPath = ''
)
$ErrorActionPreference = 'Stop'
$exe = Join-Path $GameDir 'SpideyPC.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "SpideyPC.exe not found: $exe"
}
$scriptSource = Join-Path $PSScriptRoot 'trace_directinput8_windbg.js'
if (-not (Test-Path -LiteralPath $scriptSource -PathType Leaf)) {
    throw "Debugger script missing: $scriptSource"
}
if (-not $CdbPath) {
    $kits = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x86\cdb.exe",
        "${env:ProgramFiles(x86)}\Windows Kits\11\Debuggers\x86\cdb.exe"
    )
    $found = $kits | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if ($found) { $CdbPath = $found }
    else {
        $foundCommand = Get-Command cdb.exe -ErrorAction SilentlyContinue
        if ($foundCommand) { $CdbPath = $foundCommand.Source }
    }
}
if (-not $CdbPath -or -not (Test-Path -LiteralPath $CdbPath -PathType Leaf)) {
    throw 'Microsoft CDB x86 missing. Install Debugging Tools for Windows via the Windows SDK, or use -CdbPath.'
}
$debugDir = Join-Path $env:TEMP 'SpiderDI8Trace'
New-Item -ItemType Directory -Path $debugDir -Force | Out-Null
$debugScript = Join-Path $debugDir 'trace.js'
if ($debugScript.Contains(' ')) {
    throw "WinDbg .scriptload needs a path without spaces: $debugScript"
}
Copy-Item -LiteralPath $scriptSource -Destination $debugScript -Force
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath = Join-Path $debugDir "SpiderDI8-WinDbg-$stamp.log"
Write-Host "Game: $exe"
Write-Host "CDB x86: $CdbPath"
Write-Host "Debugger log: $logPath"
Write-Host 'In the MAIN MENU: test controls; Alt+Tab ONCE; return; press F9; retest.'
Write-Host 'To print counters: Ctrl+Break in CDB, then: dx @$scriptContents.di8Summary()'
Write-Host 'To resume: g . To detach without closing game: qd .'
Write-Host 'Debugger breakpoints may change timing; this is a diagnosis, not a fix.'
# Single quotes protect the debugger's @$scriptContents literal from PowerShell interpolation.
$startup = '.scriptload ' + $debugScript + '; dx @$scriptContents.di8Start(); g'
Push-Location -LiteralPath $GameDir
try {
    & $CdbPath -hd -logo $logPath -c $startup $exe
} finally {
    Pop-Location
    Write-Host "Debugger log retained: $logPath"
}
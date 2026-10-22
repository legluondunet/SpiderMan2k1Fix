# External, read-only F12 quick-exit verification for Windows PowerShell 5.1+.
# Launch this from the game's directory:
#   powershell -NoProfile -ExecutionPolicy Bypass -File .\Verify-F12QuickExit.ps1
# Requires [System] QuickExitF12=1 and [Debug] Log=1 in SpiderMan2k1Fix.ini.
# Press F12 manually. This script never injects input or terminates the game.
[CmdletBinding()]
param(
    [string]$GameExe = ".\SpideyPC.exe",
    [ValidateRange(5,900)][int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"
$started = Get-Date
$watch = $null

try {
    $exe = (Resolve-Path -LiteralPath $GameExe -ErrorAction Stop).ProviderPath
    $folder = Split-Path -Path $exe -Parent
    if ([IO.Path]::GetFileName($exe) -ine "SpideyPC.exe") {
        throw "Expected SpideyPC.exe, got $exe"
    }
    $other = @(Get-Process -Name "SpideyPC" -ErrorAction SilentlyContinue)
    if ($other.Count -gt 0) {
        throw "Another SpideyPC.exe is already running. Close it before this test."
    }

    Write-Host "F12 exit test: starting $exe"
    Write-Host "Press F12 in the game window. This watcher never sends keys or kills the game."
    $watch = Start-Process -FilePath $exe -WorkingDirectory $folder -PassThru
    $pidToWatch = $watch.Id
    Write-Host "PID=$pidToWatch ; timeout=$TimeoutSeconds seconds"

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    [long]$peakWorkingSet = 0
    [int]$peakHandles = 0
    while ((Get-Date) -lt $deadline) {
        $watch.Refresh()
        if ($watch.HasExited) { break }
        try {
            if ($watch.WorkingSet64 -gt $peakWorkingSet) {
                $peakWorkingSet = $watch.WorkingSet64
            }
            if ($watch.HandleCount -gt $peakHandles) {
                $peakHandles = $watch.HandleCount
            }
        } catch {
            # Process may exit between HasExited and property access.
        }
        Start-Sleep -Milliseconds 250
    }
    if (-not $watch.WaitForExit(500)) {
        Write-Warning "TIMEOUT: game PID=$pidToWatch is still running. No forced termination."
        exit 2
    }
    $exitCode = $watch.ExitCode
    $elapsed = [math]::Round(((Get-Date)-$started).TotalSeconds,1)
    Write-Host "Process exited after $elapsed seconds, exit code $exitCode"
    Write-Host ("Observed peak working set: {0:N1} MiB; peak handle count: {1}" -f ($peakWorkingSet/1MB),$peakHandles)

    Start-Sleep -Seconds 2 # allow Windows Event Reporting to post its event
    $pidRemains = @(Get-Process -Id $pidToWatch -ErrorAction SilentlyContinue)
    $byNameRemains = @(Get-Process -Name "SpideyPC" -ErrorAction SilentlyContinue)
    if ($pidRemains.Count -gt 0 -or $byNameRemains.Count -gt 0) {
        Write-Warning "FAIL: SpideyPC.exe process still present."
        exit 1
    }
    Write-Host "OK: SpideyPC.exe process no longer exists; OS-owned process memory and handles are released."

    $recentLog = Get-ChildItem -LiteralPath $folder -Filter "SpiderMan2k1Fix*.log" -File -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -ge $started.AddSeconds(-2) } |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    $shortcutObserved = $false
    if ($null -ne $recentLog) {
        $lines = Get-Content -LiteralPath $recentLog.FullName -Tail 180 -ErrorAction SilentlyContinue
        $shortcutObserved = [bool]($lines | Select-String -Pattern "QUICK-EXIT-F12: (native Shell_MainMenu Quit activated: slot=3 action=18|four native movie calls completed; intro transition cleanup finished; engine Quit flag set BEFORE top-level 0x455E50 check)")
        Write-Host "Game log: $($recentLog.Name); native Quit-request trace present=$shortcutObserved"
    } else {
        Write-Warning "No recent game log found. Enable [Debug] Log=1 to verify that F12 caused the exit."
    }

    $crashEvents = @()
    $werChecked = $false
    try {
        # Check that Windows exposes the Application event log.
        $null = Get-WinEvent -ListLog "Application" -ErrorAction Stop
        $werChecked = $true
        # If no matching events exist, suppress NoMatchingEventsFound.
        $crashEvents = @(Get-WinEvent -FilterHashtable @{
            LogName = "Application"
            Id = 1000,1001
            StartTime = $started.AddSeconds(-2)
        } -ErrorAction SilentlyContinue | Where-Object {
            $_.Message -match '(?i)SpideyPC\.exe'
        })
    } catch {
        Write-Warning "Cannot read Windows Application log: crash check is inconclusive."
    }
    if ($crashEvents.Count -gt 0) {
        Write-Warning "FAIL: Windows reported an Application Error / WER event for SpideyPC.exe."
        foreach ($event in $crashEvents) {
            Write-Host ("Event {0}, time {1}, provider {2}" -f $event.Id,$event.TimeCreated,$event.ProviderName)
        }
        exit 1
    }

    if (-not $werChecked) {
        Write-Warning "INCONCLUSIVE: Windows crash event log was unavailable."
        exit 3
    }

    if ($exitCode -ne 0) {
        Write-Warning "FAIL: process exit code is nonzero ($exitCode)."
        exit 1
    }
    if (-not $shortcutObserved) {
        Write-Warning "INCONCLUSIVE: normal exit detected, but no F12 -> MENU Quit dispatch line was verified. Enable Log=1 and repeat."
        exit 3
    }
    Write-Host "PASS: F12 -> original MENU Quit logged, exit code 0, process gone, no matching crash event found."
    Write-Host "Note: this does not prove that every native COM object was explicitly released before exit."
    exit 0
}
finally {
    if ($null -ne $watch) { $watch.Dispose() }
}

param([string]$EngineRoot = 'H:\epic\UE_5.8', [int]$Port = 17784)
$ErrorActionPreference = 'Stop'
$economyEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$economyProject = Join-Path $PSScriptRoot 'Vanthex.uproject'
$economySaved = Join-Path $PSScriptRoot 'Saved'
$economyProcesses = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
if (!(Test-Path -LiteralPath $economyEditor) -or !(Test-Path -LiteralPath $economyProject)) { throw 'Engine or Vanthex project missing' }
New-Item -ItemType Directory -Path $economySaved -Force | Out-Null
foreach ($economyRole in @('Host', 'Client')) {
    $economyOldReport = Join-Path $economySaved ("EconomyNetwork$economyRole.txt")
    if (Test-Path -LiteralPath $economyOldReport) { Remove-Item -LiteralPath $economyOldReport -Force }
}
$economyStarted = Get-Date
$economyDeadline = $economyStarted.AddSeconds(180)
function Start-EconomyPeer([string]$Map, [string]$Role) {
    $economyArgs = @(('"' + $economyProject + '"'), $Map, '-game', '-EconomyNetworkTest',
        "-port=$Port", '-nullrhi', '-nosound', '-unattended', '-ExecCmds="t.MaxFPS 60"',
        ('-abslog="' + (Join-Path $economySaved ("Economy-network-$Role.log")) + '"'))
    $economyPeer = Start-Process -FilePath $economyEditor -ArgumentList $economyArgs -WindowStyle Hidden -PassThru
    $economyProcesses.Add($economyPeer)
    return $economyPeer
}
try {
    $economyHost = Start-EconomyPeer '/Game/Vanthex/Maps/GridV3?listen' 'host'
    Start-Sleep -Seconds 8
    $economyHost.Refresh()
    if ($economyHost.HasExited) { throw "Economy host exited before client join: $($economyHost.ExitCode)" }
    $economyClient = Start-EconomyPeer "127.0.0.1:$Port" 'client'
    # Bound editor/plugin startup as well as each peer's 80-second in-game probe.
    while (($economyProcesses | Where-Object { -not $_.HasExited }) -and (Get-Date) -lt $economyDeadline) {
        Start-Sleep -Seconds 1
    }
    foreach ($economyPeer in $economyProcesses) {
        $economyPeer.Refresh()
        if (-not $economyPeer.HasExited) { throw 'Economy network test exceeded the 180-second deadline' }
        if ($economyPeer.ExitCode -ne 0) { throw "Economy peer failed: PID $($economyPeer.Id), exit $($economyPeer.ExitCode)" }
    }
    foreach ($economyRole in @('Host', 'Client')) {
        $economyReport = Join-Path $economySaved ("EconomyNetwork$economyRole.txt")
        if (!(Test-Path -LiteralPath $economyReport) -or (Get-Item -LiteralPath $economyReport).LastWriteTime -lt $economyStarted) { throw "Missing fresh $economyRole report" }
        $economyResult = Get-Content -LiteralPath $economyReport -Raw
        if ($economyResult -notmatch '^PASS ') { throw $economyResult }
        Write-Output $economyResult
    }
} finally {
    # Stop only process handles created by this invocation, never other Unreal sessions.
    foreach ($economyPeer in $economyProcesses) {
        $economyPeer.Refresh()
        if (-not $economyPeer.HasExited) { Stop-Process -InputObject $economyPeer }
        $economyPeer.Dispose()
    }
}

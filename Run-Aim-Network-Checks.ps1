param([string]$EngineRoot = 'H:\epic\UE_5.8', [int]$Port = 17786, [switch]$OwnWeaponModels)
$ErrorActionPreference = 'Stop'
$aimEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$aimProject = Join-Path $PSScriptRoot 'Vanthex.uproject'
$aimSaved = Join-Path $PSScriptRoot 'Saved'
$aimPeers = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
if (!(Test-Path -LiteralPath $aimEditor) -or !(Test-Path -LiteralPath $aimProject)) { throw 'Engine or Vanthex project missing' }
New-Item -ItemType Directory -Path $aimSaved -Force | Out-Null
foreach ($aimRole in @('Host', 'Client')) {
    $aimOldReport = Join-Path $aimSaved ("AimNetwork$aimRole.txt")
    if (Test-Path -LiteralPath $aimOldReport) { Remove-Item -LiteralPath $aimOldReport -Force }
}
$aimStarted = Get-Date
$aimDeadline = $aimStarted.AddSeconds(180)
function Start-AimPeer([string]$Map, [string]$Role) {
    $aimArguments = @(('"' + $aimProject + '"'), $Map, '-game', '-AimNetworkTest',
        "-port=$Port", '-nullrhi', '-nosound', '-unattended', '-ExecCmds="t.MaxFPS 60"',
        ('-abslog="' + (Join-Path $aimSaved ("Aim-network-$Role.log")) + '"'))
    if ($OwnWeaponModels) { $aimArguments += '-VanthexOwnWeaponModels' }
    $aimPeer = Start-Process -FilePath $aimEditor -ArgumentList $aimArguments -WindowStyle Hidden -PassThru
    $aimPeers.Add($aimPeer)
    return $aimPeer
}
try {
    $aimHost = Start-AimPeer '/Game/Vanthex/Maps/GridV3?listen' 'host'
    Start-Sleep -Seconds 8
    $aimHost.Refresh()
    if ($aimHost.HasExited) { throw "Aim host exited before client join: $($aimHost.ExitCode)" }
    $aimClient = Start-AimPeer "127.0.0.1:$Port" 'client'
    while (($aimPeers | Where-Object { -not $_.HasExited }) -and (Get-Date) -lt $aimDeadline) { Start-Sleep -Seconds 1 }
    foreach ($aimPeer in $aimPeers) {
        $aimPeer.Refresh()
        if (-not $aimPeer.HasExited) { throw 'Aim network check exceeded the 180-second deadline' }
        if ($aimPeer.ExitCode -ne 0) { throw "Aim peer failed: PID $($aimPeer.Id), exit $($aimPeer.ExitCode)" }
    }
    foreach ($aimRole in @('Host', 'Client')) {
        $aimReport = Join-Path $aimSaved ("AimNetwork$aimRole.txt")
        if (!(Test-Path -LiteralPath $aimReport) -or (Get-Item -LiteralPath $aimReport).LastWriteTime -lt $aimStarted) { throw "Missing fresh $aimRole report" }
        $aimResult = Get-Content -LiteralPath $aimReport -Raw
        if ($aimResult -notmatch '^PASS ') { throw $aimResult }
        Write-Output $aimResult.Trim()
    }
} finally {
    foreach ($aimPeer in $aimPeers) {
        $aimPeer.Refresh()
        if (-not $aimPeer.HasExited) { Stop-Process -InputObject $aimPeer }
        $aimPeer.Dispose()
    }
}

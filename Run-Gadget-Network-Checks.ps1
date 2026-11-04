param([string]$EngineRoot = 'H:\epic\UE_5.8', [int]$Port = 17785)
$ErrorActionPreference = 'Stop'
$gadgetEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$gadgetProject = Join-Path $PSScriptRoot 'Vanthex.uproject'
$gadgetSaved = Join-Path $PSScriptRoot 'Saved'
$gadgetPeers = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
if (!(Test-Path -LiteralPath $gadgetEditor) -or !(Test-Path -LiteralPath $gadgetProject)) { throw 'Engine or Vanthex project missing' }
New-Item -ItemType Directory -Path $gadgetSaved -Force | Out-Null
foreach ($gadgetRole in @('Host', 'Client')) {
    $gadgetOldReport = Join-Path $gadgetSaved ("GadgetNetwork$gadgetRole.txt")
    if (Test-Path -LiteralPath $gadgetOldReport) { Remove-Item -LiteralPath $gadgetOldReport -Force }
}
$gadgetStarted = Get-Date
$gadgetDeadline = $gadgetStarted.AddSeconds(180)
function Start-GadgetPeer([string]$Map, [string]$Role) {
    $gadgetArguments = @(('"' + $gadgetProject + '"'), $Map, '-game', '-GadgetNetworkTest',
        "-port=$Port", '-nullrhi', '-nosound', '-unattended', '-ExecCmds="t.MaxFPS 60"',
        ('-abslog="' + (Join-Path $gadgetSaved ("Gadget-network-$Role.log")) + '"'))
    $gadgetPeer = Start-Process -FilePath $gadgetEditor -ArgumentList $gadgetArguments -WindowStyle Hidden -PassThru
    $gadgetPeers.Add($gadgetPeer)
    return $gadgetPeer
}
try {
    $gadgetHost = Start-GadgetPeer '/Game/Vanthex/Maps/GridV3?listen' 'host'
    Start-Sleep -Seconds 8
    $gadgetHost.Refresh()
    if ($gadgetHost.HasExited) { throw "Gadget host exited before client join: $($gadgetHost.ExitCode)" }
    $gadgetClient = Start-GadgetPeer "127.0.0.1:$Port" 'client'
    while (($gadgetPeers | Where-Object { -not $_.HasExited }) -and (Get-Date) -lt $gadgetDeadline) { Start-Sleep -Seconds 1 }
    foreach ($gadgetPeer in $gadgetPeers) {
        $gadgetPeer.Refresh()
        if (-not $gadgetPeer.HasExited) { throw 'Gadget network check exceeded the 180-second deadline' }
        if ($gadgetPeer.ExitCode -ne 0) { throw "Gadget peer failed: PID $($gadgetPeer.Id), exit $($gadgetPeer.ExitCode)" }
    }
    foreach ($gadgetRole in @('Host', 'Client')) {
        $gadgetReport = Join-Path $gadgetSaved ("GadgetNetwork$gadgetRole.txt")
        if (!(Test-Path -LiteralPath $gadgetReport) -or (Get-Item -LiteralPath $gadgetReport).LastWriteTime -lt $gadgetStarted) { throw "Missing fresh $gadgetRole report" }
        $gadgetResult = Get-Content -LiteralPath $gadgetReport -Raw
        if ($gadgetResult -notmatch '^PASS ') { throw $gadgetResult }
        Write-Output $gadgetResult
    }
} finally {
    foreach ($gadgetPeer in $gadgetPeers) {
        $gadgetPeer.Refresh()
        if (-not $gadgetPeer.HasExited) { Stop-Process -InputObject $gadgetPeer }
        $gadgetPeer.Dispose()
    }
}

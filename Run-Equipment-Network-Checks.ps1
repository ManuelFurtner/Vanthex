param([string]$EngineRoot = 'H:\epic\UE_5.8', [int]$Port = 17782)
$ErrorActionPreference = 'Stop'
$equipmentEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$equipmentProject = Join-Path $PSScriptRoot 'Vanthex.uproject'
$equipmentSaved = Join-Path $PSScriptRoot 'Saved'
$equipmentStarted = Get-Date
$equipmentProcesses = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
function Start-EquipmentPeer([string]$Map, [string]$Role) {
    $equipmentArgs = @(('"' + $equipmentProject + '"'), $Map, '-game', '-EquipmentNetworkTest',
        "-port=$Port", '-nullrhi', '-nosound', '-unattended', '-ExecCmds="t.MaxFPS 60"',
        ('-abslog="' + (Join-Path $equipmentSaved ("Equipment-network-$Role.log")) + '"'))
    $equipmentPeer = Start-Process -FilePath $equipmentEditor -ArgumentList $equipmentArgs -WindowStyle Hidden -PassThru
    $equipmentProcesses.Add($equipmentPeer)
    return $equipmentPeer
}
try {
    $equipmentHost = Start-EquipmentPeer '/Game/Vanthex/Maps/GridV3?listen' 'host'
    Start-Sleep -Seconds 8
    $equipmentClient = Start-EquipmentPeer "127.0.0.1:$Port" 'client'
    # Include editor/plugin startup and shader-cache contention before the 45s in-game probe.
    $equipmentDeadline = (Get-Date).AddSeconds(150)
    while (($equipmentProcesses | Where-Object { -not $_.HasExited }) -and (Get-Date) -lt $equipmentDeadline) { Start-Sleep -Seconds 1 }
    foreach ($equipmentPeer in $equipmentProcesses) {
        if (-not $equipmentPeer.HasExited) { throw 'Equipment network test timed out' }
        $equipmentPeer.Refresh()
        if ($equipmentPeer.ExitCode -ne 0) { throw "Equipment peer failed: exit $($equipmentPeer.ExitCode)" }
    }
    foreach ($equipmentRole in @('Host','Client')) {
        $equipmentReport = Join-Path $equipmentSaved ("EquipmentNetwork$equipmentRole.txt")
        if (!(Test-Path -LiteralPath $equipmentReport) -or (Get-Item -LiteralPath $equipmentReport).LastWriteTime -lt $equipmentStarted) { throw "Missing fresh $equipmentRole report" }
        $equipmentResult = Get-Content -LiteralPath $equipmentReport -Raw
        if ($equipmentResult -notmatch '^PASS ') { throw $equipmentResult }
        Write-Output $equipmentResult
    }
} finally {
    foreach ($equipmentPeer in $equipmentProcesses) {
        if (-not $equipmentPeer.HasExited) { Stop-Process -InputObject $equipmentPeer }
        $equipmentPeer.Dispose()
    }
}

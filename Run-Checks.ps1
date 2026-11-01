param([string]$Engine = 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe')
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$projectFile = Join-Path $projectRoot 'Vanthex.uproject'
$auditRoot = Join-Path $projectRoot 'Saved\Audit-2026-11-01'
$started = Get-Date
$ownedProcesses = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
function Start-CheckProcess([string[]]$Extra, [string]$LogName) {
    $launchArgs = @($projectFile) + $Extra + @('-game','-nullrhi','-nosound','-unattended','-ExecCmds="t.MaxFPS 60"',('-abslog=' + (Join-Path $auditRoot $LogName)))
    $process = Start-Process -FilePath $Engine -ArgumentList $launchArgs -WindowStyle Hidden -PassThru
    $ownedProcesses.Add($process)
    return $process
}
function Wait-Report([string]$Name, [datetime]$Since) {
    $path = Join-Path $projectRoot ('Saved\' + $Name)
    $deadline = (Get-Date).AddSeconds(100)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path -LiteralPath $path) -and (Get-Item -LiteralPath $path).LastWriteTime -gt $Since) {
            $report = Get-Content -LiteralPath $path -Raw
            if ($report -match '^PASS') { Write-Output $report; return }
            throw ('Check failed: ' + $report)
        }
        Start-Sleep -Seconds 1
    }
    throw ('Timed out: ' + $Name)
}
try {
    $regression = Start-CheckProcess @('/Engine/Maps/Entry','-VanthexSelfTest') 'Final-regression.log'
    if (-not $regression.WaitForExit(100000)) { throw 'Regression timeout' }
    if ($regression.ExitCode -ne 0) { throw ('Regression exit code: ' + $regression.ExitCode) }
    $reportPath = Join-Path $projectRoot 'Saved\VanthexSelfTest.txt'
    if ((Get-Item -LiteralPath $reportPath).LastWriteTime -lt $started -or (Get-Content -LiteralPath $reportPath -First 1) -ne 'OVERALL PASS') { throw 'Missing fresh regression PASS' }
    Write-Output 'Regression: PASS'
    $mapStarted = Get-Date
    $mapCheck = Start-CheckProcess @('/Game/Vanthex/Maps/Foundry','-VanthexMapTest') 'Final-map.log'
    if (-not $mapCheck.WaitForExit(100000)) { throw 'Map validation timeout' }
    $mapReport = Join-Path $projectRoot 'Saved\VanthexMapTest.txt'
    if ($mapCheck.ExitCode -ne 0 -or (Get-Item -LiteralPath $mapReport).LastWriteTime -lt $mapStarted -or (Get-Content -LiteralPath $mapReport -First 1) -ne 'OVERALL PASS') { throw 'Map validation failed' }
    Write-Output 'Map validation: PASS'
    $energyStarted = Get-Date
    $energyCheck = Start-CheckProcess @('/Game/Vanthex/Maps/Foundry','-VanthexEnergyTest') 'Final-energy.log'
    if (-not $energyCheck.WaitForExit(100000)) { throw 'Energy/lift validation timeout' }
    $energyReport = Join-Path $projectRoot 'Saved\VanthexEnergyTest.txt'
    if ($energyCheck.ExitCode -ne 0 -or (Get-Item -LiteralPath $energyReport).LastWriteTime -lt $energyStarted -or (Get-Content -LiteralPath $energyReport -First 1) -ne 'OVERALL PASS') { throw 'Energy/lift validation failed' }
    Write-Output 'Energy and lifts: PASS'
    $networkStarted = Get-Date
    $hostCheck = Start-CheckProcess @('/Game/Vanthex/Maps/Foundry?listen','-port=17777','-VanthexNetworkProbe') 'Final-network-host.log'
    Start-Sleep -Seconds 8
    $clientCheck = Start-CheckProcess @('127.0.0.1:17777','-VanthexNetworkProbe') 'Final-network-client.log'
    Wait-Report 'NetworkHost.txt' $networkStarted
    Wait-Report 'NetworkClient.txt' $networkStarted
} finally {
    foreach ($process in $ownedProcesses) {
        if (-not $process.HasExited) { Stop-Process -InputObject $process }
    }
}


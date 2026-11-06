#Requires -Version 7.2
[CmdletBinding()]
param(
    [string]$ImageTag = 'vanthex-server:local',
    [string]$ClientExecutable,
    [switch]$RoundCycle,
    [ValidateRange(1024, 65535)][int]$TestPort = 17879
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!$ClientExecutable) {
    $ClientExecutable = Join-Path $projectRoot 'Saved/Standalone/Windows/Vanthex/Binaries/Win64/Vanthex.exe'
}
if (!(Test-Path -LiteralPath $ClientExecutable)) { throw 'Matching packaged Windows client not found.' }
if (!(Get-Command docker -ErrorAction SilentlyContinue)) {
    $env:PATH = (Join-Path $env:LOCALAPPDATA 'Programs/DockerDesktop/resources/bin') + ';' + $env:PATH
}
$name = 'vanthex-smoke-' + [Guid]::NewGuid().ToString('N').Substring(0, 12)
$reportDir = Join-Path $projectRoot "Saved/LinuxServer/$name"
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
$serverLog = Join-Path $reportDir 'server.log'
$clients = @()
$created = $false
$passed = $false
try {
    $imageId = & docker image inspect $ImageTag --format '{{.Id}}'
    if ($LASTEXITCODE -ne 0) { throw "Missing runtime image: $ImageTag" }
    $serverArgs = @()
    if ($RoundCycle) { $serverArgs += '-RoundCycleTest' }
    $id = & docker run --detach --name $name --publish "127.0.0.1:${TestPort}:7777/udp" $ImageTag @serverArgs
    if ($LASTEXITCODE -ne 0) { throw 'Could not start test container.' }
    $created = $true
    $ready = $false
    for ($i = 0; $i -lt 120; $i++) {
        $log = (& docker logs $name 2>&1 | Out-String)
        if ($log -match 'IpNetDriver listening on port 7777') { $ready = $true; break }
        if ((& docker inspect --format '{{.State.Running}}' $name) -ne 'true') { throw 'Server exited before binding UDP.' }
        Start-Sleep -Seconds 1
    }
    if (!$ready) { throw 'Server readiness timeout.' }
    foreach ($label in @('A', 'B')) {
        $clientLog = Join-Path $reportDir "client-$label.log"
        $argsList = @("127.0.0.1:${TestPort}?Name=LinuxSmoke$label", '-nullrhi', '-nosound', '-unattended', "-abslog=`"$clientLog`"")
        $clients += Start-Process -FilePath $ClientExecutable -ArgumentList $argsList -WindowStyle Hidden -PassThru
    }
    $testDeadline = [DateTime]::UtcNow.AddSeconds($(if ($RoundCycle) { 155 } else { 90 }))
    while ([DateTime]::UtcNow -lt $testDeadline) {
        Start-Sleep -Seconds 1
        $log = (& docker logs $name 2>&1 | Out-String)
        if ($log -match 'ROUND_CYCLE FAIL') { throw 'Automatic round/match cycle regression failed.' }
        if (($clients | Where-Object HasExited).Count) { throw 'A test client exited unexpectedly.' }
        if ((& docker inspect --format '{{.State.Running}}' $name) -ne 'true') { throw 'Server exited during client test.' }
        # Local player settings can override the URL's Name option. This container
        # uses a unique loopback-only port and only our two test clients connect.
        if ([regex]::Matches($log, 'Join succeeded:').Count -ge 2 -and $log -match 'Round 1 active') {
            $clientLogs = @('A', 'B') | ForEach-Object { Get-Content -LiteralPath (Join-Path $reportDir "client-$_.log") -Raw }
            if (($clientLogs | Where-Object { $_ -match 'Welcomed by server' -and $_ -match '/Game/Vanthex/Maps/GridV4' }).Count -eq 2 -and (!$RoundCycle -or $log -match 'ROUND_CYCLE PASS')) {
                $passed = $true
                break
            }
        }
    }
    if (!$passed) { throw 'Two client joins and active round were not confirmed before timeout.' }
    if (($log + ($clientLogs -join "`n")) -match 'Fatal error:|NetworkFailure|OutdatedClient|OutdatedServer|NetChecksumMismatch') {
        $passed = $false
        throw 'Runtime or network compatibility error in smoke-test logs.'
    }
    # This is a cross-platform connection/startup smoke test, not full gameplay QA.
    [ordered]@{passed=$true; image=$imageId; clients=2; map='GridV4'; roundActive=$true;
        networkChecksDisabled=$false; roundCycleTest=[bool]$RoundCycle; time=(Get-Date).ToString('o')} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $reportDir 'result.json')
    Write-Host "PASS: Linux server, two Windows clients, active GRID V4 round. Logs: $reportDir"
} finally {
    if ($created) {
        & docker logs $name 2>&1 | Set-Content -LiteralPath $serverLog
        & docker stop --time 15 $name | Out-Null
        & docker inspect $name | Set-Content -LiteralPath (Join-Path $reportDir 'container-inspect.json')
        & docker rm $name | Out-Null
    }
    foreach ($client in $clients) {
        if (!$client.HasExited) { Stop-Process -Id $client.Id }
    }
}

param([string]$Engine = 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe')
$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'Vanthex.uproject'
$saved = Join-Path $PSScriptRoot 'Saved'
$ownedProcesses = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
function Start-GridCheck([string[]]$Extra, [string]$LogName) {
    $launchArgs = @('"' + $projectFile + '"') + $Extra + @('-game','-nullrhi','-nosound','-unattended','-ExecCmds="t.MaxFPS 60"',('-abslog="' + (Join-Path $saved $LogName) + '"'))
    $process = Start-Process -FilePath $Engine -ArgumentList $launchArgs -WindowStyle Hidden -PassThru
    $ownedProcesses.Add($process)
    return $process
}
function Assert-Report([string]$Name, [datetime]$Since, [string]$Pattern='^OVERALL PASS') {
    $path = Join-Path $saved $Name
    if (!(Test-Path -LiteralPath $path) -or (Get-Item -LiteralPath $path).LastWriteTime -lt $Since) { throw "Missing fresh report: $Name" }
    $report = Get-Content -LiteralPath $path -Raw
    if ($report -notmatch $Pattern) { throw "Check failed: $Name`n$report" }
    Write-Output "$Name PASS"
}
try {
    foreach ($check in @(
        @('/Engine/Maps/Entry','-VanthexSelfTest','VanthexSelfTest.txt','GridV4-regression.log'),
        @('/Game/Vanthex/Maps/GridV4','-GridV4Test','GridV4Validation.txt','GridV4-validation.log'),
        @('/Game/Vanthex/Maps/GridV4','-GridV4GameTest','GridV4Gameplay.txt','GridV4-gameplay.log')
    )) {
        $started = Get-Date
        $process = Start-GridCheck @($check[0],$check[1]) $check[3]
        $deadline = (Get-Date).AddSeconds(300)
        while (-not $process.HasExited -and (Get-Date) -lt $deadline) { Start-Sleep -Seconds 1 }
        if (-not $process.HasExited) { throw ('Timed out: ' + $check[1]) }
        if ($process.ExitCode -ne 0) { throw ('Process failed: ' + $check[1] + ', exit ' + $process.ExitCode) }
        Assert-Report $check[2] $started
    }
    $started = Get-Date
    $hostCheck = Start-GridCheck @('/Game/Vanthex/Maps/GridV4?listen','-port=17844','-VanthexNetworkProbe') 'GridV4-network-host.log'
    Start-Sleep -Seconds 8
    $clientCheck = Start-GridCheck @('127.0.0.1:17844','-VanthexNetworkProbe') 'GridV4-network-client.log'
    $deadline = (Get-Date).AddSeconds(90)
    do {
        Start-Sleep -Seconds 1
        $clientReport = Join-Path $saved 'GridV4NetworkClient.txt'
        $ready = (Test-Path -LiteralPath $clientReport) -and (Get-Item -LiteralPath $clientReport).LastWriteTime -ge $started
    } while (-not $ready -and (Get-Date) -lt $deadline)
    Assert-Report 'GridV4NetworkHost.txt' $started '^PASS'
    Assert-Report 'GridV4NetworkClient.txt' $started '^PASS'
} finally {
    foreach ($process in $ownedProcesses) {
        if (-not $process.HasExited) { Stop-Process -InputObject $process }
    }
}

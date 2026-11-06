#Requires -Version 5.1
<#
.SYNOPSIS
Runs automated VANTHEX playtest checks with the installed Unreal Editor.
.DESCRIPTION
Core and CTF run the gameplay probes on GRID V4 and Crossroads CTF. Training checks
the solo range and repeatable Core extraction. DamagePrediction
runs a dedicated server and one client on loopback for every RTT/loss combination.
Each process writes to a separate user directory under Saved/PlaytestChecks.
.EXAMPLE
.\Run-Playtest-Checks.ps1 -Scope DamagePrediction -RttMs 0,100 -PacketLossPercent 0,2
#>
[CmdletBinding()]
param(
    [ValidateSet('Core', 'CTF', 'Training', 'DamagePrediction', 'All')]
    [string]$Scope = 'All',
    [string]$EditorPath = (Join-Path ${env:ProgramFiles} 'Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'),
    [string]$ProjectPath = (Join-Path $PSScriptRoot 'Vanthex.uproject'),
    [ValidateNotNullOrEmpty()]
    [ValidateRange(0, 1000)]
    [int[]]$RttMs = @(0, 50, 100, 150),
    [ValidateNotNullOrEmpty()]
    [ValidateRange(0, 100)]
    [int[]]$PacketLossPercent = @(0),
    [ValidateRange(1024, 65535)]
    [int]$Port = 17926,
    [ValidateRange(1, 600)]
    [int]$StartupTimeoutSec = 90,
    [ValidateRange(1, 600)]
    [int]$ExitTimeoutSec = 120
)

$ErrorActionPreference = 'Stop'
foreach ($requiredFile in @($EditorPath, $ProjectPath)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required file not found: $requiredFile"
    }
}
$EditorPath = (Resolve-Path -LiteralPath $EditorPath).ProviderPath
$ProjectPath = (Resolve-Path -LiteralPath $ProjectPath).ProviderPath
$projectRoot = Split-Path -Parent $ProjectPath
$runId = '{0}-{1}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), ([guid]::NewGuid().ToString('N').Substring(0, 8))
$runRoot = Join-Path $projectRoot "Saved\PlaytestChecks\$runId"
[void][System.IO.Directory]::CreateDirectory($runRoot)
Write-Host "Automated test output: $runRoot"

function Start-PlaytestProcess {
    param(
        [string]$UserDirectory,
        [string[]]$UnrealArguments,
        [System.Collections.Generic.List[System.Diagnostics.Process]]$OwnedProcesses
    )
    [void][System.IO.Directory]::CreateDirectory((Join-Path $UserDirectory 'Saved'))
    $logPath = Join-Path $UserDirectory 'Unreal.log'
    $launchArguments = @(('"{0}"' -f $ProjectPath)) + $UnrealArguments + @(
        '-nullrhi', '-nosound', '-unattended', '-nosplash', '-ExecCmds="t.MaxFPS 60"',
        ('-userdir="{0}"' -f $UserDirectory),
        ('-abslog="{0}"' -f $logPath)
    )
    $process = Start-Process -FilePath $EditorPath -ArgumentList $launchArguments `
        -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    $OwnedProcesses.Add($process)
    return $process
}

function Wait-PlaytestServer {
    param([System.Diagnostics.Process]$Server, [string]$LogPath)
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    while ($clock.Elapsed.TotalSeconds -lt $StartupTimeoutSec) {
        if ($Server.HasExited) {
            throw "Server exited before listening (code $($Server.ExitCode)). See $LogPath"
        }
        if (Test-Path -LiteralPath $LogPath -PathType Leaf) {
            $listen = Select-String -LiteralPath $LogPath -Pattern 'IpNetDriver listening on port (\d+)' |
                Select-Object -First 1
            if ($listen) {
                $actualPort = [int]$listen.Matches[0].Groups[1].Value
                if ($actualPort -ne $Port) {
                    throw "Server listened on UDP $actualPort instead of $Port. Select a free -Port."
                }
                return
            }
        }
        Start-Sleep -Milliseconds 250
    }
    throw "Server did not report listening on UDP $Port within $StartupTimeoutSec seconds. See $LogPath"
}

function Wait-PlaytestExit {
    param([System.Diagnostics.Process]$Process, [System.Diagnostics.Process]$Server)
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    while ($clock.Elapsed.TotalSeconds -lt $ExitTimeoutSec) {
        if ($Server -and $Server.HasExited) {
            throw "Server exited during the client test (code $($Server.ExitCode))."
        }
        if ($Process.WaitForExit(250)) {
            return
        }
    }
    throw "Test process did not exit within $ExitTimeoutSec seconds."
}

function Assert-PacketProfile {
    param([string]$LogPath, [int]$LagMs, [int]$LossPercent)
    # UE 5.8 logs these values when its NetDriver parses the emulation settings.
    foreach ($pattern in @("\bPktLag set to ${LagMs}\b", "\bPktLoss set to ${LossPercent}\b")) {
        if (-not (Select-String -LiteralPath $LogPath -Pattern $pattern -Quiet)) {
            throw "Engine did not confirm packet profile (outgoing delay ${LagMs}ms, loss $LossPercent%). See $LogPath"
        }
    }
}

function Get-CompletedCtfReport {
    param([string]$UserDirectory)
    $playtestDirectory = Join-Path $UserDirectory 'Saved\Playtests'
    if (Test-Path -LiteralPath $playtestDirectory -PathType Container) {
        foreach ($file in (Get-ChildItem -LiteralPath $playtestDirectory -Filter '*.csv' -File)) {
            $rows = Import-Csv -LiteralPath $file.FullName -Encoding UTF8
            $ctf = $rows | Where-Object { $_.metric -eq 'mode' -and $_.key -eq 'CTF' -and [double]$_.value -eq 1 }
            $completed = $rows | Where-Object { $_.metric -eq 'match' -and $_.key -eq 'duration_seconds' }
            $death = $rows | Where-Object { $_.metric -eq 'deaths' -and [double]$_.value -eq 1 }
            $meleeKill = $rows | Where-Object { $_.metric -eq 'kills' -and $_.key -eq 'Knife' -and [double]$_.value -eq 1 }
            if ($ctf -and $completed -and $death -and $meleeKill) {
                return $file.FullName
            }
        }
    }
    throw "No completed CTF report contains both the test death and the Knife kill. See $playtestDirectory"
}

function Invoke-PlaytestCase {
    param(
        [string]$Name, [string]$Map, [string]$TestFlag, [string]$ReportName,
        [int]$TargetRtt = 0, [int]$LossPercent = 0
    )
    $caseRoot = Join-Path $runRoot $Name
    $owned = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
    $isNetwork = $TestFlag -eq '-DamagePredictionTest'
    $isCtf = $TestFlag -eq '-CTFTest'
    $testDirectory = $caseRoot
    if ($isNetwork) { $testDirectory = Join-Path $caseRoot 'Client' }
    $reportPath = Join-Path $testDirectory "Saved\$ReportName"
    $status = 'PASS'
    $assertions = $null
    $playtestReport = $null
    $detail = ''

    try {
        $server = $null
        if ($isNetwork) {
            # PktLag and PktLoss affect outgoing packets on each peer. Split the
            # target round-trip delay between them, keeping odd totals exact.
            $serverLag = [int][math]::Ceiling($TargetRtt / 2.0)
            $clientLag = $TargetRtt - $serverLag
            $serverDirectory = Join-Path $caseRoot 'Server'
            $serverLog = Join-Path $serverDirectory 'Unreal.log'
            $server = Start-PlaytestProcess $serverDirectory @(
                $Map, '-server', $TestFlag, "-port=$Port", '-multihome=127.0.0.1',
                "-PktLag=$serverLag", "-PktLoss=$LossPercent"
            ) $owned
            Wait-PlaytestServer $server $serverLog
            Assert-PacketProfile $serverLog $serverLag $LossPercent
            $testArguments = @(
                "127.0.0.1:${Port}?Name=PredictionTest", '-game', $TestFlag,
                "-PktLag=$clientLag", "-PktLoss=$LossPercent"
            )
        } else {
            $testArguments = @($Map, '-game', $TestFlag)
            if ($isCtf) { $testArguments += '-VanthexPlaytestReport' }
        }

        $test = Start-PlaytestProcess $testDirectory $testArguments $owned
        Wait-PlaytestExit $test $server
        if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) {
            throw "Test exited with code $($test.ExitCode) without a report. See $testDirectory\Unreal.log"
        }
        $report = Get-Content -LiteralPath $reportPath -Raw
        $detail = ($report -split '\r?\n', 2)[0]
        if ($test.ExitCode -ne 0) {
            throw "Test exited with code $($test.ExitCode): $detail"
        }
        if ($isNetwork) {
            Assert-PacketProfile (Join-Path $testDirectory 'Unreal.log') $clientLag $LossPercent
            if ($detail -notmatch '^PASS\s') { throw "Prediction report did not pass: $detail" }
        } else {
            if ($detail -notmatch '^OVERALL PASS CHECKS (\d+) FAILURES 0$') {
                throw "Gameplay report did not pass: $detail"
            }
            $assertions = [int]$Matches[1]
            if ($isCtf) { $playtestReport = Get-CompletedCtfReport $testDirectory }
        }
    } catch {
        $status = 'FAIL'
        $detail = $_.Exception.Message
    } finally {
        for ($index = $owned.Count - 1; $index -ge 0; $index--) {
            $process = $owned[$index]
            try {
                if (-not $process.HasExited) {
                    $process.Kill()
                    if (-not $process.WaitForExit(5000)) { throw 'Process did not terminate.' }
                }
            } catch {
                if (-not $process.HasExited) {
                    $status = 'FAIL'
                    $detail += " Cleanup of owned process $($process.Id) failed: $($_.Exception.Message)"
                }
            } finally {
                $process.Dispose()
            }
        }
    }

    Write-Host "$status $Name - $detail"
    [pscustomobject]@{
        Name = $Name
        Status = $status
        Detail = $detail
        GameplayAssertions = $assertions
        TargetRttMs = if ($isNetwork) { $TargetRtt } else { $null }
        OutgoingPacketLossPercentPerPeer = if ($isNetwork) { $LossPercent } else { $null }
        ReportPath = $reportPath
        PlaytestReportPath = $playtestReport
        LogDirectory = $caseRoot
    }
}

$grid = '/Game/Vanthex/Maps/GridV4'
$results = @(
    if ($Scope -in @('Core', 'All')) {
        Invoke-PlaytestCase 'Core' $grid '-ControlsTest' 'ControlsTest.txt'
    }
    if ($Scope -in @('CTF', 'All')) {
        Invoke-PlaytestCase 'CTF' '/Game/Vanthex/Maps/CrossroadsCTF' '-CTFTest' 'CTFTest.txt'
    }
    if ($Scope -in @('Training', 'All')) {
        Invoke-PlaytestCase 'Training' '/Game/Vanthex/Maps/MainMenu?game=/Script/Vanthex.VanthexTrainingMode' `
            '-TrainingRangeTest' 'TrainingRangeTest.txt'
    }
    if ($Scope -in @('DamagePrediction', 'All')) {
        foreach ($rtt in ($RttMs | Select-Object -Unique)) {
            foreach ($loss in ($PacketLossPercent | Select-Object -Unique)) {
                Invoke-PlaytestCase "DamagePrediction-rtt$rtt-loss$loss" $grid `
                    '-DamagePredictionTest' 'DamagePredictionTest.txt' $rtt $loss
            }
        }
    }
)
$failed = @($results | Where-Object Status -eq 'FAIL').Count
$summary = [ordered]@{
    AutomatedCases = $results.Count
    AutomatedPassed = $results.Count - $failed
    AutomatedFailed = $failed
    HumanPlaytests = 0
    Cases = $results
}
$summaryPath = Join-Path $runRoot 'Summary.json'
$summary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host "Automated cases passed: $($summary.AutomatedPassed)/$($summary.AutomatedCases). Human playtests: 0."
Write-Host "Summary: $summaryPath"
if ($failed -ne 0) { exit 1 }
exit 0

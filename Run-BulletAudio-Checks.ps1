param(
    [string]$Engine = 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [switch]$SkipRegression,
    [switch]$SkipNetwork,
    [switch]$IncludePlayback
)
$ErrorActionPreference = 'Stop'
$projectFile = Join-Path $PSScriptRoot 'Vanthex.uproject'
$saved = Join-Path $PSScriptRoot 'Saved'
$ownedProcesses = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
function Start-BulletCheck([string[]]$Extra, [string]$LogName) {
    $launchArgs = @($projectFile) + $Extra + @('-game','-nullrhi','-nosound','-unattended','-nopause',('-abslog=' + (Join-Path $saved $LogName)))
    $process = Start-Process -FilePath $Engine -ArgumentList $launchArgs -WindowStyle Hidden -PassThru
    $ownedProcesses.Add($process)
    return $process
}
function Wait-BulletProcess($Process, [int]$Seconds = 180) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while (-not $Process.HasExited -and (Get-Date) -lt $deadline) { Start-Sleep -Seconds 1 }
    if (-not $Process.HasExited) { throw 'Bullet audio check timed out; see Saved/BulletAudio-*.log' }
    if ($Process.ExitCode -ne 0) { throw ('Bullet audio process failed: exit ' + $Process.ExitCode) }
}
function Assert-BulletReport([string]$Name, [datetime]$Since, [string]$Pattern = '^PASS') {
    $path = Join-Path $saved $Name
    if (!(Test-Path -LiteralPath $path) -or (Get-Item -LiteralPath $path).LastWriteTime -lt $Since) { throw "Missing fresh report: $Name" }
    $report = Get-Content -LiteralPath $path -Raw
    if ($report -notmatch $Pattern) { throw "Check failed: $Name`n$report" }
    Write-Output $report.Trim()
}
try {
    $started = Get-Date
    $reportFolder = Join-Path $saved ('BulletAudioAutomation-' + $started.ToString('yyyyMMdd-HHmmss'))
    $automation = Start-BulletCheck @('/Engine/Maps/Entry','-ExecCmds="Automation RunTests Vanthex.BulletAudio"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath=' + $reportFolder)) 'BulletAudio-automation.log'
    Wait-BulletProcess $automation
    $reportFile = Join-Path $reportFolder 'index.json'
    if (!(Test-Path -LiteralPath $reportFile)) { throw 'Missing automation JSON report' }
    $report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
    $audioTests = @($report.tests | Where-Object { $_.fullTestPath -like 'Vanthex.BulletAudio.*' })
    if ($audioTests.Count -lt 3 -or @($audioTests | Where-Object { $_.state -ne 'Success' }).Count -ne 0) { throw "Bullet audio automation failed: $reportFile" }
    Write-Output ("Automation PASS: " + $audioTests.Count + " bullet audio tests")
    if (!$SkipRegression) {
        $started = Get-Date
        $regression = Start-BulletCheck @('/Engine/Maps/Entry','-VanthexSelfTest','-ExecCmds="t.MaxFPS 60"') 'BulletAudio-regression.log'
        Wait-BulletProcess $regression
        Assert-BulletReport 'VanthexSelfTest.txt' $started '^OVERALL PASS'
    }
    if (!$SkipNetwork) {
        $started = Get-Date
        $hostCheck = Start-BulletCheck @('/Engine/Maps/Entry?listen','-port=17779','-BulletAudioNetworkTest','-ExecCmds="t.MaxFPS 60"') 'BulletAudio-network-host.log'
        Start-Sleep -Seconds 6
        $clientOne = Start-BulletCheck @('127.0.0.1:17779','-BulletAudioNetworkTest','-BulletAudioTestRole=1','-ExecCmds="t.MaxFPS 60"') 'BulletAudio-network-client1.log'
        Start-Sleep -Seconds 2
        $clientTwo = Start-BulletCheck @('127.0.0.1:17779','-BulletAudioNetworkTest','-BulletAudioTestRole=2','-ExecCmds="t.MaxFPS 60"') 'BulletAudio-network-client2.log'
        $names = @('BulletAudioNetwork-Host.txt','BulletAudioNetwork-Client1.txt','BulletAudioNetwork-Client2.txt')
        $deadline = (Get-Date).AddSeconds(120)
        do {
            Start-Sleep -Seconds 1
            $fresh = @($names | Where-Object { $path = Join-Path $saved $_; (Test-Path -LiteralPath $path) -and (Get-Item -LiteralPath $path).LastWriteTime -ge $started }).Count
            if ($hostCheck.HasExited -or $clientOne.HasExited -or $clientTwo.HasExited) { throw 'Network fixture process exited before all reports were ready' }
        } while ($fresh -lt $names.Count -and (Get-Date) -lt $deadline)
        foreach ($name in $names) { Assert-BulletReport $name $started }
    }
    if ($IncludePlayback) {
        $started = Get-Date
        $launchArgs = @($projectFile, '/Engine/Maps/Entry', '-game', '-RenderOffscreen', '-ResX=640', '-ResY=360', '-audiomixer', '-unattended', '-BulletAudioPlaybackTest', '-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0', '-ExecCmds="t.MaxFPS 60,au.NeverDisableSubmixes 1"', ('-abslog=' + (Join-Path $saved 'BulletAudio-playback.log')))
        $playback = Start-Process -FilePath $Engine -ArgumentList $launchArgs -WindowStyle Hidden -PassThru
        $ownedProcesses.Add($playback)
        Wait-BulletProcess $playback
        Assert-BulletReport 'BulletAudioPlayback.txt' $started
        python (Join-Path $PSScriptRoot 'Scripts\verify_bullet_audio_playback.py')
        if ($LASTEXITCODE -ne 0) { throw 'Actual mixer waveform verification failed' }
    }
} finally {
    foreach ($process in $ownedProcesses) {
        if (-not $process.HasExited) { Stop-Process -InputObject $process }
    }
}

param([string]$EngineRoot = 'H:\epic\UE_5.8', [switch]$SkipAutomation)
$ErrorActionPreference = 'Stop'
$economyEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$economyProject = Join-Path $PSScriptRoot 'Vanthex.uproject'
$economySaved = Join-Path $PSScriptRoot 'Saved'
$economyProcesses = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
function Start-EconomyCheck([string[]]$Extra, [string]$Log) {
    $economyArgs = @(('"' + $economyProject + '"')) + $Extra + @('-game','-nullrhi','-nosound','-unattended','-ExecCmds="t.MaxFPS 60"', ('-abslog="' + (Join-Path $economySaved $Log) + '"'))
    $economyProcess = Start-Process -FilePath $economyEditor -ArgumentList $economyArgs -WindowStyle Hidden -PassThru
    $economyProcesses.Add($economyProcess)
    return $economyProcess
}
function Wait-EconomyCheck($Process) {
    $economyDeadline = (Get-Date).AddSeconds(180)
    while (-not $Process.HasExited -and (Get-Date) -lt $economyDeadline) { Start-Sleep -Seconds 1 }
    if (-not $Process.HasExited) { throw 'Economy check timed out' }
    $Process.Refresh()
    if ($Process.ExitCode -ne 0) { throw "Economy check exited $($Process.ExitCode); inspect Saved/Economy-*.log" }
}
try {
    if (!$SkipAutomation) {
        $economyReport = Join-Path $economySaved ('EconomyAutomation-' + (Get-Date).ToString('yyyyMMdd-HHmmss'))
        $automationArgs = @(('"' + $economyProject + '"'), '/Engine/Maps/Entry', '-game','-nullrhi','-nosound','-unattended', '-ExecCmds="Automation RunTests Vanthex"', '-TestExit="Automation Test Queue Empty"', ('-ReportExportPath="' + $economyReport + '"'), ('-abslog="' + (Join-Path $economySaved 'Economy-automation.log') + '"'))
        $automationProcess = Start-Process -FilePath $economyEditor -ArgumentList $automationArgs -WindowStyle Hidden -PassThru
        $economyProcesses.Add($automationProcess)
        Wait-EconomyCheck $automationProcess
        $automationResult = Get-Content -LiteralPath (Join-Path $economyReport 'index.json') -Raw | ConvertFrom-Json
        $automationTests = @($automationResult.tests | Where-Object { $_.fullTestPath -like 'Vanthex.*' })
        if ($automationTests.Count -lt 10 -or @($automationTests | Where-Object { $_.state -ne 'Success' }).Count) { throw 'Vanthex automation failed or incomplete' }
        Write-Output ("Automation PASS: " + $automationTests.Count + ' groups; ' + $economyReport)
    }
    foreach ($diagnostic in @(@('EconomyTest','EconomyIntegration.txt'), @('CoreEconomyTest','CoreEconomy.txt'), @('WeaponProfileTest','WeaponProfiles.txt'))) {
        $economyStart = Get-Date
        $economyProcess = Start-EconomyCheck @('/Game/Vanthex/Maps/GridV3', ('-' + $diagnostic[0])) ('Economy-' + $diagnostic[0] + '.log')
        Wait-EconomyCheck $economyProcess
        $economyReportFile = Join-Path $economySaved $diagnostic[1]
        if (!(Test-Path -LiteralPath $economyReportFile) -or (Get-Item -LiteralPath $economyReportFile).LastWriteTime -lt $economyStart) { throw "Missing fresh report: $economyReportFile" }
        $economyResult = Get-Content -LiteralPath $economyReportFile -Raw
        if ($economyResult -notmatch '^OVERALL PASS' -or $economyResult -match '(?m)^FAIL ') { throw $economyResult }
        Write-Output $economyResult.Trim()
    }
} finally {
    foreach ($economyProcess in $economyProcesses) {
        if (-not $economyProcess.HasExited) { Stop-Process -InputObject $economyProcess }
        $economyProcess.Dispose()
    }
}

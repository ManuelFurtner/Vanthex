param([string]$EngineRoot = 'H:\epic\UE_5.8', [switch]$SkipAutomation)
$ErrorActionPreference = 'Stop'
$gadgetEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$gadgetProject = Join-Path $PSScriptRoot 'Vanthex.uproject'
$gadgetSaved = Join-Path $PSScriptRoot 'Saved'
$gadgetProcesses = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
function Start-GadgetCheck([string[]]$Extra, [string]$Log) {
    $gadgetArguments = @(('"' + $gadgetProject + '"')) + $Extra + @('-game', '-nullrhi', '-nosound', '-unattended', '-ExecCmds="t.MaxFPS 60"', ('-abslog="' + (Join-Path $gadgetSaved $Log) + '"'))
    $gadgetProcess = Start-Process -FilePath $gadgetEditor -ArgumentList $gadgetArguments -WindowStyle Hidden -PassThru
    $gadgetProcesses.Add($gadgetProcess)
    return $gadgetProcess
}
function Wait-GadgetCheck($Process) {
    $gadgetDeadline = (Get-Date).AddSeconds(180)
    while (-not $Process.HasExited -and (Get-Date) -lt $gadgetDeadline) { Start-Sleep -Seconds 1 }
    if (-not $Process.HasExited) { throw 'Gadget check exceeded the 180-second process deadline' }
    $Process.Refresh()
    if ($Process.ExitCode -ne 0) { throw "Gadget check exited $($Process.ExitCode); inspect Saved/Gadget-*.log" }
}
try {
    if (!(Test-Path -LiteralPath $gadgetEditor)) { throw "Missing Unreal executable: $gadgetEditor" }
    if (!(Test-Path -LiteralPath $gadgetProject)) { throw "Missing project: $gadgetProject" }
    if (!$SkipAutomation) {
        $gadgetAutomationPath = Join-Path $gadgetSaved ('GadgetAutomation-' + (Get-Date).ToString('yyyyMMdd-HHmmss-fff'))
        $gadgetAutomationArgs = @(('"' + $gadgetProject + '"'), '/Engine/Maps/Entry', '-game', '-nullrhi', '-nosound', '-unattended', '-ExecCmds="Automation RunTests Vanthex.Gadgets"', '-TestExit="Automation Test Queue Empty"', ('-ReportExportPath="' + $gadgetAutomationPath + '"'), ('-abslog="' + (Join-Path $gadgetSaved 'Gadget-automation.log') + '"'))
        $gadgetAutomationProcess = Start-Process -FilePath $gadgetEditor -ArgumentList $gadgetAutomationArgs -WindowStyle Hidden -PassThru
        $gadgetProcesses.Add($gadgetAutomationProcess)
        Wait-GadgetCheck $gadgetAutomationProcess
        $gadgetAutomation = Get-Content -LiteralPath (Join-Path $gadgetAutomationPath 'index.json') -Raw | ConvertFrom-Json
        $gadgetTests = @($gadgetAutomation.tests | Where-Object { $_.fullTestPath -like 'Vanthex.Gadgets.*' })
        if ($gadgetTests.Count -ne 6 -or @($gadgetTests | Where-Object { $_.state -ne 'Success' }).Count) { throw 'Gadget automation failed or one of the six groups was not run' }
        Write-Output ("Gadget automation PASS: " + $gadgetTests.Count + ' groups; ' + $gadgetAutomationPath)
    }
    $gadgetStart = Get-Date
    $gadgetProcess = Start-GadgetCheck @('/Game/Vanthex/Maps/GridV3', '-GadgetTest') 'Gadget-integration.log'
    Wait-GadgetCheck $gadgetProcess
    $gadgetReportFile = Join-Path $gadgetSaved 'GadgetIntegration.txt'
    if (!(Test-Path -LiteralPath $gadgetReportFile) -or (Get-Item -LiteralPath $gadgetReportFile).LastWriteTime -lt $gadgetStart) { throw "Missing fresh report: $gadgetReportFile" }
    $gadgetResult = Get-Content -LiteralPath $gadgetReportFile -Raw
    if ($gadgetResult -notmatch '^OVERALL PASS' -or $gadgetResult -match '(?m)^FAIL ') { throw $gadgetResult }
    Write-Output $gadgetResult.Trim()
} finally {
    foreach ($gadgetProcess in $gadgetProcesses) {
        if (-not $gadgetProcess.HasExited) { Stop-Process -InputObject $gadgetProcess }
        $gadgetProcess.Dispose()
    }
}

param([string]$EngineRoot = 'H:\epic\UE_5.8', [switch]$SkipAutomation, [switch]$CaptureShots, [switch]$OwnWeaponModels)
$ErrorActionPreference = 'Stop'
$aimEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$aimProject = Join-Path $PSScriptRoot 'Vanthex.uproject'
$aimSaved = Join-Path $PSScriptRoot 'Saved'
$aimProcesses = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
$aimModelFlags = @()
if ($OwnWeaponModels) { $aimModelFlags += '-VanthexOwnWeaponModels' }
function Wait-AimCheck($Process, [int]$Seconds = 180) {
    $aimDeadline = (Get-Date).AddSeconds($Seconds)
    while (-not $Process.HasExited -and (Get-Date) -lt $aimDeadline) { Start-Sleep -Seconds 1 }
    $Process.Refresh()
    if (-not $Process.HasExited) { throw "Aim check exceeded the $Seconds-second process deadline" }
    if ($Process.ExitCode -ne 0) { throw "Aim check exited $($Process.ExitCode); inspect Saved/Aim-*.log" }
}
function Start-AimProcess([string]$Executable, [string[]]$Extra, [string]$Log) {
    $aimArguments = @(('"' + $aimProject + '"')) + $Extra + $aimModelFlags + @('-game', '-unattended', ('-abslog="' + (Join-Path $aimSaved $Log) + '"'))
    $aimProcess = Start-Process -FilePath $Executable -ArgumentList $aimArguments -WindowStyle Hidden -PassThru
    $aimProcesses.Add($aimProcess)
    return $aimProcess
}
function Read-FreshAimReport([string]$File, [datetime]$Started) {
    if (!(Test-Path -LiteralPath $File) -or (Get-Item -LiteralPath $File).LastWriteTime -lt $Started) { throw "Missing fresh report: $File" }
    $aimResult = Get-Content -LiteralPath $File -Raw
    if ($aimResult -notmatch '^(OVERALL )?PASS\b' -or $aimResult -match '(?m)^FAIL ') { throw $aimResult }
    return $aimResult
}
try {
    if (!(Test-Path -LiteralPath $aimEditor) -or !(Test-Path -LiteralPath $aimProject)) { throw 'Engine or Vanthex project missing' }
    New-Item -ItemType Directory -Path $aimSaved -Force | Out-Null
    if (!$SkipAutomation) {
        $aimAutomationPath = Join-Path $aimSaved ('AimAutomation-' + (Get-Date).ToString('yyyyMMdd-HHmmss-fff'))
        $aimAutomationArgs = @('/Engine/Maps/Entry', '-nullrhi', '-nosound', '-ExecCmds="Automation RunTests Vanthex.Aim"', '-TestExit="Automation Test Queue Empty"', ('-ReportExportPath="' + $aimAutomationPath + '"'))
        $aimAutomationProcess = Start-AimProcess $aimEditor $aimAutomationArgs 'Aim-automation.log'
        Wait-AimCheck $aimAutomationProcess
        $aimAutomation = Get-Content -LiteralPath (Join-Path $aimAutomationPath 'index.json') -Raw | ConvertFrom-Json
        $aimTests = @($aimAutomation.tests | Where-Object { $_.fullTestPath -like 'Vanthex.Aim.*' })
        if ($aimTests.Count -ne 1 -or @($aimTests | Where-Object { $_.state -ne 'Success' }).Count) { throw 'Aim automation failed or its group was not run' }
        Write-Output "Aim automation PASS: $aimAutomationPath"
    }
    $aimStarted = Get-Date
    $aimRuntime = Start-AimProcess $aimEditor @('/Game/Vanthex/Maps/GridV3', '-AimTest', '-nullrhi', '-nosound', '-ExecCmds="t.MaxFPS 60"') 'Aim-integration.log'
    Wait-AimCheck $aimRuntime 120
    Write-Output (Read-FreshAimReport (Join-Path $aimSaved 'AimTest.txt') $aimStarted).Trim()
    if ($CaptureShots) {
        $aimRenderEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
        if (!(Test-Path -LiteralPath $aimRenderEditor)) { throw "Missing render executable: $aimRenderEditor" }
        $aimCaptureStart = Get-Date
        $aimCapture = Start-AimProcess $aimRenderEditor @('/Game/Vanthex/Maps/GridV3', '-AimShots', '-RenderOffscreen', '-ResX=1280', '-ResY=720', '-nosound', '-ExecCmds="t.MaxFPS 60"') 'Aim-shots.log'
        Wait-AimCheck $aimCapture 150
        Write-Output (Read-FreshAimReport (Join-Path $aimSaved 'AimShots\capture.txt') $aimCaptureStart).Trim()
        $aimPngs = @(Get-ChildItem -LiteralPath (Join-Path $aimSaved 'AimShots') -Filter '*.png' | Where-Object { $_.LastWriteTime -ge $aimCaptureStart -and $_.Length -gt 0 })
        if ($aimPngs.Count -ne 9) { throw 'Expected nine fresh ADS screenshots; image quality still requires visual review' }
        Write-Output 'Nine fresh ADS screenshots captured; inspect Saved/AimShots for visual acceptance.'
    }
} finally {
    foreach ($aimProcess in $aimProcesses) {
        $aimProcess.Refresh()
        if (-not $aimProcess.HasExited) { Stop-Process -InputObject $aimProcess }
        $aimProcess.Dispose()
    }
}

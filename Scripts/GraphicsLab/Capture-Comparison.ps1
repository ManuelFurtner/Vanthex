param([int]$Revision = 4)
$ErrorActionPreference = 'Stop'
$projectFile = 'H:\Vanthex\Vanthex.uproject'
$engineExe = 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$outDir = 'H:\Vanthex\Saved\GraphicsLab'
if (Get-Process UnrealEditor* -ErrorAction SilentlyContinue) { throw 'Close other Unreal processes before comparative measurements.' }
foreach ($case in @(
    @{Name='baseline_warm'; Map='/Game/Vanthex/Maps/GridV4'},
    @{Name='final_warm'; Map="/Game/Vanthex/Experiments/GraphicsLab/Maps/PowerLab_V${Revision}_GridV4"}
)) {
    $argsText = "$projectFile $($case.Map) -game -GridV4Shots -GridV4Shot=4 -RenderOffscreen -ResX=1920 -ResY=1080 -ForceRes -nosound -unattended -NoSplash -LocalDataCachePath=$outDir\DDC -DDC=InstalledNoZenLocalFallback -ExecCmds=`"r.VSync 0,t.MaxFPS 0,r.ScreenPercentage 100`" -abslog=$outDir\$($case.Name).log"
    $run = Start-Process -FilePath $engineExe -ArgumentList $argsText -WindowStyle Hidden -PassThru
    $run.WaitForExit()
    if ($run.ExitCode -ne 0) { throw "$($case.Name) failed: $($run.ExitCode)" }
    $logText = Get-Content -LiteralPath "$outDir\$($case.Name).log" -Raw
    if ($logText -notmatch 'GRID V4 SHOTS: complete; 1 requested files; result OK') { throw 'Capture did not complete.' }
    $caseDir = Join-Path $outDir $case.Name
    New-Item -ItemType Directory -Path $caseDir -Force | Out-Null
    Copy-Item -LiteralPath 'H:\Vanthex\Saved\GridV4Shots\04_power_distribution.png' -Destination "$caseDir\room.png"
    Copy-Item -LiteralPath 'H:\Vanthex\Saved\GridV4Shots\capture_timing.csv' -Destination "$caseDir\timing.csv"
}
'COMPARISON CAPTURES COMPLETE' | Set-Content -LiteralPath "$outDir\comparison-complete.txt"

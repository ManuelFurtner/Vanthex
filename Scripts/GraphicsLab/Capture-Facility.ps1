param([int]$WaitForProcess = 0)
$ErrorActionPreference='Stop'
$dir='H:\Vanthex\Saved\GraphicsLab\Facility'
if($WaitForProcess -gt 0){Wait-Process -Id $WaitForProcess -ErrorAction SilentlyContinue}
if(Get-Process UnrealEditor* -ErrorAction SilentlyContinue){throw 'Another Unreal process is active.'}
if((Get-Content -LiteralPath "$dir\gameplay.txt" -TotalCount 1) -ne 'OVERALL PASS'){throw 'Gameplay proof missing.'}
'OPTIMIZED CAPTURE TOUR RUNNING' | Set-Content -LiteralPath "$dir\status.txt"
$run=Start-Process 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList 'H:\Vanthex\Vanthex.uproject /Game/Vanthex/Experiments/GraphicsLab/Maps/FacilityCandidate_GridV4 -game -GridV4Shots -RenderOffscreen -ResX=1920 -ResY=1080 -ForceRes -nosound -unattended -NoSplash -LocalDataCachePath=H:\Vanthex\Saved\GraphicsLab\DDC -DDC=InstalledNoZenLocalFallback -ExecCmds="r.VSync 0,t.MaxFPS 0,r.ScreenPercentage 100" -abslog=H:\Vanthex\Saved\GraphicsLab\Facility\shots_optimized.log' -WindowStyle Hidden -PassThru
$started=$run.StartTime
$run.WaitForExit()
if($run.ExitCode -ne 0){throw "Capture failed: $($run.ExitCode)"}
$files=@(Get-ChildItem -LiteralPath 'H:\Vanthex\Saved\GridV4Shots' -File -Filter '*.png' | Where-Object LastWriteTime -ge $started)
if($files.Count -ne 22){throw "Expected 22 new captures; found $($files.Count)."}
New-Item -ItemType Directory -Force -Path "$dir\Shots" | Out-Null
foreach($file in $files){Copy-Item -LiteralPath $file.FullName -Destination "$dir\Shots\$($file.Name)"}
Copy-Item -LiteralPath 'H:\Vanthex\Saved\GridV4Shots\capture_timing.csv' -Destination "$dir\Shots\capture_timing.csv"
'GAMEPLAY AND 22 SCREENSHOTS COMPLETE' | Set-Content -LiteralPath "$dir\status.txt"

param([int]$WaitForProcess = 0)
$ErrorActionPreference = 'Stop'
$dir = 'H:\Vanthex\Saved\GraphicsLab\Facility'
$engineExe = 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$projectFile = 'H:\Vanthex\Vanthex.uproject'
$mapPath = '/Game/Vanthex/Experiments/GraphicsLab/Maps/FacilityCandidate_GridV4'
if ($WaitForProcess -gt 0) { Wait-Process -Id $WaitForProcess -ErrorAction SilentlyContinue }
$verification = Get-Content -LiteralPath "$dir\verification_candidate.json" -Raw | ConvertFrom-Json
if (-not ($verification.components_equal -and $verification.queries_equal)) { throw 'Saved-map verification failed.' }
if (Get-Process UnrealEditor* -ErrorAction SilentlyContinue) { throw 'Another Unreal process is active.' }
'GAMEPLAY TEST RUNNING' | Set-Content -LiteralPath "$dir\status.txt"
if (Test-Path -LiteralPath 'H:\Vanthex\Saved\GridV4Gameplay.txt') { Copy-Item -LiteralPath 'H:\Vanthex\Saved\GridV4Gameplay.txt' -Destination "$dir\previous_gameplay.txt" }
$common = "$projectFile $mapPath -game -unattended -nosound -NoSplash -LocalDataCachePath=H:\Vanthex\Saved\GraphicsLab\DDC -DDC=InstalledNoZenLocalFallback"
$run = Start-Process $engineExe -ArgumentList "$common -nullrhi -GridV4GameTest -abslog=$dir\gameplay.log" -WindowStyle Hidden -PassThru
$run.WaitForExit()
if ($run.ExitCode -ne 0) { throw "Gameplay process failed: $($run.ExitCode)" }
Copy-Item -LiteralPath 'H:\Vanthex\Saved\GridV4Gameplay.txt' -Destination "$dir\gameplay.txt"
if ((Get-Content -LiteralPath "$dir\gameplay.txt" -TotalCount 1) -ne 'OVERALL PASS') { throw 'Gameplay tests failed.' }
'GAMEPLAY PASS; CAPTURE TOUR RUNNING' | Set-Content -LiteralPath "$dir\status.txt"
$run = Start-Process $engineExe -ArgumentList "$common -GridV4Shots -RenderOffscreen -ResX=1920 -ResY=1080 -ForceRes -ExecCmds=`"r.VSync 0,t.MaxFPS 0,r.ScreenPercentage 100`" -abslog=$dir\shots.log" -WindowStyle Hidden -PassThru
$started = $run.StartTime
$run.WaitForExit()
if ($run.ExitCode -ne 0) { throw "Capture failed: $($run.ExitCode)" }
$files = @(Get-ChildItem -LiteralPath 'H:\Vanthex\Saved\GridV4Shots' -File -Filter '*.png' | Where-Object LastWriteTime -ge $started)
if ($files.Count -ne 22) { throw "Expected 22 new screenshots; found $($files.Count)." }
New-Item -ItemType Directory -Force -Path "$dir\Shots" | Out-Null
foreach ($file in $files) { Copy-Item -LiteralPath $file.FullName -Destination "$dir\Shots\$($file.Name)" }
Copy-Item -LiteralPath 'H:\Vanthex\Saved\GridV4Shots\capture_timing.csv' -Destination "$dir\Shots\capture_timing.csv"
'GAMEPLAY AND 22 SCREENSHOTS COMPLETE' | Set-Content -LiteralPath "$dir\status.txt"

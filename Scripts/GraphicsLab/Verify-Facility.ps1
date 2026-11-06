param([int]$WaitForProcess = 0, [switch]$Production)
$ErrorActionPreference = 'Stop'
if ($WaitForProcess -gt 0) { Wait-Process -Id $WaitForProcess -ErrorAction SilentlyContinue }
if (Get-Process UnrealEditor* -ErrorAction SilentlyContinue) { throw 'Another Unreal process is active.' }
$dir = 'H:\Vanthex\Saved\GraphicsLab\Facility'
$suffix = if ($Production) { 'production' } else { 'candidate' }
$extra = if ($Production) { '-FacilityProduction' } else { '' }
$arguments = "H:\Vanthex\Vanthex.uproject -run=pythonscript -script=H:\Vanthex\Scripts\GraphicsLab\verify_facility.py $extra -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -SCCProvider=None -nullrhi -unattended -nosound -LocalDataCachePath=H:\Vanthex\Saved\GraphicsLab\DDC -DDC=InstalledNoZenLocalFallback -abslog=$dir\verify_$suffix.log"
$run = Start-Process 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru
$started = $run.StartTime
$run.WaitForExit()
if ($run.ExitCode -ne 0) { throw "Verification process failed: $($run.ExitCode)" }
$file = Get-Item -LiteralPath "$dir\verification_$suffix.json"
if ($file.LastWriteTime -lt $started) { throw 'Verification report is stale.' }
$report = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
if (-not ($report.components_equal -and $report.queries_equal) -or $report.missing_asset_files.Count -or $report.missing_visual_meshes.Count -or $report.experiment_asset_references.Count -or $report.new_decor_collision_errors.Count) { throw 'Saved-map validation failed.' }
"SAVED $suffix VERIFIED"

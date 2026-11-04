param([switch]$Editor)
$projectFile = 'H:\Vanthex\Vanthex.uproject'
$engineExe = 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$mapPath = '/Game/Vanthex/Experiments/GraphicsLab/Maps/PowerLab_V4_GridV4'
if (-not (Test-Path -LiteralPath $projectFile)) { throw 'Vanthex project missing.' }
$launchArgs = @($projectFile, $mapPath, '-LocalDataCachePath=H:\Vanthex\Saved\GraphicsLab\DDC', '-DDC=InstalledNoZenLocalFallback')
if (-not $Editor) { $launchArgs += @('-game', '-windowed', '-ResX=1600', '-ResY=900') }
& $engineExe @launchArgs

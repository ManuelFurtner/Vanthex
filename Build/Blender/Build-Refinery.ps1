#Requires -Version 7.2
param(
 [string]$Blender='H:\VanthexTools\blender-4.5.14-windows-x64\blender.exe',
 [string]$EngineRoot='H:\epic\UE_5.8',
 [switch]$FullMap,
 [switch]$ExportEditedBlend
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$project=Join-Path $root 'Vanthex.uproject'
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
if(!(Test-Path -LiteralPath $Blender)){throw 'Supply -Blender with Blender 4.5 LTS.'}
if(Get-CimInstance Win32_Process -Filter "name='UnrealEditor.exe'" | Where-Object {$_.CommandLine -like ('*'+$project+'*')}){throw 'Save and close the Vanthex Unreal Editor first.'}
Push-Location $root
try {
 if($ExportEditedBlend){
  & $Blender --background (Join-Path $root 'ArtSource/Refinery/Vanthex_Refinery.blend') --python (Join-Path $PSScriptRoot 'export_industrial.py')
 }else{
  & $Blender --background --factory-startup --python (Join-Path $PSScriptRoot 'build_industrial.py') -- --refinery
 }
 if($LASTEXITCODE -ne 0){throw 'Blender authoring/export failed'}
 & (Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat') VanthexEditor Win64 Development ('-Project='+$project) -WaitMutex -NoHotReloadFromIDE
 if($LASTEXITCODE -ne 0){throw 'Editor build failed'}
 & $editor $project -run=VanthexBuildArt -ImportRefinery -nullrhi -nosound -unattended
 if($LASTEXITCODE -ne 0){throw 'Refinery import failed'}
 $mode=if($FullMap){'-RefineryFull'}else{'-RefineryPrototype'}
 & $editor $project -run=VanthexBuildArt -BakeGridV4 $mode -nullrhi -nosound -unattended
 if($LASTEXITCODE -ne 0){throw 'Refinery bake failed'}
 $map=if($FullMap){'/Game/Vanthex/Maps/GridV4'}else{'/Game/Vanthex/Maps/RefineryLab_GridV4'}
 & $editor $project $map -game -GridV4Test -nullrhi -nosound -unattended
 if($LASTEXITCODE -ne 0){throw 'Collision/layout validation failed'}
}finally{Pop-Location}

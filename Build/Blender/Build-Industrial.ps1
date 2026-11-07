#Requires -Version 7.2
param(
 [string]$Blender='H:\VanthexTools\blender-4.5.14-windows-x64\blender.exe',
 [string]$EngineRoot='H:\epic\UE_5.8',
 [switch]$Validate,
 [switch]$ExportEditedBlend
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$project=Join-Path $root 'Vanthex.uproject'
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
if(!(Test-Path -LiteralPath $Blender)){throw 'Supply -Blender with an installed Blender 4.5 LTS executable.'}
if(Get-CimInstance Win32_Process -Filter "name='UnrealEditor.exe'" | Where-Object {$_.CommandLine -like ('*'+$project+'*')}){throw 'Save and close the Vanthex Unreal Editor before rebuilding generated assets.'}
Push-Location $root
try {
 if($ExportEditedBlend) {
  & $Blender --background (Join-Path $root 'ArtSource/Industrial/Vanthex_Industrial.blend') --python (Join-Path $PSScriptRoot 'export_industrial.py')
 } else {
  & $Blender --background --factory-startup --python (Join-Path $PSScriptRoot 'build_industrial.py')
 }
 if($LASTEXITCODE -ne 0){throw 'Blender authoring/export failed'}
 & (Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat') VanthexEditor Win64 Development ('-Project='+$project) -WaitMutex -NoHotReloadFromIDE
 if($LASTEXITCODE -ne 0){throw 'Editor build failed'}
 foreach($phase in @('ImportBlenderIndustrial','BakeGridV4')) {
  & $editor $project -run=VanthexBuildArt ('-'+$phase) -nullrhi -nosound -unattended
  if($LASTEXITCODE -ne 0){throw "$phase failed"}
 }
 if($Validate){& (Join-Path $root 'Run-GridV4-Checks.ps1') -Engine $editor}
} finally {Pop-Location}

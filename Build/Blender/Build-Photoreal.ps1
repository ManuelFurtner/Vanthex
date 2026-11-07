#Requires -Version 7.2
param([string]$Blender='H:\VanthexTools\blender-4.5.14-windows-x64\blender.exe',[string]$EngineRoot='H:\epic\UE_5.8',[switch]$FullMap)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$project=Join-Path $root 'Vanthex.uproject'
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
if(Get-CimInstance Win32_Process -Filter "name='UnrealEditor.exe'"){throw 'Save and close Unreal before authoring.'}
$fallback=Join-Path $root 'Saved/Photorealism/Fallback'
if(!(Test-Path -LiteralPath (Join-Path $fallback 'GridV4.umap'))){
 New-Item -ItemType Directory -Path $fallback -Force | Out-Null
 Copy-Item -LiteralPath (Join-Path $root 'Content/Vanthex/Maps/GridV4.umap') -Destination (Join-Path $fallback 'GridV4.umap')
 Copy-Item -LiteralPath (Join-Path $root 'Content/Vanthex/Blender/Refinery') -Destination (Join-Path $fallback 'Refinery') -Recurse
}
foreach($script in @('build_connections.py','build_zinc.py')){
 & $Blender --background --factory-startup --python (Join-Path $PSScriptRoot $script)
 if($LASTEXITCODE -ne 0){throw ($script+' failed')}
}
& $Blender --background (Join-Path $root 'ArtSource/Refinery/Vanthex_Refinery.blend') --python (Join-Path $PSScriptRoot 'smooth_refinery.py')
if($LASTEXITCODE -ne 0){throw 'Normal correction failed'}
& (Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat') VanthexEditor Win64 Development ('-Project='+$project) -WaitMutex -NoHotReloadFromIDE
if($LASTEXITCODE -ne 0){throw 'Editor build failed'}
& $editor $project -run=VanthexBuildArt -ImportRefinery -NormalsOnly -nullrhi -nosound -unattended
if($LASTEXITCODE -ne 0){throw 'Refinery mesh import failed'}
foreach($phase in @('BuildPhotorealMaterials','ImportPhotoConnections')){
 & $editor $project -run=VanthexBuildArt ('-'+$phase) -nullrhi -nosound -unattended
 if($LASTEXITCODE -ne 0){throw ($phase+' failed')}
}
$mode=if($FullMap){'-PhotorealFull'}else{'-PhotorealPrototype'}
& $editor $project -run=VanthexBuildArt -BakeGridV4 $mode -nullrhi -nosound -unattended
if($LASTEXITCODE -ne 0){throw 'Map authoring failed'}
$map=if($FullMap){'/Game/Vanthex/Maps/GridV4'}else{'/Game/Vanthex/Maps/PhotorealLab_GridV4'}
& $editor $project $map -game -GridV4Test -PhotorealAudit -nullrhi -nosound -unattended
if($LASTEXITCODE -ne 0){throw 'Geometry/physical contract failed'}

#Requires -Version 7.2
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(Get-CimInstance Win32_Process -Filter "name='UnrealEditor.exe'"){throw 'Save and close Unreal before restoring.'}
$fallback=Join-Path $root 'Saved/Photorealism/Fallback'
if(!(Test-Path -LiteralPath (Join-Path $fallback 'GridV4.umap'))){throw 'No original map backup exists.'}
if(!(Test-Path -LiteralPath (Join-Path $fallback 'Refinery'))){throw 'No original material/mesh backup exists.'}
# Copy exact original files only. Do not remove new authored assets or any user files.
Get-ChildItem -LiteralPath (Join-Path $fallback 'Refinery') -File | ForEach-Object {
 Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $root ('Content/Vanthex/Blender/Refinery/'+$_.Name)) -Force
}
Copy-Item -LiteralPath (Join-Path $fallback 'GridV4.umap') -Destination (Join-Path $root 'Content/Vanthex/Maps/GridV4.umap') -Force
Write-Output 'Original GRID V4 map and shared Refinery assets restored. Gameplay source was not changed.'

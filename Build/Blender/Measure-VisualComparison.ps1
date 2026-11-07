#Requires -Version 7.2
param([string]$EngineRoot='H:\epic\UE_5.8',[string]$BeforeMap='/Game/Vanthex/Maps/GridV4',[string]$AfterMap='/Game/Vanthex/Maps/RefineryLab_GridV4',[string]$AfterName='Prototype',[switch]$AfterOnly)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
if(Get-CimInstance Win32_Process -Filter "name='UnrealEditor.exe'"){throw 'Close other rendered Unreal processes before measuring.'}
$runs=@()
if(!$AfterOnly){$runs+=@{Name='Before';Map=$BeforeMap}}
$runs+=@{Name=$AfterName;Map=$AfterMap}
foreach($run in $runs){foreach($shot in @(1,12)){
 $destination=Join-Path $root ('Saved/HighFidelity/'+$run.Name)
 New-Item -ItemType Directory -Path $destination -Force | Out-Null
 $start=Get-Date
 $arguments=@((Join-Path $root 'Vanthex.uproject'),$run.Map,'-game','-GridV4Shots',('-GridV4Shot='+$shot),'-VisualProfile','-ExitAfterCsvProfiling','-windowed','-ResX=1600','-ResY=900','-nosound','-unattended','-ExecCmds="t.IdleWhenNotForeground 0,r.VSync 0,t.MaxFPS 0"',('-abslog='+$destination+'/profile-'+$shot+'.log'))
 $process=Start-Process $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
 try{
  if(!$process.WaitForExit(180000)){throw 'Profiler timed out'}
  if($process.ExitCode -ne 0){throw ('Profiler exited '+$process.ExitCode)}
 }finally{if(!$process.HasExited){Stop-Process -Id $process.Id}}
 $csv=Get-ChildItem (Join-Path $root 'Saved/Profiling/CSV') -File | Where-Object {$_.LastWriteTime -ge $start} | Sort-Object LastWriteTime -Descending | Select-Object -First 1
 if(!$csv){throw 'No fresh CSV profile produced'}
 Copy-Item -LiteralPath $csv.FullName -Destination (Join-Path $destination ('profile-'+$shot+'.csv')) -Force
 Get-ChildItem (Join-Path $root 'Saved/GridV4Shots') -Filter ('{0:D2}_*.png' -f $shot) | Copy-Item -Destination $destination -Force
 Write-Output ($run.Name+' camera '+$shot+' captured')
}}

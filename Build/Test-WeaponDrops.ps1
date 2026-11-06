#Requires -Version 7.2
param([string]$Editor='H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$reportRoot=Join-Path $projectRoot 'Saved/WeaponDrops'
New-Item -ItemType Directory -Path $reportRoot -Force | Out-Null
$uproject=Join-Path $projectRoot 'Vanthex.uproject'
foreach($map in @('GridV4','CrossroadsCTF')) {
 $report=Join-Path $projectRoot 'Saved/WeaponDropTest.txt'
 $started=Get-Date
 $owned=Start-Process $Editor -ArgumentList @(('"'+$uproject+'"'),('/Game/Vanthex/Maps/'+$map),'-game','-nullrhi','-nosound','-unattended','-WeaponDropTest',('-abslog="'+$reportRoot+'/'+$map+'.log"')) -WindowStyle Hidden -PassThru
 try {
  if(!$owned.WaitForExit(90000)){throw "$map test timed out"}
  if(!(Test-Path $report) -or (Get-Item $report).LastWriteTime -lt $started){throw "$map produced no fresh report"}
  $result=Get-Content $report -Raw
  Copy-Item -LiteralPath $report -Destination (Join-Path $reportRoot ($map+'.txt'))
  Write-Output "$map : $result"
  if(!$result.StartsWith('PASS') -or $owned.ExitCode -ne 0){throw "$map test failed"}
 } finally {if(!$owned.HasExited){Stop-Process -Id $owned.Id}}
}
$processes=@()
try {
 $started=Get-Date
 $hostLog=Join-Path $reportRoot 'host.log'
 $processes+=Start-Process $Editor -ArgumentList @(('"'+$uproject+'"'),'/Game/Vanthex/Maps/GridV4?listen','-port=17866','-game','-nullrhi','-nosound','-unattended','-WeaponDropNetworkTest',('-abslog="'+$hostLog+'"')) -WindowStyle Hidden -PassThru
 $ready=$false
 for($i=0;$i -lt 40;$i++) {
  Start-Sleep -Seconds 1
  if($processes[0].HasExited){throw 'Test host exited before binding'}
  if((Test-Path $hostLog) -and (Select-String -LiteralPath $hostLog -Pattern 'listening on port 17866' -Quiet)){$ready=$true;break}
 }
 if(!$ready){throw 'Test host did not bind'}
 $processes+=Start-Process $Editor -ArgumentList @(('"'+$uproject+'"'),'127.0.0.1:17866','-game','-nullrhi','-nosound','-unattended','-WeaponDropNetworkTest',('-abslog="'+$reportRoot+'/client.log"')) -WindowStyle Hidden -PassThru
 foreach($owned in $processes){if(!$owned.WaitForExit(65000)){throw 'Network test timed out'}}
 foreach($role in @('Host','Client')) {
  $report=Join-Path $projectRoot ('Saved/WeaponDropNetwork'+$role+'.txt')
  if(!(Test-Path $report) -or (Get-Item $report).LastWriteTime -lt $started){throw "$role produced no fresh report"}
  $result=Get-Content $report -Raw
  Copy-Item -LiteralPath $report -Destination $reportRoot
  Write-Output $result
  if(!$result.StartsWith('PASS')){throw "$role network test failed"}
 }
} finally {foreach($owned in $processes){if(!$owned.HasExited){Stop-Process -Id $owned.Id}}}

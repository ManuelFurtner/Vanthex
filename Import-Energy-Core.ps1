param(
 [string]$EngineRoot='H:\epic\UE_5.8',
 [string]$Blender=(Join-Path $PSScriptRoot 'Saved\Tools\blender-4.3.2-windows-x64\blender.exe'),
 [string]$Source=(Join-Path $PSScriptRoot 'Energy-542e5bc7\blender\source\p_146_MechanicalDoodle_004.blend')
)
$ErrorActionPreference='Stop'
if(!(Test-Path -LiteralPath $Source)){throw 'Licensed Energy source is missing. Obtain it locally through your own Fab entitlement.'}
if(!(Test-Path -LiteralPath $Blender)){throw 'Pass -Blender with the path to Blender 4.3 or newer.'}
& $Blender --background --disable-autoexec --python (Join-Path $PSScriptRoot 'Scripts\export_energy_core.py') -- $Source
if($LASTEXITCODE -ne 0){throw 'Blender Energy export failed'}
$energyProcess=$null
try {
 $energyArgs=@(('"'+(Join-Path $PSScriptRoot 'Vanthex.uproject')+'"'),'-run=VanthexImportEnergy','-nullrhi','-nosound','-unattended',('-abslog="'+(Join-Path $PSScriptRoot 'Saved\EnergyImport\import.log')+'"'))
 $energyProcess=Start-Process -FilePath (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') -ArgumentList $energyArgs -WindowStyle Hidden -PassThru
 if(!$energyProcess.WaitForExit(180000)){throw 'Energy import exceeded three minutes; see Saved/EnergyImport/import.log'}
 if($energyProcess.ExitCode -ne 0){throw 'Unreal Energy import failed'}
 Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Saved\EnergyImport\import.txt')
}finally{if($energyProcess -and !$energyProcess.HasExited){Stop-Process -InputObject $energyProcess}}

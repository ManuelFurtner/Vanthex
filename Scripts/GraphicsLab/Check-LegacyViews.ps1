$ErrorActionPreference='Stop'
$out='H:\Vanthex\Saved\GraphicsLab\Facility\LegacyViews'
New-Item -ItemType Directory -Force -Path $out | Out-Null
if(Get-Process UnrealEditor* -ErrorAction SilentlyContinue){throw 'Another Unreal process is active.'}
foreach($index in @(12,22)) {
    $run=Start-Process 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList "H:\Vanthex\Vanthex.uproject /Game/Vanthex/Maps/GridV4 -game -GridV4Shots -GridV4Shot=$index -RenderOffscreen -ResX=1920 -ResY=1080 -ForceRes -nosound -unattended -NoSplash -LocalDataCachePath=H:\Vanthex\Saved\GraphicsLab\DDC -DDC=InstalledNoZenLocalFallback -ExecCmds=`"r.VSync 0,t.MaxFPS 0,r.ScreenPercentage 100`" -abslog=$out\view_$index.log" -WindowStyle Hidden -PassThru
    $run.WaitForExit()
    if($run.ExitCode -ne 0){throw "Legacy view $index failed"}
    Get-ChildItem H:\Vanthex\Saved\GridV4Shots -Filter "$($index)_*.png" | Copy-Item -Destination $out
}
'LEGACY CAMERA CHECK COMPLETE'

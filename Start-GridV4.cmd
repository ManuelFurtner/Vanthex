@echo off
setlocal
set "VANTHEX_ENGINE=H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%VANTHEX_ENGINE%" (
 echo Unreal Engine 5.8 wurde unter H:\epic\UE_5.8 nicht gefunden.
 pause
 exit /b 1
)
"%VANTHEX_ENGINE%" "%~dp0Vanthex.uproject" /Game/Vanthex/Maps/GridV4 -game -windowed -ResX=1600 -ResY=900 %*

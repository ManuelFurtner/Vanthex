param([string]$EngineRoot = 'H:\epic\UE_5.8')
$ErrorActionPreference = 'Stop'
if (!(Test-Path -LiteralPath $EngineRoot)) {
    throw "Unreal Engine directory not found: $EngineRoot. Supply -EngineRoot."
}
$build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $PSScriptRoot 'Vanthex.uproject'
if (!(Test-Path -LiteralPath $build) -or !(Test-Path -LiteralPath $editor)) {
    throw "Unreal Engine 5.8 build tools not found at $EngineRoot. Supply -EngineRoot."
}
foreach ($target in @('VanthexEditor', 'Vanthex')) {
    & $build $target Win64 Development "-Project=$project" -WaitMutex
    if ($LASTEXITCODE -ne 0) { throw "$target build failed: $LASTEXITCODE" }
}
$started = Get-Date
$arguments = @(('"' + $project + '"'), '/Engine/Maps/Entry', '-game', '-nullrhi', '-nosound', '-unattended', '-VanthexSelfTest')
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
try {
    if (!$process.WaitForExit(150000)) { throw 'Equipment regression timed out' }
    if ($process.ExitCode -ne 0) { throw "Equipment regression failed: $($process.ExitCode)" }
    $report = Join-Path $PSScriptRoot 'Saved\VanthexSelfTest.txt'
    if (!(Test-Path -LiteralPath $report) -or (Get-Item -LiteralPath $report).LastWriteTime -lt $started) { throw 'Missing fresh report' }
    $lines = Get-Content -LiteralPath $report
    if ($lines[0] -ne 'OVERALL PASS' -or !($lines -match '^PASS RIFLE_DRAW_COMPLETES ')) { throw 'Equipment checks did not complete successfully' }
    $lines
} finally {
    if (!$process.HasExited) { Stop-Process -Id $process.Id -Force }
    $process.Dispose()
}

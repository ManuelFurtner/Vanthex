param([string]$EngineRoot = 'H:\epic\UE_5.8', [switch]$RefreshFeaturedShots)
$ErrorActionPreference = 'Stop'
if (!(Test-Path -LiteralPath $EngineRoot)) { throw "Unreal Engine not found at $EngineRoot; supply -EngineRoot." }
$build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $PSScriptRoot 'Vanthex.uproject'
foreach ($tool in @($build,$editor)) { if (!(Test-Path -LiteralPath $tool)) { throw "Missing tool: $tool" } }
foreach ($target in @('VanthexEditor','Vanthex')) {
    & $build $target Win64 Development "-Project=$project" -WaitMutex
    if ($LASTEXITCODE -ne 0) { throw "$target build failed" }
}
function Invoke-AudioMenuProcess([string[]]$Extra) {
    $arguments = @(('"' + $project + '"')) + $Extra + @('-unattended','-nop4','-nosplash','-nullrhi')
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    try {
        if (!$process.WaitForExit(180000)) { throw 'Audio/menu check timeout' }
        if ($process.ExitCode -ne 0) { throw "Audio/menu process failed: $($process.ExitCode)" }
    } finally {
        if (!$process.HasExited) { Stop-Process -Id $process.Id -Force }
        $process.Dispose()
    }
}
if ($RefreshFeaturedShots) {
    Invoke-AudioMenuProcess @('-run=VanthexBuildWeaponAudio','-RefreshFeaturedShots')
}
$report = Join-Path $PSScriptRoot ('Saved\AudioMenuAutomation-' + (Get-Date).ToString('yyyyMMdd-HHmmss'))
Invoke-AudioMenuProcess @('-ExecCmds="Automation RunTests Vanthex.Menu"','-TestExit="Automation Test Queue Empty"',('-ReportExportPath="' + $report + '"'))
$result = Join-Path $report 'index.json'
if (!(Test-Path -LiteralPath $result)) { throw 'Missing automation report' }
$data = Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
if ($data.failed -gt 0 -or $data.succeeded -lt 2) { throw 'Menu automation tests failed or did not run' }
Write-Output "PASS: editor/game builds and $($data.succeeded) menu automation tests. Rendered and mixer checks are separate."

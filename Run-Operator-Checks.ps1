param([string]$Engine = 'H:\epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe')
$ErrorActionPreference = 'Stop'
$operatorProject = Join-Path $PSScriptRoot 'Vanthex.uproject'
$operatorSaved = Join-Path $PSScriptRoot 'Saved'
New-Item -ItemType Directory -Path $operatorSaved -Force | Out-Null
$operatorReport = Join-Path $operatorSaved 'OperatorValidation.txt'
$operatorLog = Join-Path $operatorSaved 'Operator-validation.log'
$operatorStarted = Get-Date
$operatorProcess = $null
try {
    $operatorArguments = @(
        ('"' + $operatorProject + '"'), '/Game/Vanthex/Maps/GridV3',
        '-game', '-OperatorTest', '-nullrhi', '-nosound', '-unattended',
        '-ExecCmds="t.MaxFPS 60"', ('-abslog="' + $operatorLog + '"')
    )
    $operatorProcess = Start-Process -FilePath $Engine -ArgumentList $operatorArguments -WindowStyle Hidden -PassThru
    $operatorDeadline = (Get-Date).AddSeconds(90)
    while (-not $operatorProcess.HasExited -and (Get-Date) -lt $operatorDeadline) { Start-Sleep -Seconds 1 }
    if (-not $operatorProcess.HasExited) { throw 'Operator validation timed out.' }
    $operatorProcess.Refresh()
    if ($operatorProcess.ExitCode -ne 0) { throw ('Operator process failed: exit ' + $operatorProcess.ExitCode) }
    if (!(Test-Path -LiteralPath $operatorReport) -or (Get-Item -LiteralPath $operatorReport).LastWriteTime -lt $operatorStarted) {
        throw 'Missing fresh OperatorValidation.txt report.'
    }
    $operatorResult = Get-Content -LiteralPath $operatorReport -Raw
    if ($operatorResult -notmatch '^OVERALL PASS') { throw ('Operator validation failed:' + [Environment]::NewLine + $operatorResult) }
    Write-Output $operatorResult
} finally {
    if ($operatorProcess -and -not $operatorProcess.HasExited) { Stop-Process -InputObject $operatorProcess }
}

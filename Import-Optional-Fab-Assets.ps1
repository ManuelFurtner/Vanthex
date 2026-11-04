<#
.SYNOPSIS
Imports locally licensed Fab weapon/audio packs without changing the public fallback assets.
.EXAMPLE
.\Import-Optional-Fab-Assets.ps1 -WhatIf
.EXAMPLE
.\Import-Optional-Fab-Assets.ps1 -EngineRoot 'H:\epic\UE_5.8'
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param([string]$EngineRoot = 'H:\epic\UE_5.8')

$ErrorActionPreference = 'Stop'
$fabProject = Join-Path $PSScriptRoot 'Vanthex.uproject'
$fabSaved = Join-Path $PSScriptRoot 'Saved'
if (!(Test-Path -LiteralPath $fabProject -PathType Leaf)) { throw "Project missing: $fabProject" }
$fabProjectInfo = Get-Content -LiteralPath $fabProject -Raw | ConvertFrom-Json
$fabJobs = @()
if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'Content\FPS_Weapon_Bundle') -PathType Container) {
    $fabJobs += [pscustomobject]@{ Name = 'VanthexImportWeaponBundle'; Report = 'WeaponBundle-import.txt' }
} else { Write-Output 'FPS_Weapon_Bundle absent: keeping the complete authored weapon models.' }
if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'Content\FreeWeaponSFX') -PathType Container) {
    $fabJobs += [pscustomobject]@{ Name = 'VanthexImportFabAudio'; Report = 'FabAudioImport.txt' }
} else { Write-Output 'FreeWeaponSFX absent: keeping the project-owned weapon audio.' }
if ($fabJobs.Count -eq 0) { Write-Output 'No optional packs installed; nothing to import.'; return }

$fabCandidates = @($EngineRoot)
if (!$PSBoundParameters.ContainsKey('EngineRoot')) {
    $fabAssociation = [string]$fabProjectInfo.EngineAssociation
    foreach ($fabRegistryPath in @(
        "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$fabAssociation",
        "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$fabAssociation"
    )) {
        $fabInstalled = Get-ItemProperty -LiteralPath $fabRegistryPath -Name InstalledDirectory -ErrorAction SilentlyContinue
        if ($fabInstalled -and $fabInstalled.InstalledDirectory) { $fabCandidates += $fabInstalled.InstalledDirectory }
    }
    $fabCustomBuilds = Get-ItemProperty -LiteralPath 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue
    if ($fabCustomBuilds) {
        $fabCustom = $fabCustomBuilds.PSObject.Properties[$fabAssociation]
        if ($fabCustom -and $fabCustom.Value) { $fabCandidates += [string]$fabCustom.Value }
    }
}
$fabEditor = $null
$fabSelectedEngineRoot = $null
foreach ($fabCandidate in $fabCandidates) {
    if ([string]::IsNullOrWhiteSpace($fabCandidate)) { continue }
    $fabExecutable = Join-Path $fabCandidate 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    if (Test-Path -LiteralPath $fabExecutable -PathType Leaf) {
        $fabEditor = (Resolve-Path -LiteralPath $fabExecutable).Path
        $fabSelectedEngineRoot = $fabCandidate
        break
    }
}
if (!$fabEditor) { throw 'UnrealEditor-Cmd.exe not found. Supply -EngineRoot with the matching Unreal Engine installation.' }
$fabBuildVersionPath = Join-Path $fabSelectedEngineRoot 'Engine\Build\Build.version'
if (Test-Path -LiteralPath $fabBuildVersionPath -PathType Leaf) {
    $fabVersion = Get-Content -LiteralPath $fabBuildVersionPath -Raw | ConvertFrom-Json
    $fabEngineVersion = "$($fabVersion.MajorVersion).$($fabVersion.MinorVersion)"
    if ([string]$fabProjectInfo.EngineAssociation -match '^\d+\.\d+$' -and $fabEngineVersion -ne [string]$fabProjectInfo.EngineAssociation) {
        throw "Engine $fabEngineVersion does not match project association $($fabProjectInfo.EngineAssociation). Supply the matching -EngineRoot."
    }
}
if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'Binaries\Win64\UnrealEditor-Vanthex.dll') -PathType Leaf)) {
    throw 'Build the VanthexEditor Development Win64 target before importing. This script does not build the project.'
}

function Assert-FabEditorClosed {
    $fabRunning = @(Get-Process -Name 'UnrealEditor*' -ErrorAction SilentlyContinue)
    if ($fabRunning.Count -gt 0) {
        $fabIds = ($fabRunning | ForEach-Object { "$($_.ProcessName) (PID $($_.Id))" }) -join ', '
        throw "Close Unreal Editor and wait for existing commandlets to finish before importing: $fabIds. No process was stopped."
    }
}

Write-Output "Project: $fabProject"
Write-Output "Engine:  $fabEditor"
foreach ($fabJob in $fabJobs) {
    if (!$PSCmdlet.ShouldProcess($fabProject, "Run $($fabJob.Name), writing only local derived assets under Content/Vanthex/Imported")) { continue }
    Assert-FabEditorClosed
    New-Item -ItemType Directory -Path $fabSaved -Force | Out-Null
    $fabStarted = Get-Date
    $fabLog = Join-Path $fabSaved ("OptionalFab-$($fabJob.Name)-" + $fabStarted.ToString('yyyyMMdd-HHmmss-fff') + '.log')
    $fabArguments = @(
        ('"' + $fabProject + '"'), ('-run=' + $fabJob.Name),
        '-unattended', '-nop4', '-nosplash', '-NullRHI', ('-abslog="' + $fabLog + '"')
    )
    Write-Output "Running $($fabJob.Name). Log: $fabLog"
    $fabProcess = Start-Process -FilePath $fabEditor -ArgumentList $fabArguments -WindowStyle Hidden -PassThru -Wait
    try {
        $fabProcess.Refresh()
        if ($fabProcess.ExitCode -ne 0) { throw "$($fabJob.Name) exited $($fabProcess.ExitCode). Inspect $fabLog" }
    } finally { $fabProcess.Dispose() }
    $fabReport = Join-Path $fabSaved $fabJob.Report
    if (!(Test-Path -LiteralPath $fabReport -PathType Leaf) -or (Get-Item -LiteralPath $fabReport).LastWriteTime -lt $fabStarted) {
        throw "Importer did not produce a fresh report: $fabReport. Inspect $fabLog"
    }
    $fabResult = Get-Content -LiteralPath $fabReport -Raw
    if ($fabResult -notmatch '^OVERALL PASS\b' -or $fabResult -match '(?m)^FAIL\b') { throw "Import failed: $fabReport`n$fabResult" }
    Write-Output "PASS $($fabJob.Name): $fabReport"
}
if (!$WhatIfPreference) {
    Write-Output 'Optional imports completed. Start a new game/editor process to load them. Licensed source and derived assets remain local and Git-ignored.'
}

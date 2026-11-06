#Requires -Version 7.2
[CmdletBinding()]
param(
    [string]$ImageTag = 'vanthex-server:local',
    [string]$EngineImage = 'ghcr.io/epicgames/unreal-engine:dev-slim-5.8.3@sha256:07ae3f48e858cda09c906aa6fe7a9665213bd2d7572c823877f71a458a04bd29',
    [switch]$ExportPackage
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$docker = Get-Command docker -ErrorAction SilentlyContinue
if (!$docker) {
    $dockerPath = Join-Path $env:LOCALAPPDATA 'Programs/DockerDesktop/resources/bin/docker.exe'
    if (!(Test-Path -LiteralPath $dockerPath)) { throw 'Docker CLI not found.' }
    $env:PATH = (Split-Path $dockerPath) + ';' + $env:PATH
    $docker = Get-Command docker
}
$dockerPath = $docker.Source
$os = & $dockerPath info --format '{{.OSType}}'
if ($LASTEXITCODE -ne 0 -or $os -ne 'linux') { throw 'A running Linux Docker engine is required.' }

# The public repository deliberately omits local Fab assets. Build from the full
# licensed local project, not a bare GitHub clone with missing asset references.
$required = @(
    'Vanthex.uproject', 'Source/VanthexServer.Target.cs',
    'Content/Vanthex/Maps/GridV4.umap',
    'Content/Vanthex/Imported', 'Content/Vanthex/Art/IndustrialFinish'
)
foreach ($relative in $required) {
    if (!(Test-Path -LiteralPath (Join-Path $projectRoot $relative))) {
        throw "Missing local build input: $relative. Restore licensed assets before cooking."
    }
}
$map = Join-Path $projectRoot 'Content/Vanthex/Maps/GridV4.umap'
$mapStream = [IO.File]::OpenRead($map)
try {
    $bytes = New-Object byte[] 64
    $read = $mapStream.Read($bytes, 0, $bytes.Length)
    if ([Text.Encoding]::ASCII.GetString($bytes, 0, $read).StartsWith('version https://git-lfs.github.com/')) {
        throw 'GridV4 is only a Git LFS pointer. Run git lfs pull first.'
    }
} finally { $mapStream.Dispose() }

$logRoot = Join-Path $projectRoot 'Saved/LinuxServer'
New-Item -ItemType Directory -Path $logRoot -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
# Keep the large Unreal base as a named engine image. BuildKit's default cache
# budget is smaller than this image and can otherwise discard it after a failure.
& $dockerPath image inspect $EngineImage *> $null
if ($LASTEXITCODE -ne 0) {
    & $dockerPath pull --platform linux/amd64 $EngineImage 2>&1 |
        Tee-Object -FilePath (Join-Path $logRoot "engine-pull-$stamp.log")
    if ($LASTEXITCODE -ne 0) { throw 'Could not download and retain the pinned Unreal build image.' }
}
& $dockerPath tag $EngineImage 'vanthex-unreal-build:5.8.3'
if ($LASTEXITCODE -ne 0) { throw 'Could not retain the local Unreal build image tag.' }
$argsList = @('build', '--platform', 'linux/amd64', '--progress=plain',
    '--file', (Join-Path $PSScriptRoot 'Dockerfile'), '--build-arg', "UE_IMAGE=$EngineImage")
if ($ExportPackage) {
    $argsList += @('--target', 'package', '--output', "type=local,dest=$logRoot/Package")
} else {
    $argsList += @('--target', 'runtime', '--tag', $ImageTag)
}
$argsList += $projectRoot
& $dockerPath @argsList 2>&1 | Tee-Object -FilePath (Join-Path $logRoot "build-$stamp.log")
$buildExit = $LASTEXITCODE
if ($buildExit -ne 0) { throw "Linux server build failed ($buildExit). See $logRoot." }
Write-Host 'Linux server build completed. A build alone does not verify client connectivity.'

#
#  capture-screenshots.ps1
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-10.
#

<#
.SYNOPSIS
screenshots/capture-standard.md GL capture runner (Windows twin).

.DESCRIPTION
Thin wrapper around the GL host's own --deterministic --capture CLI
(samples/legacy-samples/shared/demo_host.cpp): no C++ change was made for this (C-007). This
script only invokes the host and writes one <frame>.sidecar.json per saved PNG, per
screenshots/capture-standard.md's schema. Mirrors capture-screenshots.sh argument for
argument.

.PARAMETER Out
Directory this run writes <preset-id>/frame_NNNN.png and <preset-id>/frame_NNNN.sidecar.json
into. Required.

.PARAMETER Exe
The GL host for this platform. Default:
build\windows-gl-dev\samples\legacy-samples\synesthesia-vibe-demo-windows-gl\Release\synesthesia-vibe-demo-windows-gl.exe

.PARAMETER CaptureSet
Path to the checked-in capture-set definition. Default: tests\golden\capture-set.json
(repo-root relative). Source of truth for preset id -> preset file, and the default WAV.

.PARAMETER Presets
Comma-separated capture ids to run. Default: every preset in -CaptureSet.

.PARAMETER SampleFrames
Comma-separated frame numbers to save and record. Default: 0,30,60,150,300,450,599
(screenshots/capture-standard.md's fixed frame list).

.PARAMETER Frames
How many frames the host renders. Default: 600, so the highest sample frame (599) is
always produced.

.PARAMETER Wav
WAV path, repo-root relative. Default: -CaptureSet's "wav" field.

.PARAMETER Build
Sidecar "build" field (e.g. a CMake preset name). Default: the -Exe path's own build-dir
segment (build\<this>\...), or "unknown".

.PARAMETER Platform
Sidecar "platform" field. Default: "windows-x86" or "windows-<arch>" (auto-detected).

.PARAMETER CaptureTimeoutSeconds
Per-preset wall-clock budget, in seconds. Default: 700.

.PARAMETER NonDeterministic
Mark every sidecar "deterministic": false instead of true. Only for a capture known to
not meet the standard's determinism bar (C-006) -- the GL host's --capture mode is itself
always deterministic, so this is not needed for a normal run.
#>

param(
    [string]$Out,
    [string]$Exe = "build\windows-gl-dev\samples\legacy-samples\synesthesia-vibe-demo-windows-gl\Release\synesthesia-vibe-demo-windows-gl.exe",
    [string]$CaptureSet = "tests\golden\capture-set.json",
    [string]$Presets,
    [string]$SampleFrames = "0,30,60,150,300,450,599",
    [int]$Frames = 600,
    [string]$Wav,
    [string]$Build,
    [string]$Platform,
    [int]$CaptureTimeoutSeconds = 700,
    [switch]$NonDeterministic
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path

function Resolve-RepoPath
{
    param([string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) { return $Path }
    return (Join-Path $RepoRoot $Path)
}

if (-not $Out) { Write-Error "Error: -Out is required"; exit 2 }

$CaptureSet = Resolve-RepoPath $CaptureSet
$Out = Resolve-RepoPath $Out
$Exe = Resolve-RepoPath $Exe

foreach ($required in @($CaptureSet, $Exe))
{
    if (-not (Test-Path $required)) { Write-Host "Not found: $required"; exit 2 }
}

$sampleFrameList = @()
foreach ($part in ($SampleFrames -split ","))
{
    $trimmed = $part.Trim()
    if ($trimmed -eq "") { continue }
    if ($trimmed -notmatch '^[0-9]+$')
    {
        Write-Error "-SampleFrames must be a comma-separated list of non-negative frame numbers."
        exit 2
    }
    $sampleFrameList += [int]$trimmed
}
if ($sampleFrameList.Count -eq 0) { Write-Error "-SampleFrames parsed to an empty frame list."; exit 2 }
$sampleFramesArg = ($sampleFrameList -join ",")

$set = Get-Content -Raw -Path $CaptureSet | ConvertFrom-Json
if ($Wav) { $wavPath = Resolve-RepoPath $Wav } else { $wavPath = Join-Path $RepoRoot $set.wav }
if (-not (Test-Path $wavPath)) { Write-Host "WAV not found: $wavPath"; exit 2 }
$presetRoot = Join-Path $RepoRoot (Join-Path "ref-src\beatdrop\BeatDrop" $set.presetPathPrefix)
$workingDirectory = Split-Path -Parent $Exe

$selected = New-Object System.Collections.Generic.List[object]
if ($Presets)
{
    $requested = $Presets -split ","
    foreach ($preset in $set.presets)
    {
        if ($requested -contains $preset.id) { $selected.Add($preset) }
    }
}
else
{
    foreach ($preset in $set.presets) { $selected.Add($preset) }
}
if ($selected.Count -eq 0) { Write-Error "-Presets matched no capture ids."; exit 2 }

if (-not $Build)
{
    if ($Exe -match '[\\/]build[\\/]([^\\/]+)[\\/]')
    {
        $Build = $Matches[1]
    }
    else
    {
        $Build = "unknown"
    }
}

if (-not $Platform)
{
    $arch = $env:PROCESSOR_ARCHITECTURE
    $Platform = "windows-$arch".ToLowerInvariant()
}

function Get-RepoRelativePath
{
    param([string]$Path)
    $full = (Resolve-Path $Path).Path
    if ($full.StartsWith($RepoRoot))
    {
        return $full.Substring($RepoRoot.Length).TrimStart('\', '/').Replace('\', '/')
    }
    return $full.Replace('\', '/')
}

function Read-PngDimensions
{
    param([string]$Path)
    $stream = [System.IO.File]::OpenRead($Path)
    try
    {
        $reader = New-Object System.IO.BinaryReader($stream)
        $null = $reader.ReadBytes(16)
        $header = $reader.ReadBytes(8)
        $width = ([int]$header[0] -shl 24) -bor ([int]$header[1] -shl 16) -bor ([int]$header[2] -shl 8) -bor $header[3]
        $height = ([int]$header[4] -shl 24) -bor ([int]$header[5] -shl 16) -bor ([int]$header[6] -shl 8) -bor $header[7]
        return @{ width = [int]$width; height = [int]$height }
    }
    finally
    {
        $stream.Dispose()
    }
}

$expectedWidth = 838
$expectedHeight = 441

$gitDescribe = (git -C $RepoRoot describe --always --dirty 2>$null)
if (-not $gitDescribe) { $gitDescribe = "unknown" }
$gitDirty = $gitDescribe -match '-dirty$'
$gitRef = (git -C $RepoRoot rev-parse --abbrev-ref HEAD 2>$null)
if (-not $gitRef) { $gitRef = "unknown" }
$hostName = [System.IO.Path]::GetFileNameWithoutExtension($Exe)
$wavSha256 = (Get-FileHash -Algorithm SHA256 -Path $wavPath).Hash.ToLowerInvariant()
$wavRepoRelative = Get-RepoRelativePath $wavPath
$deterministicValue = -not $NonDeterministic

New-Item -ItemType Directory -Force -Path $Out | Out-Null

$captureFailed = $false

foreach ($preset in $selected)
{
    $id = $preset.id
    $captureOut = Join-Path $Out $id
    Write-Host "[$id] frames=$Frames save-frames=$sampleFramesArg"

    if (Test-Path $captureOut) { Remove-Item -Recurse -Force $captureOut }
    New-Item -ItemType Directory -Force -Path $captureOut | Out-Null

    $presetPath = Join-Path $presetRoot $preset.file
    $argString = '--deterministic --capture "{0}" "{1}" --frames {2} --out "{3}" --save-frames {4} --no-json' -f `
        $presetPath, $wavPath, $Frames, $captureOut, $sampleFramesArg

    $process = Start-Process -FilePath $Exe -ArgumentList $argString -WorkingDirectory $workingDirectory `
        -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $captureOut "_process.log.stdout.txt") `
        -RedirectStandardError (Join-Path $captureOut "_process.log.stderr.txt")

    if (-not $process.WaitForExit($CaptureTimeoutSeconds * 1000))
    {
        & taskkill.exe /PID $process.Id /T /F 2>$null | Out-Null
        Write-Host "  [TIMEOUT] capture did not finish within ${CaptureTimeoutSeconds}s"
        $captureFailed = $true
        continue
    }

    Write-Host "  exitcode=$($process.ExitCode) (informational; sampled-frame completeness below is the verdict)"

    $missing = @()
    foreach ($frame in $sampleFrameList)
    {
        $frameName = "frame_{0:D4}" -f $frame
        $path = Join-Path $captureOut "$frameName.png"
        if (-not (Test-Path $path)) { $missing += "$frameName.png (missing)" }
        elseif ((Get-Item $path).Length -eq 0) { $missing += "$frameName.png (empty)" }
    }
    if ($missing.Count -gt 0)
    {
        $captureFailed = $true
        Write-Host "  INCOMPLETE -- missing/empty sampled frames: $($missing -join ', ')"
        continue
    }

    $stdoutLog = Join-Path $captureOut "_process.log.stdout.txt"
    $wavLine = Select-String -Path $stdoutLog -Pattern ': wav="' -SimpleMatch:$false | Select-Object -First 1
    $rate = 44100
    $channels = 2
    if ($wavLine -and $wavLine.Line -match 'rate=(\d+)') { $rate = [int]$Matches[1] }
    if ($wavLine -and $wavLine.Line -match 'channels=(\d+)') { $channels = [int]$Matches[1] }

    $firstFrameName = "frame_{0:D4}" -f $sampleFrameList[0]
    $dimensions = Read-PngDimensions (Join-Path $captureOut "$firstFrameName.png")
    if ($dimensions.width -ne $expectedWidth -or $dimensions.height -ne $expectedHeight)
    {
        $captureFailed = $true
        Write-Host "  DIMENSION MISMATCH -- $firstFrameName.png is $($dimensions.width)x$($dimensions.height), expected ${expectedWidth}x${expectedHeight}"
        continue
    }

    foreach ($frame in $sampleFrameList)
    {
        $frameName = "frame_{0:D4}" -f $frame
        $sampleOffset = $frame * 735
        $timeSeconds = $frame / 60.0

        $sidecar = [ordered]@{
            frame = $frame
            width = $dimensions.width
            height = $dimensions.height
            time_seconds = $timeSeconds
            audio = [ordered]@{
                path = $wavRepoRelative
                sha256 = $wavSha256
                rate = $rate
                channels = $channels
                sample_offset = $sampleOffset
            }
            build = $Build
            host = $hostName
            platform = $Platform
            git = $gitDescribe
            ref = $gitRef
            dirty = $gitDirty
            seed = 20260927
            deterministic = $deterministicValue
        }

        $sidecar | ConvertTo-Json -Depth 5 | Set-Content -Path (Join-Path $captureOut "$frameName.sidecar.json")
    }

    Write-Host "  OK -- wrote $captureOut\{$sampleFramesArg}.{png,sidecar.json}"
}

if ($captureFailed)
{
    Write-Error "One or more presets failed to produce every sampled frame."
    exit 1
}

Write-Host ""
Write-Host "Done: $Out"
exit 0

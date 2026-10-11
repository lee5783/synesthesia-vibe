#
#  run-visual-check.ps1
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-08.
#

param(
    [string]$CaptureSet = "tests\golden\capture-set.json",
    [string]$Reference = "tests\golden\corpus",
    [string]$Out,
    [string]$Exe = "build\windows-gl-dev\samples\legacy-samples\synesthesia-vibe-demo-windows-gl\Release\synesthesia-vibe-demo-windows-gl.exe",
    [string]$StateCompareExe,
    [string]$FrameCompareExe,
    [string[]]$Presets = @(
        "preset-cat-scan",
        "preset-dark-side-of-the-moon",
        "preset-now-and-later",
        "preset-fractopia-upspoken-mix",
        "preset-jesters-calling-2",
        "preset-fuzzy-sciences",
        "preset-shifter-swarm"
    ),
    [string]$SampleFrames = "0,30,60,150,300,450,599",
    [int]$Frames = 600,
    [int]$CaptureTimeoutSeconds = 700,
    [switch]$SkipCapture,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Get-HelpText
{
    return @"
run-visual-check.ps1 -- W-1 visual check runner (Windows wrapper)

USAGE

  Thin wrapper around the CMake-built tests/tools/state-compare tool (C2-020: all
  SSIM, composite and Markdown logic lives in the C++ tool, not here). Renders the
  7 presetver-100 presets through the GL host with a sampled, PNG-only capture
  (--save-frames, --no-json), then asks state-compare's 'visual' subcommand to
  pair each sampled frame against the golden corpus, write a
  golden | GL | amplified-abs-diff composite per pair and write report.md.

    .\run-visual-check.ps1 -Out <dir> [-Reference <dir>] [-CaptureSet <capture-set.json>] ``
        [-Exe <exe>] [-StateCompareExe <exe>] [-FrameCompareExe <exe>] ``
        [-Presets id1,id2,...] [-SampleFrames N,N,...] [-Frames N] [-SkipCapture]

PARAMETERS

  -CaptureSet        Path to the checked-in capture-set definition. Default:
                     tests\golden\capture-set.json (repo-root relative). Used for
                     the preset id -> preset file mapping only.
  -Reference         Directory of golden frame directories to compare against.
                     Default: tests\golden\corpus.
  -Out               Directory this run writes captures, composites and report.md
                     into. Required.
  -Exe               The Windows GL host. Default:
                     build\windows-gl-dev\samples\legacy-samples\synesthesia-vibe-demo-windows-gl\Release\synesthesia-vibe-demo-windows-gl.exe
  -StateCompareExe   state-compare.exe (this tool). If omitted, this script looks
                     in the usual build output locations under build\.
  -FrameCompareExe   frame-compare.exe, the SSIM engine and composite writer
                     state-compare spawns once per frame pair. If omitted, this
                     script looks in the usual build output locations under build\.
  -Presets           Capture ids to check. Default: the 7 presetver-100 presets
                     of the Phase 2 matrix.
  -SampleFrames      Comma-separated frame numbers to save and compare. Default:
                     0,30,60,150,300,450,599. Captures are always sampled, never
                     600 PNGs (Phase 2 constraint).
  -Frames            How many frames each capture renders. Default: 600, matching
                     the golden corpus length so frame numbering lines up.
  -CaptureTimeoutSeconds
                     Per-attempt wall-clock budget for a single capture
                     invocation, in seconds. Default: 700.
  -SkipCapture       Skip invoking the GL host entirely and compare whatever
                     already exists under -Out/<id>/. Used to re-run the report
                     without paying capture time.

EXIT CODE

  0  state-compare's 'visual' subcommand compared every sampled frame.
  1  a capture timed out / came back incomplete, or state-compare reported a
     frame error (e.g. a sampled PNG missing from a capture).
  2  usage error, or a required tool, capture set, WAV or reference path was
     absent.

  The capture process's own exit code is never the pass/fail signal (P0-007:
  a capture-mode teardown crash after every frame is written is a known issue);
  sampled-frame completeness and state-compare's verdict are.

  SSIM is REPORTED, never gated (CW-005 recommendation (a)): a low score is not
  a failure of this script. Scores below the C2-011 0.98/0.95 gate are expected for
  audio-reactive feedback presets, where small numeric differences grow with frame number.
"@
}

if ($Help)
{
    Write-Host (Get-HelpText)
    exit 0
}

function Get-RepoRoot
{
    return (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
}

function Resolve-RepoPath
{
    param([string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) { return $Path }
    return (Join-Path $RepoRoot $Path)
}

function Find-BuiltExe
{
    param([string]$Name)

    $candidates = @(
        (Join-Path $RepoRoot "build\windows-gl-dev\tests\tools\$Name\Release\$Name.exe"),
        (Join-Path $RepoRoot "build\x86-release\tests\tools\$Name\Release\$Name.exe"),
        (Join-Path $RepoRoot "build\windows-gl-dev\tests\tools\$Name\$Name.exe"),
        (Join-Path $RepoRoot "build\x86-release\tests\tools\$Name\$Name.exe"),
        (Join-Path $RepoRoot "build\windows-gl-dev\tests\tools\$Name\Debug\$Name.exe"),
        (Join-Path $RepoRoot "build\x86-release\tests\tools\$Name\Debug\$Name.exe")
    )
    return ($candidates | Where-Object { Test-Path $_ } | Select-Object -First 1)
}

function Get-CaptureList
{
    param([string]$CaptureSetPath)

    $captureSet = Get-Content -Raw -Path $CaptureSetPath | ConvertFrom-Json
    $wav = $captureSet.wav
    $prefix = $captureSet.presetPathPrefix
    $defaultFrames = $captureSet.defaultFrames

    $captures = New-Object System.Collections.Generic.List[object]

    foreach ($preset in $captureSet.presets)
    {
        $captures.Add([pscustomobject]@{
            Id         = $preset.id
            PresetFile = $preset.file
        })
    }

    return [pscustomobject]@{
        Wav      = $wav
        Prefix   = $prefix
        Frames   = $defaultFrames
        Captures = $captures
    }
}

function Split-SampleFrames
{
    param([string]$Value)

    $frames = New-Object System.Collections.Generic.List[int]
    foreach ($part in ($Value -split ','))
    {
        $trimmed = $part.Trim()
        if ($trimmed -eq "") { continue }
        $frames.Add([int]$trimmed)
    }
    return [int[]]$frames
}

function ConvertTo-QuotedArgumentString
{
    param([string[]]$ProcessArgs)

    $quoted = foreach ($a in $ProcessArgs)
    {
        '"' + ($a -replace '"', '""') + '"'
    }
    return ($quoted -join " ")
}

function Invoke-CaptureProcessOnce
{
    param(
        [string]$ExePath,
        [string]$ExeProcessName,
        [string]$ArgumentString,
        [string]$WorkingDirectory,
        [string]$StdoutPath,
        [string]$StderrPath,
        [int]$TimeoutSeconds
    )

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $ExePath -ArgumentList $ArgumentString -WorkingDirectory $WorkingDirectory `
        -PassThru -WindowStyle Hidden -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath

    $exited = $process.WaitForExit($TimeoutSeconds * 1000)
    $stopwatch.Stop()

    if (-not $exited)
    {
        & taskkill.exe /PID $process.Id /T /F 2>$null | Out-Null

        return [pscustomobject]@{
            TimedOut    = $true
            ExitCode    = $null
            WallSeconds = [math]::Round($stopwatch.Elapsed.TotalSeconds, 2)
        }
    }

    return [pscustomobject]@{
        TimedOut    = $false
        ExitCode    = $process.ExitCode
        WallSeconds = [math]::Round($stopwatch.Elapsed.TotalSeconds, 2)
    }
}

function Invoke-CaptureProcess
{
    param(
        [string]$ExePath,
        [string[]]$ProcessArgs,
        [string]$LogPath,
        [int]$TimeoutSeconds
    )

    $argumentString = ConvertTo-QuotedArgumentString -ProcessArgs $ProcessArgs
    $workingDirectory = Split-Path -Parent $ExePath
    $exeProcessName = [System.IO.Path]::GetFileNameWithoutExtension($ExePath)
    $stdoutPath = "$LogPath.stdout.txt"
    $stderrPath = "$LogPath.stderr.txt"

    $attempt1 = Invoke-CaptureProcessOnce -ExePath $ExePath -ExeProcessName $exeProcessName -ArgumentString $argumentString `
        -WorkingDirectory $workingDirectory -StdoutPath $stdoutPath -StderrPath $stderrPath -TimeoutSeconds $TimeoutSeconds

    "(stdout/stderr redirected to $stdoutPath / $stderrPath)" | Set-Content -Path $LogPath -Encoding utf8

    if (-not $attempt1.TimedOut)
    {
        return [pscustomobject]@{
            ExitCode    = $attempt1.ExitCode
            WallSeconds = $attempt1.WallSeconds
            TimedOut    = $false
        }
    }

    Write-Host "  [TIMEOUT] attempt 1 exceeded ${TimeoutSeconds}s"
    return [pscustomobject]@{
        ExitCode    = $null
        WallSeconds = $attempt1.WallSeconds
        TimedOut    = $true
    }
}

function Test-SampledFrameSet
{
    param([string]$CaptureOutDir, [int[]]$SampleFrames)

    $missing = New-Object System.Collections.Generic.List[string]
    foreach ($frame in $SampleFrames)
    {
        $frameName = "frame_{0:D4}" -f $frame
        $path = Join-Path $CaptureOutDir "$frameName.png"
        if (-not (Test-Path $path))
        {
            $missing.Add("$frameName.png (missing)")
        }
        elseif ((Get-Item $path).Length -eq 0)
        {
            $missing.Add("$frameName.png (empty)")
        }
    }

    return [pscustomobject]@{
        Complete = ($missing.Count -eq 0)
        Missing  = $missing
    }
}

$RepoRoot = Get-RepoRoot

if (-not $Out) { Write-Error "Error: -Out is required"; exit 2 }
if ($Frames -lt 1) { Write-Error "Error: -Frames must be positive"; exit 2 }
if ($CaptureTimeoutSeconds -lt 1) { Write-Error "Error: -CaptureTimeoutSeconds must be positive"; exit 2 }

$CaptureSet = Resolve-RepoPath $CaptureSet
$Reference = Resolve-RepoPath $Reference
$Out = Resolve-RepoPath $Out
$Exe = Resolve-RepoPath $Exe

if (-not $StateCompareExe) { $StateCompareExe = Find-BuiltExe -Name "state-compare" }
if (-not $FrameCompareExe) { $FrameCompareExe = Find-BuiltExe -Name "frame-compare" }
if ($StateCompareExe) { $StateCompareExe = Resolve-RepoPath $StateCompareExe }
if ($FrameCompareExe) { $FrameCompareExe = Resolve-RepoPath $FrameCompareExe }

$sampleFrameList = Split-SampleFrames -Value $SampleFrames
if ($sampleFrameList.Count -eq 0)
{
    Write-Host "Error: -SampleFrames parsed to an empty frame list"
    exit 2
}

if (-not (Test-Path $CaptureSet))
{
    Write-Host "Capture set not found: $CaptureSet"
    exit 2
}
if (-not (Test-Path $Reference))
{
    Write-Host "Reference directory not found: $Reference"
    exit 2
}
if (-not $StateCompareExe -or -not (Test-Path $StateCompareExe))
{
    Write-Host "state-compare.exe not found. Build it first (-Help), or pass -StateCompareExe:"
    Write-Host "  cmake --build build\windows-gl-dev --config Release --target state-compare"
    exit 2
}
if (-not $FrameCompareExe -or -not (Test-Path $FrameCompareExe))
{
    Write-Host "frame-compare.exe not found. Pass -FrameCompareExe, e.g.:"
    Write-Host "  build\windows-gl-dev\tests\tools\frame-compare\Release\frame-compare.exe"
    exit 2
}
if (-not $SkipCapture -and -not (Test-Path $Exe))
{
    Write-Host "GL host not found: $Exe"
    Write-Host "Pass -Exe, or use -SkipCapture to compare an existing capture directory."
    exit 2
}

$captureSetData = Get-CaptureList -CaptureSetPath $CaptureSet
$wavAbs = Resolve-RepoPath $captureSetData.Wav
if (-not $SkipCapture -and -not (Test-Path $wavAbs))
{
    Write-Host "WAV not found: $wavAbs"
    exit 2
}

$selected = $captureSetData.Captures | Where-Object { $Presets -contains $_.Id }
if (-not $selected -or $selected.Count -eq 0)
{
    Write-Host "Error: -Presets matched no capture ids"
    exit 2
}

if (-not (Test-Path $Out)) { New-Item -ItemType Directory -Force -Path $Out | Out-Null }

$sampleFramesArg = ($sampleFrameList -join ",")
$captureFailed = $false
$subsetIds = New-Object System.Collections.Generic.List[string]

foreach ($capture in $selected)
{
    $captureOutDir = Join-Path $Out $capture.Id
    $subsetIds.Add($capture.Id)

    Write-Host "[$($capture.Id)] frames=$Frames save-frames=$sampleFramesArg"

    if (-not $SkipCapture)
    {
        if (Test-Path $captureOutDir) { Remove-Item -Recurse -Force $captureOutDir }
        New-Item -ItemType Directory -Force -Path $captureOutDir | Out-Null

        $presetArg = "$($captureSetData.Prefix)\$($capture.PresetFile)"
        $processArgs = New-Object System.Collections.Generic.List[string]
        $processArgs.Add("--capture")
        $processArgs.Add($presetArg)
        $processArgs.Add($wavAbs)
        $processArgs.Add("--frames")
        $processArgs.Add("$Frames")
        $processArgs.Add("--out")
        $processArgs.Add($captureOutDir)
        $processArgs.Add("--save-frames")
        $processArgs.Add($sampleFramesArg)
        $processArgs.Add("--no-json")

        $logPath = Join-Path $captureOutDir "_process.log"
        $invokeResult = Invoke-CaptureProcess -ExePath $Exe -ProcessArgs $processArgs -LogPath $logPath `
            -TimeoutSeconds $CaptureTimeoutSeconds

        if ($invokeResult.TimedOut)
        {
            Write-Host "  [TIMEOUT] capture did not finish within ${CaptureTimeoutSeconds}s"
            $captureFailed = $true
            continue
        }

        Write-Host "  exitcode=$($invokeResult.ExitCode) wall=$($invokeResult.WallSeconds)s (informational only -- see EXIT CODE in -Help)"
    }
    else
    {
        if (-not (Test-Path $captureOutDir))
        {
            Write-Host "Error: -SkipCapture given but $captureOutDir does not exist"
            exit 2
        }
    }

    $frameSet = Test-SampledFrameSet -CaptureOutDir $captureOutDir -SampleFrames $sampleFrameList
    if (-not $frameSet.Complete)
    {
        $captureFailed = $true
        Write-Host "  INCOMPLETE -- missing/empty sampled frames: $($frameSet.Missing -join ', ')"
    }
}

$visualArgs = New-Object System.Collections.Generic.List[string]
$visualArgs.Add("visual")
$visualArgs.Add("--golden")
$visualArgs.Add($Reference)
$visualArgs.Add("--actual")
$visualArgs.Add($Out)
$visualArgs.Add("--frame-compare")
$visualArgs.Add($FrameCompareExe)
$visualArgs.Add("--out")
$visualArgs.Add($Out)
foreach ($id in $subsetIds)
{
    $visualArgs.Add("--capture")
    $visualArgs.Add($id)
}

& $StateCompareExe @visualArgs
$visualExit = $LASTEXITCODE

Write-Host ""
Write-Host "Report:  $(Join-Path $Out 'report.md')"

if ($visualExit -ne 0) { exit $visualExit }
if ($captureFailed) { exit 1 }
exit 0
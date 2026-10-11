#
#  run-p2-gate.ps1
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-07.
#

param(
    [string]$CaptureSet = "tests\golden\capture-set.json",
    [string]$Reference = "tests\golden\corpus",
    [string]$Out,
    [string]$Exe = "build\windows-gl-dev\samples\legacy-samples\synesthesia-vibe-demo-windows-gl\Release\synesthesia-vibe-demo-windows-gl.exe",
    [string]$StateCompareExe,
    [string]$FrameCompareExe,
    [string]$Matrix = "tests\golden\phase2-matrix.md",
    [string[]]$Subset,
    [int]$Frames = 0,
    [int]$CaptureTimeoutSeconds = 700,
    [switch]$SkipCapture,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Get-HelpText
{
    return @"
run-p2-gate.ps1 -- P2-028 Phase 2 gate runner (Windows wrapper)

USAGE

  Thin wrapper around the CMake-built tests/tools/state-compare tool (C2-020: all
  compare, tolerance, SSIM and Markdown logic lives in the C++ tool, not here).
  Runs the GL host capture set -- one fresh process per capture -- and then asks
  state-compare to compare it against the reference, write the per-preset matrix
  and write report.md.

    .\run-p2-gate.ps1 -Out <dir> [-Reference <dir>] [-CaptureSet <capture-set.json>] ``
        [-Exe <exe>] [-StateCompareExe <exe>] [-FrameCompareExe <exe>] ``
        [-Matrix <path>] [-Subset id1,id2,...] [-Frames N] [-SkipCapture]

PARAMETERS

  -CaptureSet        Path to the checked-in capture-set definition. Default:
                     tests\golden\capture-set.json (repo-root relative).
  -Reference         Directory of golden frame directories to compare against.
                     Default: tests\golden\corpus.
  -Out               Directory this run writes captures, report.md and logs into.
                     Required.
  -Exe               The Windows GL host. Default:
                     build\windows-gl-dev\samples\legacy-samples\synesthesia-vibe-demo-windows-gl\Release\synesthesia-vibe-demo-windows-gl.exe
                     (P2-031 pins this path; until then use -SkipCapture to drive
                     the gate end-to-end against the checked-in golden corpus).
  -StateCompareExe   state-compare.exe (this tool). If omitted, this script looks
                     in the usual build output locations under build\.
  -FrameCompareExe   frame-compare.exe (P0-008), the SSIM engine state-compare
                     spawns once per frame pair. If omitted, this script looks in
                     the usual build output locations under build\.
  -Matrix            Path the per-preset matrix is written to. Default:
                     tests\golden\phase2-matrix.md.
  -Subset            Capture ids to run/compare instead of the whole set.
  -Frames            Caps how many frames per capture are captured and compared
                     (0 = every frame in the capture set). Quick subset
                     verification only -- never use this to shrink an actual
                     golden-comparison run.
  -CaptureTimeoutSeconds
                     Per-attempt wall-clock budget for a single capture
                     invocation, in seconds. Default: 700. Mirrors
                     run-equivalence-gate.ps1; see its TIMEOUT / HANG HANDLING.
  -SkipCapture       Skip invoking the GL host entirely and compare whatever
                     already exists under -Out/<id>/. Used to drive the gate
                     against a checked-in capture set without paying capture time.

EXIT CODE

  0  state-compare reported every compared frame as PASS.
  1  state-compare reported a divergence (or a missing/unpaired frame), or a
     capture timed out / came back incomplete.
  2  usage error, or a required tool, capture set or reference path was absent.

  The capture process's own exit code is never the pass/fail signal (P0-007:
  a capture-mode teardown crash after every frame is written is a known issue);
  frame completeness and the compare verdict are.
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
    $blendPattern = $captureSet.blendPattern

    $captures = New-Object System.Collections.Generic.List[object]

    foreach ($preset in $captureSet.presets)
    {
        $captures.Add([pscustomobject]@{
            Id            = $preset.id
            Kind          = "single"
            Frames        = $defaultFrames
            PresetFile    = $preset.file
            BlendToFile   = $null
            BlendAtFrame  = $null
            BlendTime     = $null
            BlendPattern  = $blendPattern
        })
    }

    foreach ($blend in $captureSet.blends)
    {
        $blendFrames = if ($blend.frames) { $blend.frames } else { $defaultFrames }
        $captures.Add([pscustomobject]@{
            Id            = $blend.id
            Kind          = "blend"
            Frames        = $blendFrames
            PresetFile    = $blend.from
            BlendToFile   = $blend.to
            BlendAtFrame  = $blend.blendAtFrame
            BlendTime     = $blend.blendTimeSeconds
            BlendPattern  = $blendPattern
        })
    }

    return [pscustomobject]@{
        Wav      = $wav
        Prefix   = $prefix
        Captures = $captures
    }
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

    Get-Process -Name $ExeProcessName -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $ExePath -ArgumentList $ArgumentString -WorkingDirectory $WorkingDirectory `
        -PassThru -WindowStyle Hidden -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath

    $exited = $process.WaitForExit($TimeoutSeconds * 1000)
    $stopwatch.Stop()

    if (-not $exited)
    {
        & taskkill.exe /PID $process.Id /T /F 2>$null | Out-Null
        Get-Process -Name $ExeProcessName -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

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

    if (-not $attempt1.TimedOut)
    {
        "(stdout/stderr redirected to $stdoutPath / $stderrPath)" | Set-Content -Path $LogPath -Encoding utf8
        return [pscustomobject]@{
            ExitCode    = $attempt1.ExitCode
            WallSeconds = $attempt1.WallSeconds
            TimedOut    = $false
            Retried     = $false
        }
    }

    Write-Host "  [TIMEOUT] attempt 1 exceeded ${TimeoutSeconds}s -- retrying once"
    Start-Sleep -Seconds 5

    $attempt2 = Invoke-CaptureProcessOnce -ExePath $ExePath -ExeProcessName $exeProcessName -ArgumentString $argumentString `
        -WorkingDirectory $workingDirectory -StdoutPath $stdoutPath -StderrPath $stderrPath -TimeoutSeconds $TimeoutSeconds

    "(stdout/stderr redirected to $stdoutPath / $stderrPath)" | Set-Content -Path $LogPath -Encoding utf8
    $totalWallSeconds = [math]::Round($attempt1.WallSeconds + $attempt2.WallSeconds, 2)

    if ($attempt2.TimedOut)
    {
        Write-Host "  [TIMEOUT] attempt 2 ALSO exceeded ${TimeoutSeconds}s -- reporting TIMEOUT, continuing with remaining captures"
        return [pscustomobject]@{
            ExitCode    = $null
            WallSeconds = $totalWallSeconds
            TimedOut    = $true
            Retried     = $true
        }
    }

    Write-Host "  attempt 2 succeeded after retry, exitcode=$($attempt2.ExitCode)"
    return [pscustomobject]@{
        ExitCode    = $attempt2.ExitCode
        WallSeconds = $totalWallSeconds
        TimedOut    = $false
        Retried     = $true
    }
}

function Test-CaptureFrameSet
{
    param([string]$CaptureOutDir, [int]$FrameCount)

    $missing = New-Object System.Collections.Generic.List[string]
    for ($i = 0; $i -lt $FrameCount; $i++)
    {
        $frameName = "frame_{0:D4}" -f $i
        foreach ($ext in @("png", "json"))
        {
            $path = Join-Path $CaptureOutDir "$frameName.$ext"
            if (-not (Test-Path $path))
            {
                $missing.Add("$frameName.$ext (missing)")
            }
            elseif ((Get-Item $path).Length -eq 0)
            {
                $missing.Add("$frameName.$ext (empty)")
            }
        }
    }

    return [pscustomobject]@{
        Complete = ($missing.Count -eq 0)
        Missing  = $missing
    }
}

$RepoRoot = Get-RepoRoot

if (-not $Out) { Write-Error "Error: -Out is required"; exit 2 }
if ($Frames -lt 0) { Write-Error "Error: -Frames must be 0 (all) or a positive count"; exit 2 }
if ($CaptureTimeoutSeconds -lt 1) { Write-Error "Error: -CaptureTimeoutSeconds must be positive"; exit 2 }

$CaptureSet = Resolve-RepoPath $CaptureSet
$Reference = Resolve-RepoPath $Reference
$Out = Resolve-RepoPath $Out
$Exe = Resolve-RepoPath $Exe
$Matrix = Resolve-RepoPath $Matrix

if (-not $StateCompareExe) { $StateCompareExe = Find-BuiltExe -Name "state-compare" }
if (-not $FrameCompareExe) { $FrameCompareExe = Find-BuiltExe -Name "frame-compare" }
if ($StateCompareExe) { $StateCompareExe = Resolve-RepoPath $StateCompareExe }
if ($FrameCompareExe) { $FrameCompareExe = Resolve-RepoPath $FrameCompareExe }

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
    Write-Host "  build\x86-release\tests\tools\frame-compare\Release\frame-compare.exe"
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

$selected = $captureSetData.Captures
if ($Subset -and $Subset.Count -gt 0)
{
    $selected = $selected | Where-Object { $Subset -contains $_.Id }
    if ($selected.Count -eq 0)
    {
        Write-Host "Error: -Subset matched no capture ids"
        exit 2
    }
}

if (-not (Test-Path $Out)) { New-Item -ItemType Directory -Force -Path $Out | Out-Null }

$captureFailed = $false
$subsetIds = New-Object System.Collections.Generic.List[string]

foreach ($capture in $selected)
{
    $frameCount = if ($Frames -gt 0) { $Frames } else { $capture.Frames }
    $captureOutDir = Join-Path $Out $capture.Id
    $subsetIds.Add($capture.Id)

    Write-Host "[$($capture.Id)] frames=$frameCount kind=$($capture.Kind)"
    $timedOut = $false

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
        $processArgs.Add("$frameCount")
        $processArgs.Add("--out")
        $processArgs.Add($captureOutDir)

        if ($capture.Kind -eq "blend")
        {
            $blendToArg = "$($captureSetData.Prefix)\$($capture.BlendToFile)"
            $processArgs.Add("--blend-to")
            $processArgs.Add($blendToArg)
            $processArgs.Add("--blend-at")
            $processArgs.Add("$($capture.BlendAtFrame)")
            $processArgs.Add("--blend-time")
            $processArgs.Add("$($capture.BlendTime)")
            $processArgs.Add("--blend-pattern")
            $processArgs.Add("$($capture.BlendPattern)")
        }

        $logPath = Join-Path $captureOutDir "_process.log"
        $invokeResult = Invoke-CaptureProcess -ExePath $Exe -ProcessArgs $processArgs -LogPath $logPath `
            -TimeoutSeconds $CaptureTimeoutSeconds
        $timedOut = $invokeResult.TimedOut

        if (-not $timedOut)
        {
            Write-Host "  exitcode=$($invokeResult.ExitCode) wall=$($invokeResult.WallSeconds)s (exit code is informational only -- see KNOWN-ISSUE note)"
        }
    }
    else
    {
        if (-not (Test-Path $captureOutDir))
        {
            Write-Host "Error: -SkipCapture given but $captureOutDir does not exist"
            exit 2
        }
    }

    if ($timedOut)
    {
        $captureFailed = $true
        continue
    }

    $frameSet = Test-CaptureFrameSet -CaptureOutDir $captureOutDir -FrameCount $frameCount
    if (-not $frameSet.Complete)
    {
        $captureFailed = $true
        Write-Host "  INCOMPLETE -- missing/empty frames: $($frameSet.Missing -join ', ')"
    }
}

$reportArgs = New-Object System.Collections.Generic.List[string]
$reportArgs.Add("report")
$reportArgs.Add("--golden")
$reportArgs.Add($Reference)
$reportArgs.Add("--actual")
$reportArgs.Add($Out)
$reportArgs.Add("--frame-compare")
$reportArgs.Add($FrameCompareExe)
$reportArgs.Add("--out")
$reportArgs.Add($Out)
$reportArgs.Add("--matrix")
$reportArgs.Add($Matrix)
if ($Frames -gt 0)
{
    $reportArgs.Add("--frames")
    $reportArgs.Add("$Frames")
}
foreach ($id in $subsetIds)
{
    $reportArgs.Add("--capture")
    $reportArgs.Add($id)
}

& $StateCompareExe @reportArgs
$reportExit = $LASTEXITCODE

Write-Host ""
Write-Host "Report:  $(Join-Path $Out 'report.md')"
Write-Host "Matrix:  $Matrix"

if ($reportExit -ne 0) { exit $reportExit }
if ($captureFailed) { exit 1 }
exit 0
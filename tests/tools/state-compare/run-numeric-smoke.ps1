#
#  run-numeric-smoke.ps1
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-09.
#

param(
    [string]$CaptureSet = "tests\golden\capture-set.json",
    [string]$Reference = "tests\golden\corpus",
    [string]$Out,
    [string]$Exe = "build\windows-gl-dev\samples\legacy-samples\synesthesia-vibe-demo-windows-gl\Release\synesthesia-vibe-demo-windows-gl.exe",
    [string]$StateCompareExe,
    [string[]]$Subset,
    [int]$Frames = 30,
    [int]$CaptureTimeoutSeconds = 120
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
$Reference = Resolve-RepoPath $Reference
$Out = Resolve-RepoPath $Out
$Exe = Resolve-RepoPath $Exe

if (-not $StateCompareExe)
{
    $StateCompareExe = Join-Path $RepoRoot "build\windows-gl-dev\tests\tools\state-compare\Release\state-compare.exe"
}
$StateCompareExe = Resolve-RepoPath $StateCompareExe

foreach ($required in @($CaptureSet, $Reference, $Exe, $StateCompareExe))
{
    if (-not (Test-Path $required)) { Write-Host "Not found: $required"; exit 2 }
}

$set = Get-Content -Raw -Path $CaptureSet | ConvertFrom-Json
$wav = Join-Path $RepoRoot $set.wav
$presetRoot = Join-Path $RepoRoot (Join-Path "ref-src\beatdrop\BeatDrop" $set.presetPathPrefix)
$workingDirectory = Split-Path -Parent $Exe

$ids = New-Object System.Collections.Generic.List[string]
foreach ($preset in $set.presets)
{
    if ($Subset -and ($Subset -notcontains $preset.id)) { continue }
    $ids.Add($preset.id)

    $captureOut = Join-Path $Out $preset.id
    New-Item -ItemType Directory -Force -Path $captureOut | Out-Null

    $presetPath = Join-Path $presetRoot $preset.file
    $argString = '--capture "{0}" "{1}" --frames {2} --out "{3}"' -f $presetPath, $wav, $Frames, $captureOut
    $process = Start-Process -FilePath $Exe -ArgumentList $argString -WorkingDirectory $workingDirectory `
        -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $captureOut "_process.stdout.txt") `
        -RedirectStandardError (Join-Path $captureOut "_process.stderr.txt")

    if (-not $process.WaitForExit($CaptureTimeoutSeconds * 1000))
    {
        & taskkill.exe /PID $process.Id /T /F 2>$null | Out-Null
        Write-Host "[TIMEOUT] $($preset.id)"
    }
}

$compareArgs = @("compare", "--golden", $Reference, "--actual", $Out, "--frames", "$Frames", "--json-out", (Join-Path $Out "compare.json"))
foreach ($id in $ids) { $compareArgs += @("--capture", $id) }

& $StateCompareExe @compareArgs | Tee-Object -FilePath (Join-Path $Out "compare.txt")
exit $LASTEXITCODE

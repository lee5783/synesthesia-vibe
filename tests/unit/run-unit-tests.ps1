#
#  run-unit-tests.ps1
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-09-30.
#

param(
    [string]$BuildDir = "build\windows-gl-dev",
    [string]$Config = "Release",
    [string]$Filter,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Get-HelpText
{
    return @"
run-unit-tests.ps1 -- P2-019 synesthesia-vibe-unit-tests runner (Windows wrapper)

USAGE

  Thin wrapper around ctest (C2-020: ctest itself is the tool - no logic lives
  here). Runs the synesthesia-vibe-unit-tests suite registered by tests/unit/CMakeLists.txt
  against an already-configured/built windows-gl-dev (or macos-arm64) build tree.

    .\run-unit-tests.ps1 [-BuildDir <path>] [-Config <cfg>] [-Filter <ctest -R regex>]

PARAMETERS

  -BuildDir  Path to the CMake build directory. Default: build\windows-gl-dev.
  -Config    Build configuration passed to ctest -C. Default: Release.
  -Filter    Optional ctest -R regex to run a subset of tests.

  Build first if the tree isn't configured/built yet:

    cmake --preset windows-gl-dev
    cmake --build build\windows-gl-dev --config Release

EXIT CODE

  Forwards ctest's exit code: 0 all tests passed, non-zero otherwise.
"@
}

if ($Help)
{
    Write-Host (Get-HelpText)
    exit 0
}

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

if (-not [System.IO.Path]::IsPathRooted($BuildDir))
{
    $BuildDir = Join-Path $RepoRoot $BuildDir
}

if (-not (Test-Path $BuildDir))
{
    Write-Host "Build directory not found: $BuildDir"
    Write-Host "Configure and build it first (see -Help)."
    exit 2
}

$ctestArgs = @("--test-dir", $BuildDir, "-C", $Config, "--output-on-failure")
if ($Filter)
{
    $ctestArgs += @("-R", $Filter)
}

& ctest @ctestArgs
exit $LASTEXITCODE

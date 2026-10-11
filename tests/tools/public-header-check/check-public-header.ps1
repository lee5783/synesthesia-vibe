#
#  check-public-header.ps1
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-09-30.
#

param(
    [string]$Out,
    [string]$Dir = "include/synesthesia-vibe",
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Get-HelpText
{
    return @"
check-public-header.ps1 -- P2-015 public-header hygiene checker

USAGE

  Static source-text scan (no build required) of synesthesia-vibe's public header directory
  (include/synesthesia-vibe/) that verifies it never leaks a platform/GL header into its own text - a
  consumer building against include/synesthesia-vibe/*.h alone (no beatdrop-core, no render/gl, no
  platform TU) must never be forced to also have the Windows SDK, an OpenGL loader or the Apple
  SDK on its include path. include/synesthesia-vibe/ does not exist yet (it is created by P2-027); until
  then this check finds 0 files and trivially PASSes, the same convention
  tests/tools/dx9-confinement-check/check-dx9-confinement.ps1 uses for its own not-yet-existing
  watched directories (src/render/core/, src/render/gl/).

    .\check-public-header.ps1 -Out <dir> [-Dir <path>]

PARAMETERS

  -Out   Directory this run writes check-report.md into. Required.
  -Dir   Directory scanned, repo-root relative unless absolute. Default: include/synesthesia-vibe.

WHAT COUNTS AS A HIT

  A case-insensitive #include or #import of any of:
    - <GL/gl.h> (or any GL/*.h - the OpenGL loader/header family)
    - <GLES2/...>, <GLES3/...>, <OpenGLES/...>, <EGL/...> (OpenGL ES / EGL)
    - <Metal/...>, <MetalKit/...>
    - <jni.h>, <android/...>
    - <glad/...>
    - <windows.h>
    - <TargetConditionals.h>

  found anywhere under the scanned directory (recursively, *.h/*.hpp).

  Text after // on a line is ignored; mentions found only there are listed in the report as
  informational. /* */ block comments are not stripped.

EXIT CODE

  0   Zero forbidden-header hits under include/synesthesia-vibe/ (including the trivial "directory does
      not exist yet" case).
  1   One or more forbidden-header hits found.
"@
}

if ($Help)
{
    Write-Host (Get-HelpText)
    exit 0
}

if (-not $Out)
{
    throw "-Out is required. See -Help."
}

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$Out = (Resolve-Path $Out).Path

$PublicHeaderDir = $Dir -replace '\\', '/'

# The platform/GL headers a backend-neutral public header must never pull in (C2-001/C2-017's
# "no GL header in the public API" rule, extended to windows.h and TargetConditionals.h since a
# public header reaching either would force a platform-specific toolchain on every consumer).
$ForbiddenIncludePattern = '(?i)#\s*(?:include|import)\s*[<"](GL/[^">]+\.h|(?:GLES2|GLES3|OpenGLES|EGL|Metal|MetalKit|android|glad)/[^">]+|jni\.h|windows\.h|TargetConditionals\.h)[>"]'

function Remove-LineComment
{
    param([string]$Line)
    $idx = $Line.IndexOf("//")
    if ($idx -ge 0) { return $Line.Substring(0, $idx) }
    return $Line
}

function Get-RelativeUnixPath
{
    param([string]$FullPath)
    $rel = $FullPath
    if ($FullPath.StartsWith($RepoRoot + [System.IO.Path]::DirectorySeparatorChar))
    {
        $rel = $FullPath.Substring($RepoRoot.Length + 1)
    }
    return ($rel -replace '\\', '/')
}

if ([System.IO.Path]::IsPathRooted($PublicHeaderDir))
{
    $fullPublicHeaderDir = $PublicHeaderDir
}
else
{
    $fullPublicHeaderDir = Join-Path $RepoRoot $PublicHeaderDir
}
$publicHeaderFiles = @()
if (Test-Path $fullPublicHeaderDir)
{
    $publicHeaderFiles = @(Get-ChildItem -Path $fullPublicHeaderDir -Recurse -Include *.h,*.hpp -File)
}

$hits = New-Object System.Collections.Generic.List[object]
$commentOnlyHits = New-Object System.Collections.Generic.List[object]

foreach ($f in $publicHeaderFiles)
{
    $rel = Get-RelativeUnixPath -FullPath $f.FullName
    $lines = @(Get-Content -Path $f.FullName)
    for ($i = 0; $i -lt $lines.Count; $i++)
    {
        $code = Remove-LineComment -Line $lines[$i]
        if ($code -match $ForbiddenIncludePattern)
        {
            $hits.Add([PSCustomObject]@{
                File  = $rel
                Line  = $i + 1
                Text  = $lines[$i].Trim()
                Match = $Matches[1]
            })
        }
        elseif ($lines[$i] -match $ForbiddenIncludePattern)
        {
            $commentOnlyHits.Add([PSCustomObject]@{
                File  = $rel
                Line  = $i + 1
                Text  = $lines[$i].Trim()
                Match = $Matches[1]
            })
        }
    }
}

$pass = ($hits.Count -eq 0)

$reportPath = Join-Path $Out "check-report.md"
$reportLines = New-Object System.Collections.Generic.List[string]
$reportLines.Add("# P2-015 public-header hygiene check")
$reportLines.Add("")
if ($publicHeaderFiles.Count -eq 0)
{
    $reportLines.Add("``$PublicHeaderDir/`` does not exist yet (created by P2-027) - trivial PASS, 0 files checked.")
}
else
{
    $reportLines.Add("Static source scan (no build). Files checked under ``$PublicHeaderDir/``: $($publicHeaderFiles.Count).")
}
$reportLines.Add("")
$reportLines.Add("Overall result: **$(if ($pass) { 'PASS' } else { 'FAIL' })**")
$reportLines.Add("")
$reportLines.Add("Forbidden platform/GL header hits: $($hits.Count)")
if ($hits.Count -gt 0)
{
    $reportLines.Add("")
    foreach ($h in $hits)
    {
        $reportLines.Add("  - ``$($h.File):$($h.Line)`` (``$($h.Match)``): ``$($h.Text)``")
    }
}
$reportLines.Add("")
$reportLines.Add("Forbidden-header mentions inside // comments only (informational, not a violation): $($commentOnlyHits.Count)")
if ($commentOnlyHits.Count -gt 0)
{
    $reportLines.Add("")
    foreach ($h in $commentOnlyHits)
    {
        $reportLines.Add("  - ``$($h.File):$($h.Line)`` (``$($h.Match)``): ``$($h.Text)``")
    }
}
$reportLines | Out-File -FilePath $reportPath -Encoding utf8

Write-Host "Public-header forbidden hits: $($hits.Count)"
Write-Host "Report written to $reportPath"

if (-not $pass) { exit 1 }
exit 0

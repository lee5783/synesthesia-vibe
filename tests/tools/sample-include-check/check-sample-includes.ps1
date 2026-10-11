#
#  check-sample-includes.ps1
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-11.
#

param(
    [string]$Out,
    [string]$Dir = "samples",
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Get-HelpText
{
    return @"
check-sample-includes.ps1 -- samples-use-only-the-public-API checker

USAGE

  Static source-text scan (no build required) of samples/** that verifies every sample reaches the
  library through its public surface only. samples/legacy-samples/ is skipped (the legacy GL hosts
  predate the public API and are exempt). Mirrors check-sample-includes.sh argument for argument
  and hit for hit.

    .\check-sample-includes.ps1 -Out <dir> [-Dir <path>]

PARAMETERS

  -Out   Directory this run writes check-report.md into. Required.
  -Dir   Samples root scanned, repo-root relative unless absolute. Default: samples.
         <root>/legacy-samples is always skipped.

WHAT COUNTS AS A VIOLATION

  C, C++, Objective-C (*.h, *.hpp, *.c, *.cc, *.cpp, *.m, *.mm):
    - #include/#import <X> where X contains "..", or X is reached through a repo-internal include
      root (src/, src/<module>/, include/synesthesia-vibe/ without its prefix,
      platform/<p>/{include,src}/, vendor/<v>/{,include/}, samples/legacy-samples/shared/), unless
      X starts with synesthesia-vibe/ or SynesthesiaVibeCore/;
    - #include/#import "X" that does not resolve inside the same sample (next to the including
      file, or anywhere under samples/<platform>/<sample>/), unless X starts with
      synesthesia-vibe/ or SynesthesiaVibeCore/;
    - @import M; where M is a project module other than SynesthesiaVibe / SynesthesiaVibeCore.
  Swift (*.swift):
    - import M where M is a project module (Synesthesia*, BeatDrop*, beatdrop*) other than
      SynesthesiaVibe / SynesthesiaVibeCore. Any other module is treated as a system module.
  Kotlin / Java (*.kt, *.kts, *.java):
    - import com.synesthesiavibe.internal.*;
    - System.loadLibrary (JNI loading belongs to platform/android only).

  Text after // on a line is ignored.

EXIT CODE

  0   Zero violations (including the "no sample outside legacy-samples yet" case).
  1   One or more violations.
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

if ([System.IO.Path]::IsPathRooted($Dir))
{
    $SamplesDir = $Dir.TrimEnd('\', '/')
}
else
{
    $SamplesDir = (Join-Path $RepoRoot $Dir).TrimEnd('\', '/')
}
$LegacyDir = Join-Path $SamplesDir "legacy-samples"

function Get-DisplayPath
{
    param([string]$FullPath)
    $rel = $FullPath
    if ($FullPath.StartsWith($RepoRoot + [System.IO.Path]::DirectorySeparatorChar))
    {
        $rel = $FullPath.Substring($RepoRoot.Length + 1)
    }
    return ($rel -replace '\\', '/')
}

$InternalRoots = New-Object System.Collections.Generic.List[string]
function Add-InternalRoot
{
    param([string]$Path)
    if (Test-Path -PathType Container $Path) { $InternalRoots.Add((Resolve-Path $Path).Path) }
}
Add-InternalRoot (Join-Path $RepoRoot "src")
foreach ($parent in @("src", "vendor"))
{
    $p = Join-Path $RepoRoot $parent
    if (Test-Path $p)
    {
        foreach ($d in @(Get-ChildItem -Path $p -Directory | Sort-Object Name)) { Add-InternalRoot $d.FullName }
    }
}
$vendorRoot = Join-Path $RepoRoot "vendor"
if (Test-Path $vendorRoot)
{
    foreach ($d in @(Get-ChildItem -Path $vendorRoot -Directory | Sort-Object Name)) { Add-InternalRoot (Join-Path $d.FullName "include") }
}
$platformRoot = Join-Path $RepoRoot "platform"
if (Test-Path $platformRoot)
{
    foreach ($d in @(Get-ChildItem -Path $platformRoot -Directory | Sort-Object Name)) { Add-InternalRoot (Join-Path $d.FullName "include") }
    foreach ($d in @(Get-ChildItem -Path $platformRoot -Directory | Sort-Object Name)) { Add-InternalRoot (Join-Path $d.FullName "src") }
}
Add-InternalRoot (Join-Path $RepoRoot "include/synesthesia-vibe")
Add-InternalRoot $vendorRoot
Add-InternalRoot $platformRoot
Add-InternalRoot (Join-Path $RepoRoot "samples/legacy-samples/shared")

$CIncludePattern = '^\s*#\s*(?:include|import)\s*([<"])([^">]+)[>"]'
$ObjcModulePattern = '^\s*@import\s+([A-Za-z_][A-Za-z0-9_]*)'
$SwiftImportPattern = '^\s*(?:@[A-Za-z_]+\s+)*import\s+(?:(?:typealias|struct|class|enum|protocol|let|var|func)\s+)?([A-Za-z_][A-Za-z0-9_]*)'
$JvmInternalPattern = '^\s*import\s+(?:static\s+)?com\.synesthesiavibe\.internal(?:[.;\s]|$)'
$LoadLibraryPattern = 'System\s*\.\s*loadLibrary'

function Remove-LineComment
{
    param([string]$Line)
    $idx = $Line.IndexOf("//")
    if ($idx -ge 0) { return $Line.Substring(0, $idx) }
    return $Line
}

function Test-PublicPrefix
{
    param([string]$Name)
    return ($Name.StartsWith("synesthesia-vibe/") -or $Name.StartsWith("SynesthesiaVibeCore/"))
}

function Test-ProjectModule
{
    param([string]$Name)
    if ($Name -ceq "SynesthesiaVibe" -or $Name -ceq "SynesthesiaVibeCore") { return $false }
    return ($Name -cmatch '^(Synesthesia|synesthesia|BeatDrop|Beatdrop|beatdrop)')
}

function Get-InternalRoot
{
    param([string]$Name)
    foreach ($r in $InternalRoots)
    {
        if (Test-Path -LiteralPath (Join-Path $r $Name)) { return (Get-DisplayPath -FullPath $r) }
    }
    return $null
}

function Get-SampleRoot
{
    param([string]$FilePath)
    $rel = ($FilePath.Substring($SamplesDir.Length + 1) -replace '\\', '/')
    $parts = $rel.Split('/')
    if ($parts.Count -le 2) { return (Join-Path $SamplesDir $parts[0]) }
    return (Join-Path (Join-Path $SamplesDir $parts[0]) $parts[1])
}

function Test-ResolvesInSample
{
    param([string]$FilePath, [string]$Name)
    $dir = Split-Path -Parent $FilePath
    $root = Get-SampleRoot -FilePath $FilePath
    $candidate = Join-Path $dir $Name
    if (Test-Path -LiteralPath $candidate)
    {
        $resolved = (Resolve-Path -LiteralPath $candidate).Path
        return ($resolved.StartsWith($root + [System.IO.Path]::DirectorySeparatorChar))
    }
    if ($Name.Contains("..")) { return $false }
    $suffix = "/" + ($Name -replace '\\', '/')
    $match = Get-ChildItem -Path $root -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { ($_.FullName -replace '\\', '/').EndsWith($suffix) } |
        Select-Object -First 1
    return ($null -ne $match)
}

$legacySkipped = 0
if (Test-Path $LegacyDir)
{
    $legacySkipped = @(Get-ChildItem -Path $LegacyDir -Recurse -File).Count
}

$extensions = @(".h", ".hpp", ".c", ".cc", ".cpp", ".m", ".mm", ".swift", ".kt", ".kts", ".java")
$sampleFiles = @()
if (Test-Path $SamplesDir)
{
    $legacyPrefix = $LegacyDir + [System.IO.Path]::DirectorySeparatorChar
    $sampleFiles = @(Get-ChildItem -Path $SamplesDir -Recurse -File |
        Where-Object { -not $_.FullName.StartsWith($legacyPrefix) -and ($extensions -contains $_.Extension.ToLowerInvariant()) } |
        Sort-Object FullName)
}

$hits = New-Object System.Collections.Generic.List[object]
function Add-Hit
{
    param([string]$File, [int]$Line, [string]$Reason, [string]$Text)
    $hits.Add([PSCustomObject]@{ File = $File; Line = $Line; Reason = $Reason; Text = $Text.Trim() })
}

foreach ($f in $sampleFiles)
{
    $rel = Get-DisplayPath -FullPath $f.FullName
    $ext = $f.Extension.ToLowerInvariant()
    $kind = "c"
    if ($ext -eq ".swift") { $kind = "swift" }
    elseif (@(".kt", ".kts", ".java") -contains $ext) { $kind = "jvm" }
    $lines = @(Get-Content -Path $f.FullName)
    for ($i = 0; $i -lt $lines.Count; $i++)
    {
        $code = Remove-LineComment -Line $lines[$i]
        if ($kind -eq "c")
        {
            if ($code -match $CIncludePattern)
            {
                $delim = $Matches[1]
                $name = $Matches[2]
                if (Test-PublicPrefix -Name $name) { continue }
                if ($delim -eq "<")
                {
                    if ($name.Contains(".."))
                    {
                        Add-Hit $rel ($i + 1) "relative path in system include" $lines[$i]
                    }
                    else
                    {
                        $root = Get-InternalRoot -Name $name
                        if ($root) { Add-Hit $rel ($i + 1) "internal header (resolves under $root)" $lines[$i] }
                    }
                }
                elseif (-not (Test-ResolvesInSample -FilePath $f.FullName -Name $name))
                {
                    Add-Hit $rel ($i + 1) "quoted include outside this sample" $lines[$i]
                }
            }
            elseif ($code -match $ObjcModulePattern)
            {
                if (Test-ProjectModule -Name $Matches[1]) { Add-Hit $rel ($i + 1) "internal module" $lines[$i] }
            }
        }
        elseif ($kind -eq "swift")
        {
            if ($code -match $SwiftImportPattern)
            {
                if (Test-ProjectModule -Name $Matches[1]) { Add-Hit $rel ($i + 1) "internal module" $lines[$i] }
            }
        }
        else
        {
            if ($code -match $JvmInternalPattern) { Add-Hit $rel ($i + 1) "internal package import" $lines[$i] }
            if ($code -match $LoadLibraryPattern) { Add-Hit $rel ($i + 1) "System.loadLibrary outside platform/android" $lines[$i] }
        }
    }
}

$pass = ($hits.Count -eq 0)

$reportPath = Join-Path $Out "check-report.md"
$displayDir = Get-DisplayPath -FullPath $SamplesDir
$displayLegacy = Get-DisplayPath -FullPath $LegacyDir
$reportLines = New-Object System.Collections.Generic.List[string]
$reportLines.Add("# Sample include check (public API only)")
$reportLines.Add("")
$reportLines.Add("Static source scan (no build). Files checked under ``$displayDir/``: $($sampleFiles.Count).")
$reportLines.Add("")
$reportLines.Add("legacy-samples skipped: ``$displayLegacy/`` ($legacySkipped files, exempt).")
$reportLines.Add("")
$reportLines.Add("Overall result: **$(if ($pass) { 'PASS' } else { 'FAIL' })**")
$reportLines.Add("")
$reportLines.Add("Violations: $($hits.Count)")
if ($hits.Count -gt 0)
{
    $reportLines.Add("")
    foreach ($h in $hits)
    {
        $reportLines.Add("  - ``$($h.File):$($h.Line): $($h.Reason): $($h.Text)``")
    }
}
$reportLines | Out-File -FilePath $reportPath -Encoding utf8

Write-Host "Sample files checked: $($sampleFiles.Count)"
Write-Host "legacy-samples skipped ($legacySkipped files)"
Write-Host "Sample include violations: $($hits.Count)"
Write-Host "Report written to $reportPath"

if (-not $pass) { exit 1 }
exit 0

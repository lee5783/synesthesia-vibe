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
  JavaScript under <root>/web (*.js, *.mjs, and inline <script> blocks in *.html / *.htm other
  than type=importmap / JSON):
    - an import specifier (import ... from, export ... from, import 'X', import('X')) other
      than the bare specifier synesthesia-vibe or a relative path (./, ../) that resolves
      inside <root>/web;
    - a specifier containing synesthesia-vibe-wasm, platform/web or /js/ is always a violation;
    - any use of createSynesthesiaVibeModule (the raw Emscripten factory is not public API);
    - <script src="X"> in HTML where X is not a relative path resolving inside <root>/web.

  Text after // on a line is ignored (for JavaScript only when // starts the line or follows
  whitespace or ;, so URLs inside strings are kept). <!-- --> and /* */ are not stripped.

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
$JsSpecifierPattern = '(?:^|[^A-Za-z0-9_$.])(?:from|import)\s*\(?\s*[''"`]([^''"`]+)[''"`]'
$JsFactoryPattern = 'createSynesthesiaVibeModule'
$HtmlScriptOpenPattern = '<script([^>]*)>'
$HtmlScriptSrcPattern = '\ssrc\s*=\s*[''"]?([^''"\s>]+)'
$HtmlScriptDataTypePattern = 'type\s*=\s*[''"]?[^''"\s>]*(?:importmap|json)'
$WebDir = Join-Path $SamplesDir "web"

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

function Test-WebRelativeResolves
{
    param([string]$FilePath, [string]$Name)
    $target = Join-Path (Split-Path -Parent $FilePath) $Name
    if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { return $false }
    $targetDir = Split-Path -Parent (Resolve-Path -LiteralPath $target).Path
    return ($targetDir -eq $WebDir -or $targetDir.StartsWith($WebDir + [System.IO.Path]::DirectorySeparatorChar))
}

function Test-JsSpecifier
{
    param([string]$Rel, [int]$LineNumber, [string]$FilePath, [string]$Spec, [string]$Text)
    if ($Spec -ceq "synesthesia-vibe") { return }
    if ($Spec.Contains("synesthesia-vibe-wasm")) { Add-Hit $Rel $LineNumber "raw wasm module import" $Text }
    elseif ($Spec.Contains("platform/web")) { Add-Hit $Rel $LineNumber "platform/web internal import" $Text }
    elseif ($Spec.Contains("/js/")) { Add-Hit $Rel $LineNumber "library internals import (/js/)" $Text }
    elseif ($Spec.StartsWith("./") -or $Spec.StartsWith("../"))
    {
        if (-not (Test-WebRelativeResolves -FilePath $FilePath -Name $Spec)) { Add-Hit $Rel $LineNumber "relative import outside this sample" $Text }
    }
    else { Add-Hit $Rel $LineNumber "import other than synesthesia-vibe or a file inside this sample" $Text }
}

function Remove-JsLineComment
{
    param([string]$Line)
    return ($Line -replace '(?:^\s*|[\s;])//.*$', '')
}

function Test-JsCode
{
    param([string]$Rel, [int]$LineNumber, [string]$FilePath, [string]$Code, [string]$Text)
    foreach ($m in [regex]::Matches($Code, $JsSpecifierPattern))
    {
        Test-JsSpecifier $Rel $LineNumber $FilePath $m.Groups[1].Value $Text
    }
    if ($Code -cmatch $JsFactoryPattern) { Add-Hit $Rel $LineNumber "createSynesthesiaVibeModule outside the public API" $Text }
}

function Test-ScriptSrc
{
    param([string]$Rel, [int]$LineNumber, [string]$FilePath, [string]$Src, [string]$Text)
    if ($Src.Contains("synesthesia-vibe-wasm") -or $Src.Contains("platform/web") -or $Src.Contains("/js/"))
    {
        Add-Hit $Rel $LineNumber "script src reaches library internals" $Text
    }
    elseif ($Src.StartsWith("/") -or $Src.Contains(":") -or -not (Test-WebRelativeResolves -FilePath $FilePath -Name $Src))
    {
        Add-Hit $Rel $LineNumber "script src outside this sample" $Text
    }
}

function Test-JsFile
{
    param([string]$FilePath, [string]$Rel)
    $lines = @(Get-Content -Path $FilePath)
    for ($i = 0; $i -lt $lines.Count; $i++)
    {
        Test-JsCode $Rel ($i + 1) $FilePath (Remove-JsLineComment -Line $lines[$i]) $lines[$i]
    }
}

function Test-HtmlFile
{
    param([string]$FilePath, [string]$Rel)
    $lines = @(Get-Content -Path $FilePath)
    $inScript = $false
    for ($i = 0; $i -lt $lines.Count; $i++)
    {
        $line = $lines[$i]
        $code = ""
        if ($inScript)
        {
            $code = $line
        }
        elseif ($line -cmatch $HtmlScriptOpenPattern)
        {
            $attrs = $Matches[1]
            $openTag = $Matches[0]
            if ((" " + $attrs) -cmatch $HtmlScriptSrcPattern)
            {
                Test-ScriptSrc $Rel ($i + 1) $FilePath $Matches[1] $line
            }
            elseif (-not ($attrs -cmatch $HtmlScriptDataTypePattern))
            {
                $inScript = $true
                $code = $line.Substring($line.IndexOf($openTag) + $openTag.Length)
            }
        }
        if ($inScript)
        {
            $closeIdx = $code.IndexOf("</script")
            if ($closeIdx -ge 0)
            {
                $code = $code.Substring(0, $closeIdx)
                $inScript = $false
            }
            Test-JsCode $Rel ($i + 1) $FilePath (Remove-JsLineComment -Line $code) $line
        }
    }
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
$webExtensions = @(".js", ".mjs", ".html", ".htm")
$webFiles = @()
if (Test-Path $WebDir)
{
    $webFiles = @(Get-ChildItem -Path $WebDir -Recurse -File |
        Where-Object { $webExtensions -contains $_.Extension.ToLowerInvariant() } |
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

foreach ($f in $webFiles)
{
    $rel = Get-DisplayPath -FullPath $f.FullName
    $ext = $f.Extension.ToLowerInvariant()
    if ($ext -eq ".html" -or $ext -eq ".htm") { Test-HtmlFile -FilePath $f.FullName -Rel $rel }
    else { Test-JsFile -FilePath $f.FullName -Rel $rel }
}

$fileCount = $sampleFiles.Count + $webFiles.Count
$pass = ($hits.Count -eq 0)

$reportPath = Join-Path $Out "check-report.md"
$displayDir = Get-DisplayPath -FullPath $SamplesDir
$displayLegacy = Get-DisplayPath -FullPath $LegacyDir
$reportLines = New-Object System.Collections.Generic.List[string]
$reportLines.Add("# Sample include check (public API only)")
$reportLines.Add("")
$reportLines.Add("Static source scan (no build). Files checked under ``$displayDir/``: $fileCount.")
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

Write-Host "Sample files checked: $fileCount"
Write-Host "legacy-samples skipped ($legacySkipped files)"
Write-Host "Sample include violations: $($hits.Count)"
Write-Host "Report written to $reportPath"

if (-not $pass) { exit 1 }
exit 0

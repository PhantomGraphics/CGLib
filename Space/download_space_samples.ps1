#Requires -Version 5.1
<#
.SYNOPSIS
  Space サンプルデータ 生成
  合成点群を CGLib\Space\samples\ に生成する（実データのダウンロードは行わない -- Stanford Bunny/Armadillo
  は SpaceView のどのシナリオからも参照されていなかった死んだ設定だったため撤去した、
  docs/todo/PLAN_scenario_test_synthetic_assets.md Phase 6）。
  ソース定義: CGLib\Space\download_space_samples.json
#>
$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'

$outputDir = Join-Path $PSScriptRoot 'samples'
$config    = Get-Content -Path (Join-Path $PSScriptRoot 'download_space_samples.json') -Raw | ConvertFrom-Json
$samples   = @($config.samples)

# ---- 合成データ ----
function Get-Gauss([Random]$rng, [double]$sigma) {
    $u = [Math]::Max($rng.NextDouble(), 1e-15)
    $sigma * [Math]::Sqrt(-2 * [Math]::Log($u)) * [Math]::Cos(2 * [Math]::PI * $rng.NextDouble())
}

function Write-PlyAscii([string]$path, $points) {
    $sw = [IO.StreamWriter]::new($path, $false, [Text.Encoding]::ASCII)
    $sw.NewLine = "`n"
    $sw.WriteLine('ply'); $sw.WriteLine('format ascii 1.0')
    $sw.WriteLine("element vertex $($points.Count)")
    $sw.WriteLine('property float x'); $sw.WriteLine('property float y'); $sw.WriteLine('property float z')
    $sw.WriteLine('end_header')
    foreach ($p in $points) { $sw.WriteLine(('{0:F6} {1:F6} {2:F6}' -f $p[0], $p[1], $p[2])) }
    $sw.Close()
}

function New-SpherePoints([double]$radius, [int]$nPoints) {
    $rng = [Random]::new()
    $pts = [Collections.Generic.List[double[]]]::new()
    for ($i = 0; $i -lt $nPoints; $i++) {
        $theta = [Math]::Acos(1 - 2 * $rng.NextDouble())
        $phi   = 2 * [Math]::PI * $rng.NextDouble()
        $pts.Add([double[]]@(($radius * [Math]::Sin($theta) * [Math]::Cos($phi)),
                   ($radius * [Math]::Sin($theta) * [Math]::Sin($phi)),
                   ($radius * [Math]::Cos($theta))))
    }
    ,$pts
}

function New-PlanePoints([double]$size, [int]$nPoints, [double]$noise) {
    $rng = [Random]::new()
    $pts = [Collections.Generic.List[double[]]]::new()
    for ($i = 0; $i -lt $nPoints; $i++) {
        $pts.Add([double[]]@(($size * ($rng.NextDouble() - 0.5)),
                   ($size * ($rng.NextDouble() - 0.5)),
                   (Get-Gauss $rng $noise)))
    }
    ,$pts
}

# ---- メイン ----
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

Write-Host ('=' * 60)
Write-Host 'Space sample data generation'
Write-Host "Output: $outputDir"
Write-Host ('=' * 60)

$results = [ordered]@{}
foreach ($s in $samples) {
    Write-Host "`n[$($s.name)]  $($s.desc)"

    $outPath = Join-Path $outputDir $s.output
    if (Test-Path $outPath) {
        Write-Host "  Already exists: $($s.output) -- skipping"
        $results[$s.name] = 'CACHED'; continue
    }

    $p   = $s.params
    $pts = switch ($s.generator) {
        'sphere' { New-SpherePoints $p.radius $p.nPoints }
        'plane'  { New-PlanePoints  $p.size   $p.nPoints $p.noise }
    }
    Write-Host "  Generated $($pts.Count) points"
    Write-Host "  Writing: $($s.output) ..." -NoNewline
    Write-PlyAscii $outPath $pts
    Write-Host (' {0:N0} KB' -f ((Get-Item $outPath).Length / 1KB))
    $results[$s.name] = 'OK'
}

Write-Host "`n$('=' * 60)"
Write-Host 'Summary'
Write-Host ('=' * 60)
foreach ($kv in $results.GetEnumerator()) {
    $icon = @{ OK='OK  '; CACHED='CACHE' }[$kv.Value]
    Write-Host "  [$icon] $($kv.Key)"
}

Write-Host "`nDone. Output: $outputDir"

# run_gltf_scenarios.ps1 - Run all GltfViewer scenario tests
# Usage: .\CGLib\GltfViewer\run_gltf_scenarios.ps1 [-Configuration Debug|Release]
# Run from the repository root or the CGLib\GltfViewer directory.
#
# Scenarios that include a "skip_if_missing" field are skipped (not failed) when
# the referenced file does not exist.  Run .\CGLib\GltfViewer\download_gltf_samples.ps1
# to download the PBR models required for render scenarios.

param(
    [string]$Configuration = "Debug"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = $scriptDir
while ($repoRoot -and -not (Test-Path (Join-Path $repoRoot "Phantom2026.sln"))) {
    $parent = Split-Path -Parent $repoRoot
    if ($parent -eq $repoRoot) { $repoRoot = $null; break }
    $repoRoot = $parent
}
if (-not $repoRoot) {
    Write-Host "ERROR: Could not locate repository root (Phantom2026.sln)"
    exit 1
}
$preset    = "windows-$($Configuration.ToLower())"
$exe       = Join-Path $repoRoot "build\$preset\CGLib\GltfRenderer\GltfViewer.exe"
$scenDir   = Join-Path $scriptDir "scenarios"

if (-not (Test-Path $exe)) {
    Write-Host "ERROR: Executable not found: $exe"
    exit 1
}

$scenarios = Get-ChildItem "$scenDir\*.json" | Sort-Object Name
if ($scenarios.Count -eq 0) {
    Write-Host "ERROR: No scenario JSON files found in $scenDir"
    exit 1
}

# Ensure screenshots output directory exists
$screenshotsDir = Join-Path $scenDir "screenshots"
New-Item -ItemType Directory -Force $screenshotsDir | Out-Null

$passed  = 0
$failed  = 0
$skipped = 0

foreach ($s in $scenarios) {
    # Check skip_if_missing before launching the exe
    $json = Get-Content $s.FullName -Raw | ConvertFrom-Json
    if ($json.skip_if_missing) {
        $required = Join-Path $scriptDir $json.skip_if_missing
        if (-not (Test-Path $required)) {
            Write-Host "SKIP: $($s.BaseName) (required file missing — run .\CGLib\GltfViewer\download_gltf_samples.ps1)"
            $skipped++
            continue
        }
    }

    Write-Host "Running: $($s.BaseName)"
    $proc = Start-Process -FilePath $exe `
        -ArgumentList "--run-scenario `"$($s.FullName)`"" `
        -PassThru -Wait -WorkingDirectory $scriptDir
    if ($proc.ExitCode -eq 0) {
        Write-Host "PASSED: $($s.BaseName)"
        $passed++
    } else {
        Write-Host "FAILED: $($s.BaseName) (exit $($proc.ExitCode))"
        $failed++
    }
}

Write-Host ""
Write-Host "Results: $passed/$($scenarios.Count) PASSED, $failed FAILED, $skipped SKIPPED"
exit $failed

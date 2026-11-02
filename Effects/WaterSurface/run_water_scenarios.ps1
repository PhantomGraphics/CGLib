param([ValidateSet('Debug','Release')][string]$Configuration='Debug')
$ErrorActionPreference='Stop'
$cglibRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$binary=Join-Path $cglibRoot "build/windows-$($Configuration.ToLowerInvariant())/Effects/WaterSurfaceView.exe"
if (!(Test-Path -LiteralPath $binary)) { throw "Build WaterSurfaceView first: $binary" }
# SpvResolver uses the working directory on non-Windows platforms.
Push-Location (Split-Path $binary -Parent)
try {
    foreach ($scenario in Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'scenarios') -Filter '*.json') {
        & $binary --run-scenario $scenario.FullName
        if ($LASTEXITCODE -ne 0) { throw "Scenario failed: $($scenario.Name)" }
    }
} finally { Pop-Location }

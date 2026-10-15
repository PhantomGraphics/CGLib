param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$ExePath,
    [ValidateRange(1, 600)][int]$TimeoutSeconds = 60
)
$ErrorActionPreference = 'Stop'
$scriptDir = $PSScriptRoot
if (-not $ExePath) {
    $sourceDir = Split-Path -Parent $scriptDir
    $preset = 'windows-' + $Configuration.ToLowerInvariant()
    for ($root = $sourceDir; $root; $root = Split-Path -Parent $root) {
        if (-not (Test-Path (Join-Path $root 'CMakePresets.json'))) { continue }
        $relative = $sourceDir.Substring($root.Length).TrimStart('\')
        $candidate = Join-Path $root "build\$preset\$relative\PBRView.exe"
        if (Test-Path -LiteralPath $candidate) { $ExePath = $candidate; break }
    }
}
if (-not $ExePath -or -not (Test-Path -LiteralPath $ExePath)) {
    throw 'PBRView.exe not found. Build PBRView first, or supply -ExePath.'
}
$ExePath = (Resolve-Path -LiteralPath $ExePath).Path
$scenarios = @(Get-ChildItem (Join-Path $scriptDir 'scenarios') -Filter '*.json' | Sort-Object Name)
if ($scenarios.Count -eq 0) { throw 'No PBRView scenarios found.' }
$failed = 0
$logDir = Join-Path ([IO.Path]::GetTempPath()) ('PBRView-scenarios-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $logDir | Out-Null
Write-Host "Logs: $logDir"
foreach ($scenario in $scenarios) {
    Write-Host "Running $($scenario.Name)"
    $process = Start-Process -FilePath $ExePath -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $logDir ($scenario.BaseName + '.stdout.log')) `
        -RedirectStandardError (Join-Path $logDir ($scenario.BaseName + '.stderr.log')) `
        -WorkingDirectory (Split-Path -Parent $ExePath) `
        -ArgumentList "--run-scenario `"$($scenario.FullName)`""
    # Retain the OS handle so Windows PowerShell can retrieve ExitCode after exit.
    $processHandle = $process.Handle
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill()
        $process.WaitForExit()
        Write-Host "FAILED: $($scenario.Name) (timeout)"
        $failed++
    } else {
        $process.WaitForExit()
        $process.Refresh()
        if ($process.ExitCode -eq 0) { Write-Host "PASSED: $($scenario.Name)" }
        else {
            Write-Host "FAILED: $($scenario.Name) (exit $($process.ExitCode))"
            Get-Content (Join-Path $logDir ($scenario.BaseName + '.stderr.log'))
            $failed++
        }
    }
    $process.Dispose()
}
Write-Host "Results: $($scenarios.Count - $failed)/$($scenarios.Count) PASSED, $failed FAILED"
if ($failed) { exit 1 }
exit 0

<#
.SYNOPSIS
  Install CGLib (CPU components), MOVE the install prefix, then build and run an external
  consumer that only uses find_package(CGLib). Proves the package does not depend on the
  source tree or the build tree (forwarding headers hold absolute source paths).

.EXAMPLE
  .\tests\run_install_consumer.ps1 -Configuration Debug
#>
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$WorkDir = (Join-Path $env:TEMP 'cglib_install_check'),
    # Build/install with Vulkan ON and also consume the exported VulkanGraphics component.
    [switch]$Vulkan
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# VS-bundled cmake + vcvars (same cmake as Phantom builds, see repository CLAUDE.md).
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
& "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -SkipAutomaticLocation | Out-Null
$cmake = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if (Test-Path $WorkDir) { Remove-Item -Recurse -Force $WorkDir }
New-Item -ItemType Directory $WorkDir | Out-Null
$build = Join-Path $WorkDir 'build'
$prefix = Join-Path $WorkDir 'prefix'
$moved = Join-Path $WorkDir 'moved_prefix'
$cons = Join-Path $WorkDir 'consumer_build'

& $cmake -S $root -B $build -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" "-DCGLIB_ENABLE_VULKAN=$(if ($Vulkan) { 'ON' } else { 'OFF' })" -DCGLIB_BUILD_TESTING=OFF -DCGLIB_BUILD_VIEWERS=OFF
if ($LASTEXITCODE) { throw 'configure failed' }
& $cmake --build $build
if ($LASTEXITCODE) { throw 'build failed' }
& $cmake --install $build --prefix $prefix
if ($LASTEXITCODE) { throw 'install failed' }

# The install tree must not mention the source or build directory.
$leaks = Get-ChildItem -Recurse $prefix -Include *.cmake | Select-String -SimpleMatch -Pattern $root, $build
if ($leaks) { $leaks | ForEach-Object { Write-Host $_ }; throw 'install tree references the source/build tree' }

# Relocate the prefix and hide the original build tree so nothing can fall back to it.
Move-Item $prefix $moved
Remove-Item -Recurse -Force $build

& $cmake -S (Join-Path $root 'tests\install_consumer') -B $cons -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_PREFIX_PATH=$moved" "-DCGLIB_CONSUMER_VULKAN=$(if ($Vulkan) { 'ON' } else { 'OFF' })"
if ($LASTEXITCODE) { throw 'consumer configure failed' }
& $cmake --build $cons -- -k 0
if ($LASTEXITCODE) { throw 'consumer build failed' }
& (Join-Path $cons 'cglib_install_consumer.exe')
if ($LASTEXITCODE) { throw 'consumer run failed' }
Write-Host 'install consumer check: PASS'

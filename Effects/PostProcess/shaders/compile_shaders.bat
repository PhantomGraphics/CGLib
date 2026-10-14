@echo off
setlocal enabledelayedexpansion

set GLSLC="%VULKAN_SDK%\Bin\glslc.exe"
if not exist %GLSLC% (
    echo [ERROR] glslc.exe not found. VULKAN_SDK=%VULKAN_SDK%
    exit /b 1
)

set OUTDIR=%~dp0

for %%f in ("%OUTDIR%*.vert") do (
    %GLSLC% -fshader-stage=vert "%%f" -o "%%f.spv"
    if errorlevel 1 ( echo FAILED: %%f & exit /b 1 )
)

for %%f in ("%OUTDIR%*.frag") do (
    %GLSLC% -fshader-stage=frag "%%f" -o "%%f.spv"
    if errorlevel 1 ( echo FAILED: %%f & exit /b 1 )
)

echo All shaders compiled successfully.
endlocal

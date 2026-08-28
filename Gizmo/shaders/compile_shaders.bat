@echo off
setlocal

set GLSLC="%VULKAN_SDK%\Bin\glslc.exe"
if not exist %GLSLC% (
    echo [ERROR] glslc.exe not found. VULKAN_SDK=%VULKAN_SDK%
    exit /b 1
)

set OUTDIR=%~dp0

%GLSLC% -fshader-stage=vert "%OUTDIR%gizmo.vert" -o "%OUTDIR%gizmo.vert.spv"
if errorlevel 1 ( echo FAILED: gizmo.vert & exit /b 1 )

%GLSLC% -fshader-stage=frag "%OUTDIR%gizmo.frag" -o "%OUTDIR%gizmo.frag.spv"
if errorlevel 1 ( echo FAILED: gizmo.frag & exit /b 1 )

echo All shaders compiled successfully.
endlocal

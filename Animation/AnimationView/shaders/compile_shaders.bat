@echo off
setlocal

set GLSLC="%VULKAN_SDK%\Bin\glslc.exe"
if not exist %GLSLC% (
    echo [ERROR] glslc.exe not found. VULKAN_SDK=%VULKAN_SDK%
    exit /b 1
)

set OUTDIR=%~dp0

%GLSLC% -fshader-stage=vert "%OUTDIR%gltf.vert" -o "%OUTDIR%gltf.vert.spv"
if errorlevel 1 ( echo FAILED: gltf.vert & exit /b 1 )

%GLSLC% -fshader-stage=frag "%OUTDIR%gltf.frag" -o "%OUTDIR%gltf.frag.spv"
if errorlevel 1 ( echo FAILED: gltf.frag & exit /b 1 )

echo All shaders compiled successfully.
endlocal

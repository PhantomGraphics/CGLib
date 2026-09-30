@echo off
setlocal

set GLSLC="%VULKAN_SDK%\Bin\glslc.exe"
if not exist %GLSLC% (
    echo [ERROR] glslc.exe not found. VULKAN_SDK=%VULKAN_SDK%
    exit /b 1
)

set OUTDIR=%~dp0

echo Compiling volume raymarch shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%volume_fullscreen.vert" -o "%OUTDIR%volume_fullscreen.vert.spv"
if errorlevel 1 ( echo FAILED: volume_fullscreen.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%volume_raymarch.frag" -o "%OUTDIR%volume_raymarch.frag.spv"
if errorlevel 1 ( echo FAILED: volume_raymarch.frag & exit /b 1 )
%GLSLC% -fshader-stage=comp "%OUTDIR%volume_sun_transmittance.comp" -o "%OUTDIR%volume_sun_transmittance.comp.spv"
if errorlevel 1 ( echo FAILED: volume_sun_transmittance.comp & exit /b 1 )

echo Done.
endlocal

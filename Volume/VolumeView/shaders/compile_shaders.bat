@echo off
setlocal

set GLSLC="%VULKAN_SDK%\Bin\glslc.exe"
if not exist %GLSLC% (
    echo [ERROR] glslc.exe not found. VULKAN_SDK=%VULKAN_SDK%
    exit /b 1
)

set OUTDIR=%~dp0

echo Compiling volume point shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%volume_point.vert" -o "%OUTDIR%volume_point.vert.spv"
if errorlevel 1 ( echo FAILED: volume_point.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%volume_point.frag" -o "%OUTDIR%volume_point.frag.spv"
if errorlevel 1 ( echo FAILED: volume_point.frag & exit /b 1 )

echo Compiling volume line shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%volume_line.vert" -o "%OUTDIR%volume_line.vert.spv"
if errorlevel 1 ( echo FAILED: volume_line.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%volume_line.frag" -o "%OUTDIR%volume_line.frag.spv"
if errorlevel 1 ( echo FAILED: volume_line.frag & exit /b 1 )

echo Compiling PBVR shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%pbvr_render.vert" -o "%OUTDIR%pbvr_render.vert.spv"
if errorlevel 1 ( echo FAILED: pbvr_render.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%pbvr_render.frag" -o "%OUTDIR%pbvr_render.frag.spv"
if errorlevel 1 ( echo FAILED: pbvr_render.frag & exit /b 1 )

echo Compiling triangle shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%triangle.vert" -o "%OUTDIR%triangle.vert.spv"
if errorlevel 1 ( echo FAILED: triangle.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%triangle.frag" -o "%OUTDIR%triangle.frag.spv"
if errorlevel 1 ( echo FAILED: triangle.frag & exit /b 1 )

echo Compiling PBVR GPU compute shader...
%GLSLC% -fshader-stage=comp "%OUTDIR%volume_pbvr_gen.comp" -o "%OUTDIR%volume_pbvr_gen.comp.spv"
if errorlevel 1 ( echo FAILED: volume_pbvr_gen.comp & exit /b 1 )

echo Compiling opacity shadow deposit shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%opacity_shadow_deposit.vert" -o "%OUTDIR%opacity_shadow_deposit.vert.spv"
if errorlevel 1 ( echo FAILED: opacity_shadow_deposit.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%opacity_shadow_deposit.frag" -o "%OUTDIR%opacity_shadow_deposit.frag.spv"
if errorlevel 1 ( echo FAILED: opacity_shadow_deposit.frag & exit /b 1 )

echo Done.
endlocal

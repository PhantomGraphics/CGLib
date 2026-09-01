@echo off
rem ============================================================
rem  CGLib/Renderer/VkRenderer/Shaders/compile_shaders.bat
rem
rem  Compile all VkRenderer GLSL shaders to SPIR-V using glslc.
rem  Requires the VULKAN_SDK environment variable to be set
rem  (installed automatically by the Vulkan SDK installer).
rem
rem  Output files (written next to this script):
rem    point.vert.spv / point.frag.spv
rem    line.vert.spv  / line.frag.spv
rem    triangle.vert.spv / triangle.frag.spv
rem    tex.vert.spv   / tex.frag.spv
rem    skybox.vert.spv / skybox.frag.spv
rem
rem  Run manually or register as a Visual Studio pre-build event.
rem ============================================================

setlocal

set GLSLC="%VULKAN_SDK%\Bin\glslc.exe"

if not exist %GLSLC% (
    echo [ERROR] glslc.exe not found. VULKAN_SDK=%VULKAN_SDK%
    exit /b 1
)

set OUTDIR=%~dp0

echo Compiling point shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%point.vert" -o "%OUTDIR%point.vert.spv"
if errorlevel 1 ( echo FAILED: point.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%point.frag" -o "%OUTDIR%point.frag.spv"
if errorlevel 1 ( echo FAILED: point.frag & exit /b 1 )

echo Compiling line shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%line.vert"  -o "%OUTDIR%line.vert.spv"
if errorlevel 1 ( echo FAILED: line.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%line.frag"  -o "%OUTDIR%line.frag.spv"
if errorlevel 1 ( echo FAILED: line.frag & exit /b 1 )

echo Compiling triangle shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%triangle.vert" -o "%OUTDIR%triangle.vert.spv"
if errorlevel 1 ( echo FAILED: triangle.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%triangle.frag" -o "%OUTDIR%triangle.frag.spv"
if errorlevel 1 ( echo FAILED: triangle.frag & exit /b 1 )

echo Compiling tex shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%tex.vert" -o "%OUTDIR%tex.vert.spv"
if errorlevel 1 ( echo FAILED: tex.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%tex.frag" -o "%OUTDIR%tex.frag.spv"
if errorlevel 1 ( echo FAILED: tex.frag & exit /b 1 )

echo Compiling skybox shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%skybox.vert" -o "%OUTDIR%skybox.vert.spv"
if errorlevel 1 ( echo FAILED: skybox.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%skybox.frag" -o "%OUTDIR%skybox.frag.spv"
if errorlevel 1 ( echo FAILED: skybox.frag & exit /b 1 )

echo All shaders compiled successfully.
endlocal

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

for %%f in ("%OUTDIR%*.comp") do (
    %GLSLC% -fshader-stage=compute "%%f" -o "%%f.spv"
    if errorlevel 1 ( echo FAILED: %%f & exit /b 1 )
)

%GLSLC% "%OUTDIR%scene.frag" -DCLIP_MODE=1 -o "%OUTDIR%scene_reflection.frag.spv"
if errorlevel 1 exit /b 1
%GLSLC% "%OUTDIR%scene.frag" -DCLIP_MODE=2 -o "%OUTDIR%scene_refraction.frag.spv"
if errorlevel 1 exit /b 1
echo All shaders compiled successfully.
endlocal

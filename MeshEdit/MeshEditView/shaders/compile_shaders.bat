@echo off
setlocal

set GLSLC="%VULKAN_SDK%\Bin\glslc.exe"
if not exist %GLSLC% (
    echo [ERROR] glslc.exe not found. VULKAN_SDK=%VULKAN_SDK%
    exit /b 1
)

set OUTDIR=%~dp0

for %%S in (point line triangle) do (
    %GLSLC% -fshader-stage=vert "%OUTDIR%%%S.vert" -o "%OUTDIR%%%S.vert.spv"
    if errorlevel 1 ( echo FAILED: %%S.vert & exit /b 1 )
    %GLSLC% -fshader-stage=frag "%OUTDIR%%%S.frag" -o "%OUTDIR%%%S.frag.spv"
    if errorlevel 1 ( echo FAILED: %%S.frag & exit /b 1 )
)

echo All shaders compiled successfully.
endlocal

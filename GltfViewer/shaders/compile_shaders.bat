@echo off
setlocal

set GLSLC="%VULKAN_SDK%\Bin\glslc.exe"
if not exist %GLSLC% (
    echo [ERROR] glslc.exe not found. VULKAN_SDK=%VULKAN_SDK%
    exit /b 1
)

set OUTDIR=%~dp0

echo Compiling glTF shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%gltf.vert" -o "%OUTDIR%gltf.vert.spv"
if errorlevel 1 ( echo FAILED: gltf.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%gltf.frag" -o "%OUTDIR%gltf.frag.spv"
if errorlevel 1 ( echo FAILED: gltf.frag & exit /b 1 )

echo Compiling skybox shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%skybox.vert" -o "%OUTDIR%skybox.vert.spv"
if errorlevel 1 ( echo FAILED: skybox.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%skybox.frag" -o "%OUTDIR%skybox.frag.spv"
if errorlevel 1 ( echo FAILED: skybox.frag & exit /b 1 )

echo Compiling shadow shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%shadow.vert" -o "%OUTDIR%shadow.vert.spv"
if errorlevel 1 ( echo FAILED: shadow.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%shadow.frag" -o "%OUTDIR%shadow.frag.spv"
if errorlevel 1 ( echo FAILED: shadow.frag & exit /b 1 )

echo Compiling IBL shaders...
%GLSLC% -fshader-stage=vert "%OUTDIR%irradiance.vert" -o "%OUTDIR%irradiance.vert.spv"
if errorlevel 1 ( echo FAILED: irradiance.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%irradiance.frag" -o "%OUTDIR%irradiance.frag.spv"
if errorlevel 1 ( echo FAILED: irradiance.frag & exit /b 1 )
%GLSLC% -fshader-stage=vert "%OUTDIR%prefilter.vert" -o "%OUTDIR%prefilter.vert.spv"
if errorlevel 1 ( echo FAILED: prefilter.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%prefilter.frag" -o "%OUTDIR%prefilter.frag.spv"
if errorlevel 1 ( echo FAILED: prefilter.frag & exit /b 1 )
%GLSLC% -fshader-stage=vert "%OUTDIR%brdf_lut.vert" -o "%OUTDIR%brdf_lut.vert.spv"
if errorlevel 1 ( echo FAILED: brdf_lut.vert & exit /b 1 )
%GLSLC% -fshader-stage=frag "%OUTDIR%brdf_lut.frag" -o "%OUTDIR%brdf_lut.frag.spv"
if errorlevel 1 ( echo FAILED: brdf_lut.frag & exit /b 1 )

echo Done.
endlocal

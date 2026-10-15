@echo off
setlocal
if not defined VULKAN_SDK (
    echo VULKAN_SDK must be set.
    exit /b 1
)
"%VULKAN_SDK%\Bin\glslc.exe" "%~dp0pbr.frag" -o "%~dp0pbr.frag.spv"
exit /b %ERRORLEVEL%

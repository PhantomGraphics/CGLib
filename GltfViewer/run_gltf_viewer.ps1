param(
    [ValidateSet(
        'DamagedHelmet','WaterBottle','NormalTangentTest','MetalRoughSpheres','MetalRoughSpheresNoTextures',
        'BoomBox','Lantern','BoxTextured',
        'AntiqueCamera','Corset','Avocado','Duck','NormalTangentMirrorTest',
        'TextureCoordinateTest','MultiUVTest','OrientationTest','NegativeScaleTest',
        'RiggedSimple','RiggedFigure','CesiumMan',
        'BoxVertexColors','AlphaBlendModeTest','EmissiveStrengthTest','UnlitTest')]
    [string]$Model = 'DamagedHelmet',

    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir '..\..')

$exePath = Join-Path $repoRoot ("x64\$Configuration\GltfViewer.exe")

# Matches the output layout of download_gltf_samples.ps1: samples\<Name>\<Name>.glb
$modelPath = Join-Path $scriptDir "samples\$Model\$Model.glb"

if (!(Test-Path $exePath)) {
    throw "Executable not found: $exePath`nBuild first (x64 $Configuration)."
}

if (!(Test-Path $modelPath)) {
    throw "Model not found: $modelPath`nRun .\CGLib\GltfViewer\download_gltf_samples.ps1 -All first."
}

Write-Host "Launching: $exePath"
Write-Host "Model   : $modelPath"

Start-Process -FilePath $exePath -ArgumentList @($modelPath)

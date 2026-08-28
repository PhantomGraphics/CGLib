# VulkanMemoryAllocator (VMA) v3 ヘッダをダウンロードするスクリプト
# 実行方法: .\download_vma.ps1

$url  = "https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/master/include/vk_mem_alloc.h"
$dest = Join-Path $PSScriptRoot "vk_mem_alloc.h"

Write-Host "Downloading vk_mem_alloc.h ..."
Invoke-WebRequest -Uri $url -OutFile $dest
Write-Host "Saved to: $dest"

#include "ImageFileWriter.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ThirdParty/stb/stb_image_write.h"

#include <algorithm>
#include <cctype>

using namespace Phantom::Graphics;

bool ImageFileWriter::write(const std::string& filepath, const Graphics::Imageuc& image)
{
	// 基本情報
	this->width = image.getWidth();
	this->height = image.getHeight();
	this->bpp = 4; // Imageuc は RGBA として扱う

	if (width == 0 || height == 0) return false;

	// 値をコピーして内部バッファに保存
	this->data = image.getValues();
	if (data.empty()) return false;

	// 拡張子判定（小文字化）
	std::filesystem::path p(filepath);
	auto ext = p.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

	int result = 0;

	if (ext == ".png" || ext == ".png\r") {
		// stride = 横ピクセル数 * チャンネル数
		result = stbi_write_png(filepath.c_str(), width, height, bpp, data.data(), width * bpp);
	}
	else if (ext == ".jpg" || ext == ".jpeg") {
		// JPEG はアルファ無効のため RGB バッファに変換
		std::vector<unsigned char> rgb;
		rgb.resize(width * height * 3);
		const int pixels = width * height;
		for (int i = 0; i < pixels; ++i) {
			const int src = i * 4;
			const int dst = i * 3;
			rgb[dst + 0] = data[src + 0];
			rgb[dst + 1] = data[src + 1];
			rgb[dst + 2] = data[src + 2];
		}
		const int quality = 100; // max quality
		result = stbi_write_jpg(filepath.c_str(), width, height, 3, rgb.data(), quality);
	}
	else if (ext == ".bmp") {
		result = stbi_write_bmp(filepath.c_str(), width, height, bpp, data.data());
	}
	else if (ext == ".tga") {
		result = stbi_write_tga(filepath.c_str(), width, height, bpp, data.data());
	}
	else {
		// デフォルトは PNG
		result = stbi_write_png(filepath.c_str(), width, height, bpp, data.data(), width * bpp);
	}

	return result != 0;
}
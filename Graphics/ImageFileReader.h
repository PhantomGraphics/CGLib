#pragma once

#include <string>
#include "Image.h"

namespace Phantom {
	namespace Graphics {

/// @brief 画像ファイルを8bit RGBA画像として読み込むクラスです。
class ImageFileReader
{
public:
	/// @brief 画像ファイルを読み込みます。
	/// @param filepath 画像ファイルパス。
	/// @return 読み込み成功時はtrue。
	bool read(const std::string& filepath);

	/// @brief 読み込み結果を`Imageuc`として取得します。
	/// @return 8bit画像。
	Graphics::Imageuc toImage() const;

private:
	int width;
	int height;
	int bpp;
	std::vector<unsigned char> data;
};

/// @brief HDR画像ファイルをfloat RGBA画像として読み込むクラスです。
class HDRImageFileReader
{
public:
	/// @brief HDR画像ファイルを読み込みます。
	/// @param filepath HDR画像ファイルパス。
	/// @return 読み込み成功時はtrue。
	bool read(const std::string& filepath);

	/// @brief 読み込み結果を`Imagef`として取得します。
	/// @return float画像。
	Graphics::Imagef toImage() const;

private:
	int width;
	int height;
	int bpp;
	std::vector<float> data;

};
	}
}
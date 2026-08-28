#pragma once

#include <string>
#include "Image.h"
#include <filesystem>

namespace Phantom {
	namespace Graphics {

		/// @brief 8bit RGBA画像をファイルへ書き出すクラスです。
		class ImageFileWriter
		{
		public:
			/// @brief 画像ファイルを書き出します。
			/// @param filepath 出力先ファイルパス。
			/// @param image 書き出す画像。
			/// @return 書き出し成功時はtrue。
			bool write(const std::string& filepath, const Graphics::Imageuc& image);

			//Graphics::Imageuc toImage() const;

		private:
			int width;
			int height;
			int bpp;
			std::vector<unsigned char> data;
		};

		/*
		class HDRImageFileReader
		{
		public:
			bool read(const std::string& filepath);

			Graphics::Imagef toImage() const;

		private:
			int width;
			int height;
			int bpp;
			std::vector<float> data;

		};
		*/
	}
}
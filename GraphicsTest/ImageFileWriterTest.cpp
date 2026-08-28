#include "pch.h"
#include "gtest/gtest.h"

#include "../Graphics/ImageFileWriter.h"
#include "../Graphics/Image.h"

#include <filesystem>
#include <string>

using namespace Phantom::Graphics;

static Imageuc createTestImage(const int w, const int h)
{
	Imageuc img(w, h);
	// シンプルなパターン：左上赤、右上緑、左下青、右下白
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			ColorRGBAuc c;
			if (x == 0 && y == 0) { c = ColorRGBAuc(255, 0, 0, 255); }   // red
			else if (x == 1 && y == 0) { c = ColorRGBAuc(0, 255, 0, 255); } // green
			else if (x == 0 && y == 1) { c = ColorRGBAuc(0, 0, 255, 255); } // blue
			else { c = ColorRGBAuc(255, 255, 255, 255); }                   // white
			img.setColor(x, y, c);
		}
	}
	return img;
}

TEST(ImageFileWriterTest, WritePNG)
{
	const auto img = createTestImage(2, 2);
	ImageFileWriter writer;
	const std::string path = "ImageFileWriterTest_out.png";

	EXPECT_TRUE(writer.write(path, img));
	EXPECT_TRUE(std::filesystem::exists(path));
	std::filesystem::remove(path);
}

TEST(ImageFileWriterTest, WriteJPG)
{
	const auto img = createTestImage(2, 2);
	ImageFileWriter writer;
	const std::string path = "ImageFileWriterTest_out.jpg";

	EXPECT_TRUE(writer.write(path, img));
	EXPECT_TRUE(std::filesystem::exists(path));
	std::filesystem::remove(path);
}

TEST(ImageFileWriterTest, WriteBMP)
{
	const auto img = createTestImage(2, 2);
	ImageFileWriter writer;
	const std::string path = "ImageFileWriterTest_out.bmp";

	EXPECT_TRUE(writer.write(path, img));
	EXPECT_TRUE(std::filesystem::exists(path));
	std::filesystem::remove(path);
}

TEST(ImageFileWriterTest, WriteTGA)
{
	const auto img = createTestImage(2, 2);
	ImageFileWriter writer;
	const std::string path = "ImageFileWriterTest_out.tga";

	EXPECT_TRUE(writer.write(path, img));
	EXPECT_TRUE(std::filesystem::exists(path));
	std::filesystem::remove(path);
}

TEST(ImageFileWriterTest, WriteDefaultWhenNoExtension)
{
	const auto img = createTestImage(2, 2);
	ImageFileWriter writer;
	const std::string path = "ImageFileWriterTest_out_noext"; // 拡張子無し → デフォルトでPNG扱い

	EXPECT_TRUE(writer.write(path, img));
	EXPECT_TRUE(std::filesystem::exists(path));
	std::filesystem::remove(path);
}

TEST(ImageFileWriterTest, FailOnZeroSizeImage)
{
	Imageuc img(0, 0);
	ImageFileWriter writer;
	const std::string path = "ImageFileWriterTest_fail.png";

	EXPECT_FALSE(writer.write(path, img));
	// ファイルは作成されないはず
	EXPECT_FALSE(std::filesystem::exists(path));
}